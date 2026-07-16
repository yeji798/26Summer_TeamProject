/*=====================================================================
  <vehicle_manager.c>
  : 차량 입차/출차/검색/조회 + 동적 배열 관리

  ---------------------------------------------------------------------

  [기능 설명]
  - 차량 입차 처리 (사용자 입력 + 스마트 공간 추천 + 배정)
  - 차량 출차 처리 (요금 계산 + 공간 반납 + 동적 메모리 해제)
  - 차량번호로 검색
  - 전체 주차 차량 조회
  - Vehicle 포인터를 담는 동적 배열(realloc 기반)의 추가/삭제/해제
=====================================================================*/
#include "parking.h"

#define INIT_CAPACITY 4

/*---------------------------------------------------------------------
  동적 배열에 차량 추가 (필요 시 realloc으로 용량 확장)
---------------------------------------------------------------------*/
void add_vehicle_to_system(ParkingSystem *sys, Vehicle *v)
{
    if (sys->vehicles == NULL) {
        sys->capacity = INIT_CAPACITY;
        sys->vehicles = (Vehicle **)malloc(sizeof(Vehicle *) * sys->capacity);
        if (!sys->vehicles) {
            printf("[오류] 메모리 할당에 실패했습니다.\n");
            exit(1);
        }
    }

    if (sys->count >= sys->capacity) {
        sys->capacity *= 2;
        Vehicle **tmp = (Vehicle **)realloc(sys->vehicles, sizeof(Vehicle *) * sys->capacity);
        if (!tmp) {
            printf("[오류] 메모리 재할당에 실패했습니다.\n");
            exit(1);
        }
        sys->vehicles = tmp;
    }

    sys->vehicles[sys->count] = v;
    sys->count++;
}

/* 차량번호로 배열 인덱스 검색 (없으면 -1) */
int find_vehicle_index_by_plate(const ParkingSystem *sys, const char *plate)
{
    for (int i = 0; i < sys->count; i++) {
        if (strcmp(sys->vehicles[i]->plate, plate) == 0) return i;
    }
    return -1;
}

/* 지정 인덱스의 차량을 해제하고 배열에서 제거(뒤 요소들을 앞으로 당김) */
void remove_vehicle_from_system(ParkingSystem *sys, int index)
{
    if (index < 0 || index >= sys->count) return;

    free(sys->vehicles[index]);   /* 동적 메모리 해제 (메모리 누수 방지) */

    /* 아래 for문에서 Visual Studio 코드 분석기가 sys->vehicles를
       "초기화되지 않은 메모리"로 오탐(false positive)하는 경우가 있다.
       실제로는 add_vehicle_to_system()에서 항상 먼저 할당되므로 안전하다. */
#ifdef _MSC_VER
#pragma warning(suppress: 6001)
#endif
    for (int i = index; i < sys->count - 1; i++) {
        sys->vehicles[i] = sys->vehicles[i + 1];
    }
    sys->count--;
}

/* 프로그램 종료 시 모든 차량 메모리 해제 */
void free_all_vehicles(ParkingSystem *sys)
{
    for (int i = 0; i < sys->count; i++) {
        free(sys->vehicles[i]);
    }
    free(sys->vehicles);
    sys->vehicles = NULL;
    sys->count = 0;
    sys->capacity = 0;
}

