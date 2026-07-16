/*=====================================================================
  file_io.c
  ---------------------------------------------------------------------
  [기능 설명]
  - 현재 주차 중인 차량 목록 + 통계용 입/출차 기록(전체 이력) +
    구역별 배치 설정(공간 개수/가로 길이/입구 좌표)을 parking.txt
    파일에 저장(save_to_file)
  - 프로그램 시작 시(또는 관리자모드 "파일 저장" 이후 재실행 시)
    parking.txt 파일로부터 데이터를 읽어와 시스템 상태를 복원
    (load_from_file)
  - 구역별 배치 설정(NUM_*, *_WIDTH, *_ENTRANCE_ROW/COL)은 parking.h의
    매크로 값이므로 재컴파일 전에는 바뀌지 않지만, 혹시 파일을 저장한
    이후 매크로 값을 바꿔서 재컴파일했다면 저장된 위치 정보("A1", "B3"
    등)가 현재 배치와 어긋날 수 있다. 이를 감지("동기화 확인")하기
    위해 저장 당시의 배치 설정을 함께 기록해두고, 불러올 때 현재
    프로그램의 배치 설정과 비교하여 다르면 경고 메시지를 출력한다.
  - 차량번호는 "12우 1234"처럼 중간에 공백이 포함될 수 있는데,
    파일은 공백으로 각 값을 구분하는 형식이라 그대로 저장하면
    fscanf("%s", ...)가 공백에서 끊겨 값이 어긋난다. 이를 막기 위해
    저장할 때는 차량번호의 공백을 밑줄(_)로 바꿔서 기록하고(encode_plate),
    불러올 때는 다시 밑줄을 공백으로 되돌린다(decode_plate).
  - 파일 형식(모두 텍스트, 공백으로 구분)
      [1번째 줄] 구역 개수(항상 5: 일반/전기차/경차/장애인/임산부)
      [5개 줄]   구역번호 공간개수 가로길이 입구행 입구열
      [다음 줄]  입차기록 수(N1)
      [N1개 줄]  입차시각(long)
      [다음 줄]  출차기록 수(N2)
      [N2개 줄]  입차시각(long) 출차시각(long) 요금 주차시간(분)
      [다음 줄]  현재 주차 중인 차량 수(N3)
      [N3개 줄]  차량번호(공백은 _로 치환됨) 차종번호 할인번호 입차시각(long) 위치 추천점수
=====================================================================*/
#include "parking.h"

/* 저장/비교 대상이 되는 구역 목록 (parking.h의 ZoneType 순서와 동일) */
static const ZoneType ALL_ZONES[ZONE_COUNT] = {
    ZONE_NORMAL, ZONE_ELECTRIC, ZONE_COMPACT, ZONE_DISABLED, ZONE_PREGNANT
};

/*---------------------------------------------------------------------
  차량번호 문자열의 공백(' ')을 밑줄('_')로 치환하여 dst에 저장
  (파일에 공백으로 값을 구분해 저장하므로, 차량번호 안의 공백이
   구분자로 오인되지 않도록 저장 직전에만 임시로 바꿔준다)
---------------------------------------------------------------------*/
static void encode_plate(const char *src, char *dst, size_t dst_size)
{
    size_t i = 0;
    for (; i < dst_size - 1 && src[i] != '\0'; i++) {
        dst[i] = (src[i] == ' ') ? '_' : src[i];
    }
    dst[i] = '\0';
}

/* 밑줄('_')로 치환되어 저장되어 있던 문자를 다시 공백(' ')으로 되돌림(제자리 변환) */
static void decode_plate(char *plate)
{
    for (int i = 0; plate[i] != '\0'; i++) {
        if (plate[i] == '_') plate[i] = ' ';
    }
}