/*---------------------------------------------------------------------
  차량 입차 처리
---------------------------------------------------------------------*/
void vehicle_entry(ParkingSystem *sys)
{
    char plate[PLATE_LEN];

    printf("\n===== 차량 입차 =====\n");
    read_line("차량번호를 입력하세요: ", plate, sizeof(plate));

    if (find_vehicle_index_by_plate(sys, plate) != -1) {
        printf("[오류] 이미 주차장에 있는 차량번호입니다.\n");
        return;
    }

    printf("차종을 선택하세요\n");
    printf("1. 일반차량  2. 전기차  3. 경차  4. 장애인차량  5. 임산부차량\n");
    int car_choice = read_int("선택: ", 1, 5);
    CarType car_type = (CarType)(car_choice - 1);

    /* 입차 시간 입력 */
    time_t entry_time;
    printf("입차시간 입력 방식을 선택하세요\n");
    printf("1. 현재 시간으로 입력   2. 직접 입차시간 입력\n");
    int time_choice = read_int("선택: ", 1, 2);

    if (time_choice == 1) {
        entry_time = time(NULL);
    } else {
        int y, mo, d, h, mi;
        y  = read_int("연도 (예: 2026): ", 1970, 2100);
        mo = read_int("월 (1~12): ", 1, 12);
        d  = read_int("일 (1~31): ", 1, 31);
        h  = read_int("시 (0~23): ", 0, 23);
        mi = read_int("분 (0~59): ", 0, 59);

        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_year = y - 1900;
        t.tm_mon  = mo - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min  = mi;
        t.tm_sec  = 0;
        t.tm_isdst = -1;
        entry_time = mktime(&t);
    }

    /* 할인 적용 여부 및 종류 선택*/
    printf("국가유공자이신가요?\n");
    printf("0. 아니오   1. 예\n");
    int veteran_choice = read_int("선택: ", 0, 1);

    DiscountType discount;
    if (veteran_choice == 1) {
        discount = DISCOUNT_VETERAN;
    } else {
        switch (car_type) {
            case CAR_DISABLED: discount = DISCOUNT_DISABLED; break;
            case CAR_ELECTRIC: discount = DISCOUNT_ELECTRIC; break;
            case CAR_COMPACT:  discount = DISCOUNT_COMPACT;  break;
            default:            discount = DISCOUNT_NONE;     break;
        }
    }

    /* 스마트 주차공간 추천 */
    int score = 0;
    char reason[256];
    int spot_idx = recommend_spot(sys, car_type, &score, reason, sizeof(reason));

    if (spot_idx == -1) {
        printf("\n[안내] 현재 배정 가능한 빈 주차공간이 없습니다. 입차를 취소합니다.\n");
        return;
    }

    printf("\n추천 주차공간 : %s\n", sys->spots[spot_idx].location);
    printf("\n추천 이유\n%s\n", reason);

    /* 추천된 위치가 해당 구역 배치도의 어디에 있는지 하트(♥)로 강조하여 보여준다 */
    printf("\n[%s구역 배치도]\n", zone_name(sys->spots[spot_idx].zone));
    print_zone_map_highlight(sys, sys->spots[spot_idx].zone, sys->spots[spot_idx].location);
    printf("(♥ = 배정된 위치, ■ = 사용중, □ = 빈자리, △ = 입구)\n");

    /* 차량 구조체 동적 할당 */
    Vehicle *v = (Vehicle *)malloc(sizeof(Vehicle));
    if (!v) {
        printf("[오류] 메모리 할당 실패\n");
        return;
    }
    strncpy(v->plate, plate, PLATE_LEN - 1);
    v->plate[PLATE_LEN - 1] = '\0';
    v->car_type = car_type;
    v->discount = discount;
    v->entry_time = entry_time;
    strncpy(v->location, sys->spots[spot_idx].location, LOC_LEN - 1);
    v->location[LOC_LEN - 1] = '\0';
    v->is_settled = 0;
    v->recommend_score = score;

    /* 주차공간 배정 처리 */
    sys->spots[spot_idx].occupied = 1;
    strncpy(sys->spots[spot_idx].vehicle_plate, plate, PLATE_LEN - 1);
    sys->spots[spot_idx].vehicle_plate[PLATE_LEN - 1] = '\0';

    add_vehicle_to_system(sys, v);
    record_entry_time_slot(sys, entry_time);

    printf("\n[완료] 차량 [%s] 이(가) %s 위치에 배정되었습니다.\n", plate, v->location);
}