/*---------------------------------------------------------------------
  파일 저장 (관리자모드 "파일 저장" 메뉴 / 프로그램 종료 시 자동 호출)
---------------------------------------------------------------------*/
void save_to_file(const ParkingSystem *sys)
{
    FILE *fp = fopen(FILE_NAME, "w");
    if (!fp) {
        printf("[오류] 파일을 저장할 수 없습니다: %s\n", FILE_NAME);
        return;
    }

    /* 구역별 배치 설정 저장 (공간개수/가로길이/입구좌표) - 동기화 확인용 */
    fprintf(fp, "%d\n", ZONE_COUNT);
    for (int z = 0; z < ZONE_COUNT; z++) {
        int count, width, entrance_row, entrance_col;
        get_zone_layout(ALL_ZONES[z], &count, &width, &entrance_row, &entrance_col);
        fprintf(fp, "%d %d %d %d %d\n", (int)ALL_ZONES[z], count, width, entrance_row, entrance_col);
    }

    /* 입차 기록 저장 (통계용) */
    fprintf(fp, "%d\n", sys->entry_record_count);
    for (int i = 0; i < sys->entry_record_count; i++) {
        fprintf(fp, "%lld\n", (long long)sys->entry_records[i].entry_time);
    }

    /* 출차(정산) 기록 저장 (통계용) */
    fprintf(fp, "%d\n", sys->exit_record_count);
    for (int i = 0; i < sys->exit_record_count; i++) {
        const ExitRecord *r = &sys->exit_records[i];
        fprintf(fp, "%lld %lld %.2f %.2f\n",
                (long long)r->entry_time, (long long)r->exit_time, r->fee, r->parked_minutes);
    }

    /* 현재 주차 중인 차량 목록 저장 */
    fprintf(fp, "%d\n", sys->count);
    for (int i = 0; i < sys->count; i++) {
        Vehicle *v = sys->vehicles[i];
        char plate_enc[PLATE_LEN];
        encode_plate(v->plate, plate_enc, sizeof(plate_enc)); /* 공백 -> '_' */
        fprintf(fp, "%s %d %d %lld %s %d\n",
                plate_enc,
                (int)v->car_type,
                (int)v->discount,
                (long long)v->entry_time,
                v->location,
                v->recommend_score);
    }

    fclose(fp);
    printf("\n[완료] 데이터가 %s 파일에 저장되었습니다.\n", FILE_NAME);
}