/*---------------------------------------------------------------------
  차량 출차 처리
  차량번호 입력 -> 검색 -> 정산여부 확인 -> 요금계산 -> 출차 -> 공간반납 -> 메모리 삭제
---------------------------------------------------------------------*/
void vehicle_exit(ParkingSystem *sys)
{
    char plate[PLATE_LEN];
    printf("\n===== 차량 출차 =====\n");
    read_line("차량번호를 입력하세요: ", plate, sizeof(plate));

    int idx = find_vehicle_index_by_plate(sys, plate);
    if (idx == -1) {
        printf("[오류] 해당 차량번호를 찾을 수 없습니다.\n");
        return;
    }

    Vehicle *v = sys->vehicles[idx];
    time_t now = time(NULL);
    double fee = calculate_fee(v, now);

    /* 출차 처리 전, 차량 정보와 예상 요금을 먼저 보여준다 */
    printf("\n----- 출차 정보 -----\n");
    printf("차량번호       : %s\n", v->plate);
    printf("차종           : %s\n", car_type_name(v->car_type));
    printf("주차 위치      : %s\n", v->location);
    printf("입차 시간      : ");
    print_time(v->entry_time);
    printf("예상 출차시간  : ");
    print_time(now);
    printf("할인 적용      : %s\n", discount_name(v->discount));
    printf("최종 주차요금  : %.0f원\n", fee);

    /* 출차 여부를 한번 더 확인 */
    printf("\n차량을 출차하시겠습니까?\n");
    printf("0. 아니오   1. 예\n");
    int confirm = read_int("선택: ", 0, 1);
    if (confirm == 0) {
        printf("\n[안내] 출차를 취소하였습니다.\n");
        return;
    }

    if (v->is_settled) {
        /* 이미 요금 정산이 완료된 차량이라면 추가 요금 없이 바로 출차 처리 */
        printf("[안내] 이미 요금이 정산된 차량입니다. 추가 요금 없이 출차 처리합니다.\n");
    } else {
        /* 아직 정산되지 않은 차량이라면, 정산 여부를 다시 한번 확인한다 */
        printf("\n요금 정산이 되지 않았습니다. 지금 정산하시겠습니까?\n");
        printf("0. 아니오   1. 예\n");
        int pay_confirm = read_int("선택: ", 0, 1);

        if (pay_confirm == 0) {
            printf("\n[안내] 요금을 정산하지 않아 출차를 진행하지 않습니다.\n");
            return;
        }

        /* 정산 시점 기준으로 요금을 다시 계산하여 확정한다 */
        time_t settle_time = time(NULL);
        double settle_fee = calculate_fee(v, settle_time);
        double settle_minutes = difftime(settle_time, v->entry_time) / 60.0;

        record_exit_stats(sys, v->entry_time, settle_time, settle_fee, settle_minutes);
        v->is_settled = 1;

        printf("\n[완료] 요금 %.0f원이 정산되었습니다.\n", settle_fee);
    }

    /* 주차 공간 반납 */
    int spot_idx = find_spot_index_by_location(sys, v->location);
    if (spot_idx != -1) {
        sys->spots[spot_idx].occupied = 0;
        sys->spots[spot_idx].vehicle_plate[0] = '\0';
    }

    /* 동적 메모리에서 삭제 */
    remove_vehicle_from_system(sys, idx);

    printf("\n[완료] 차량 [%s] 출차 처리되었습니다.\n", plate);
}

/*---------------------------------------------------------------------
  차량 검색 (메뉴 3번)
---------------------------------------------------------------------*/
void vehicle_search(ParkingSystem *sys)
{
    char plate[PLATE_LEN];
    printf("\n===== 차량 검색 =====\n");
    read_line("검색할 차량번호를 입력하세요: ", plate, sizeof(plate));

    int idx = find_vehicle_index_by_plate(sys, plate);
    if (idx == -1) {
        printf("[안내] 해당 차량번호를 찾을 수 없습니다.\n");
        return;
    }

    Vehicle *v = sys->vehicles[idx];
    time_t now = time(NULL);
    double fee = calculate_fee(v, now);

    printf("\n----- 검색 결과 -----\n");
    printf("차량번호           : %s\n", v->plate);
    printf("차종               : %s\n", car_type_name(v->car_type));
    printf("주차 위치          : %s\n", v->location);
    printf("입차 시간          : ");
    print_time(v->entry_time);
    printf("현재까지 예상요금  : %.0f원\n", fee);
    printf("할인 적용 여부     : %s\n", discount_name(v->discount));
}

/*---------------------------------------------------------------------
  전체 차량 조회 (메뉴 4번)
---------------------------------------------------------------------*/
void vehicle_list_all(const ParkingSystem *sys)
{
    printf("\n===== 전체 주차 차량 조회 (총 %d대) =====\n", sys->count);
    if (sys->count == 0) {
        printf("현재 주차된 차량이 없습니다.\n");
        return;
    }

    time_t now = time(NULL);
    printf("%-10s %-10s %-6s %-17s %-8s %-10s\n",
           "차량번호", "차종", "위치", "입차시간", "할인", "예상요금");
    printf("--------------------------------------------------------------------------\n");

    for (int i = 0; i < sys->count; i++) {
        Vehicle *v = sys->vehicles[i];
        double fee = calculate_fee(v, now);
        struct tm *lt = localtime(&v->entry_time);
        char timebuf[32];
        /* 날짜(YYYY-MM-DD)까지 함께 표시 */
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d %02d:%02d",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min);

        printf("%-10s %-10s %-6s %-17s %-8s %8.0f원\n",
               v->plate, car_type_name(v->car_type), v->location,
               timebuf, discount_name(v->discount), fee);
    }
}