/*---------------------------------------------------------------------
  파일 불러오기 (프로그램 시작 시 자동 호출)
---------------------------------------------------------------------*/
void load_from_file(ParkingSystem *sys)
{
    FILE *fp = fopen(FILE_NAME, "r");
    if (!fp) {
        /* 파일이 없으면(최초 실행) 조용히 넘어간다 */
        return;
    }

    /* 기존에 남아있던 차량/통계 데이터가 있다면 모두 정리 후 새로 로드 */
    free_all_vehicles(sys);
    free_stats_records(sys);
    for (int i = 0; i < TOTAL_SPOTS; i++) {
        sys->spots[i].occupied = 0;
        sys->spots[i].vehicle_plate[0] = '\0';
    }

    /* 구역별 배치 설정 불러오기 및 현재 설정과 비교(동기화 확인) */
    int zone_count = 0;
    if (fscanf(fp, "%d", &zone_count) != 1) {
        printf("[오류] 파일 형식이 올바르지 않습니다.\n");
        fclose(fp);
        return;
    }

    int layout_mismatch = 0;
    for (int i = 0; i < zone_count; i++) {
        int zone_i, saved_count, saved_width, saved_row, saved_col;
        int matched = fscanf(fp, "%d %d %d %d %d", &zone_i, &saved_count, &saved_width, &saved_row, &saved_col);
        if (matched != 5) break;

        if (zone_i >= 0 && zone_i < ZONE_COUNT) {
            int cur_count, cur_width, cur_row, cur_col;
            get_zone_layout((ZoneType)zone_i, &cur_count, &cur_width, &cur_row, &cur_col);
            if (cur_count != saved_count || cur_width != saved_width ||
                cur_row != saved_row || cur_col != saved_col) {
                printf("[경고] '%s구역'의 배치 설정이 저장 당시와 다릅니다. "
                       "(저장됨: 공간 %d개, 가로 %d칸, 입구(%d,%d) / 현재: 공간 %d개, 가로 %d칸, 입구(%d,%d))\n",
                       zone_name((ZoneType)zone_i), saved_count, saved_width, saved_row, saved_col,
                       cur_count, cur_width, cur_row, cur_col);
                layout_mismatch = 1;
            }
        }
    }
    if (layout_mismatch) {
        printf("[안내] 구역 배치 설정이 변경되어 일부 차량의 주차 위치가 "
               "현재 배치도와 맞지 않을 수 있습니다.\n");
    }

    /* 입차 기록 불러오기 */
    int entry_record_count = 0;
    if (fscanf(fp, "%d", &entry_record_count) != 1) {
        printf("[오류] 파일 형식이 올바르지 않습니다.\n");
        fclose(fp);
        return;
    }
    for (int i = 0; i < entry_record_count; i++) {
        long long et;
        if (fscanf(fp, "%lld", &et) != 1) break;
        record_entry_time_slot(sys, (time_t)et);
    }

    /* 출차(정산) 기록 불러오기 */
    int exit_record_count = 0;
    if (fscanf(fp, "%d", &exit_record_count) != 1) {
        fclose(fp);
        return;
    }
    for (int i = 0; i < exit_record_count; i++) {
        long long et, xt;
        double fee, minutes;
        int matched = fscanf(fp, "%lld %lld %lf %lf", &et, &xt, &fee, &minutes);
        if (matched != 4) break;
        record_exit_stats(sys, (time_t)et, (time_t)xt, fee, minutes);
    }

    /* 현재 주차 중인 차량 목록 불러오기 */
    int vehicle_count = 0;
    if (fscanf(fp, "%d", &vehicle_count) != 1) {
        fclose(fp);
        return;
    }

    for (int i = 0; i < vehicle_count; i++) {
        char plate[PLATE_LEN], location[LOC_LEN];
        int car_type_i, discount_i, score;
        long long entry_time_ll;

        int matched = fscanf(fp, "%19s %d %d %lld %9s %d",
                              plate, &car_type_i, &discount_i, &entry_time_ll, location, &score);
        if (matched != 6) break;

        Vehicle *v = (Vehicle *)malloc(sizeof(Vehicle));
        if (!v) {
            printf("[오류] 메모리 할당 실패(불러오기 중단)\n");
            break;
        }
        strncpy(v->plate, plate, PLATE_LEN - 1);
        v->plate[PLATE_LEN - 1] = '\0';
        decode_plate(v->plate); /* '_' -> 공백 복원 (예: "12우_1234" -> "12우 1234") */
        v->car_type = (CarType)car_type_i;
        v->discount = (DiscountType)discount_i;
        v->entry_time = (time_t)entry_time_ll;
        strncpy(v->location, location, LOC_LEN - 1);
        v->location[LOC_LEN - 1] = '\0';
        v->is_settled = 0;
        v->recommend_score = score;

        add_vehicle_to_system(sys, v);

        /* 주차 공간 점유 상태 복원 */
        int spot_idx = find_spot_index_by_location(sys, v->location);
        if (spot_idx != -1) {
            sys->spots[spot_idx].occupied = 1;
            strncpy(sys->spots[spot_idx].vehicle_plate, v->plate, PLATE_LEN - 1);
            sys->spots[spot_idx].vehicle_plate[PLATE_LEN - 1] = '\0';
        }
    }

    fclose(fp);
    printf("\n[완료] %s 파일에서 데이터를 불러왔습니다. (주차 중 %d대)\n", FILE_NAME, sys->count);
}
