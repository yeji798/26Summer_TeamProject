/*=====================================================================
  <stats.c>
  : 통계 기록 및 조회
  ---------------------------------------------------------------------
  [기능 설명]
  - 차량 입차/출차가 발생할 때마다 EntryRecord/ExitRecord를 동적 배열
    (realloc 기반)에 하나씩 추가하여 기록한다.
  - 관리자모드 "통계 조회" 메뉴에서 1) 오늘자 통계, 2) 한달(이번 달)
    통계 중 하나를 선택하면, 저장된 기록 중 해당 기간에 속하는 것만
    걸러서 총 매출, 입차 차량 수, 출차 차량 수, 평균 주차시간,
    시간대별(오전/오후/야간) 이용률을 계산하여 출력한다.
=====================================================================*/
#include "parking.h"

#define INIT_RECORD_CAPACITY 4

/*---------------------------------------------------------------------
  입차 기록 추가 (동적 배열, 필요 시 realloc으로 확장)
---------------------------------------------------------------------*/
void record_entry_time_slot(ParkingSystem *sys, time_t entry_time)
{
    if (sys->entry_records == NULL) {
        sys->entry_record_capacity = INIT_RECORD_CAPACITY;
        sys->entry_records = (EntryRecord *)malloc(sizeof(EntryRecord) * sys->entry_record_capacity);
        if (!sys->entry_records) {
            printf("[오류] 메모리 할당에 실패했습니다.\n");
            exit(1);
        }
    }

    if (sys->entry_record_count >= sys->entry_record_capacity) {
        sys->entry_record_capacity *= 2;
        EntryRecord *tmp = (EntryRecord *)realloc(sys->entry_records, sizeof(EntryRecord) * sys->entry_record_capacity);
        if (!tmp) {
            printf("[오류] 메모리 재할당에 실패했습니다.\n");
            exit(1);
        }
        sys->entry_records = tmp;
    }

    sys->entry_records[sys->entry_record_count].entry_time = entry_time;
    sys->entry_record_count++;
}

/*---------------------------------------------------------------------
  출차(정산) 기록 추가 (동적 배열, 필요 시 realloc으로 확장)
  main.c 및 vehicle_manager.c 의 vehicle_exit 에서 호출
---------------------------------------------------------------------*/
void record_exit_stats(ParkingSystem *sys, time_t entry_time, time_t exit_time, double fee, double parked_minutes)
{
    if (sys->exit_records == NULL) {
        sys->exit_record_capacity = INIT_RECORD_CAPACITY;
        sys->exit_records = (ExitRecord *)malloc(sizeof(ExitRecord) * sys->exit_record_capacity);
        if (!sys->exit_records) {
            printf("[오류] 메모리 할당에 실패했습니다.\n");
            exit(1);
        }
    }

    if (sys->exit_record_count >= sys->exit_record_capacity) {
        sys->exit_record_capacity *= 2;
        ExitRecord *tmp = (ExitRecord *)realloc(sys->exit_records, sizeof(ExitRecord) * sys->exit_record_capacity);
        if (!tmp) {
            printf("[오류] 메모리 재할당에 실패했습니다.\n");
            exit(1);
        }
        sys->exit_records = tmp;
    }

    ExitRecord *r = &sys->exit_records[sys->exit_record_count];
    r->entry_time = entry_time;
    r->exit_time = exit_time;
    r->fee = fee;
    r->parked_minutes = parked_minutes;
    sys->exit_record_count++;
}

/* 프로그램 종료 시 통계 기록 동적 메모리 해제 (메모리 누수 방지) */
void free_stats_records(ParkingSystem *sys)
{
    free(sys->entry_records);
    sys->entry_records = NULL;
    sys->entry_record_count = 0;
    sys->entry_record_capacity = 0;

    free(sys->exit_records);
    sys->exit_records = NULL;
    sys->exit_record_count = 0;
    sys->exit_record_capacity = 0;
}

/* 두 시각이 같은 날(연/월/일)인지 확인 */
static int is_same_day(time_t t, time_t ref)
{
    struct tm lt = *localtime(&t);
    struct tm lr = *localtime(&ref);
    return (lt.tm_year == lr.tm_year && lt.tm_yday == lr.tm_yday);
}

/* 두 시각이 같은 달(연/월)인지 확인 */
static int is_same_month(time_t t, time_t ref)
{
    struct tm lt = *localtime(&t);
    struct tm lr = *localtime(&ref);
    return (lt.tm_year == lr.tm_year && lt.tm_mon == lr.tm_mon);
}

/* 입차 시각을 기준으로 시간대(오전/오후/야간) 인덱스를 반환
     오전 : 06:00 ~ 11:59
     오후 : 12:00 ~ 17:59
     야간 : 18:00 ~ 05:59 */
static int time_slot_of(time_t t)
{
    struct tm *lt = localtime(&t);
    int hour = lt->tm_hour;
    if (hour >= 6 && hour < 12)      return 0; /* 오전 */
    else if (hour >= 12 && hour < 18) return 1; /* 오후 */
    else                               return 2; /* 야간 */
}

/*---------------------------------------------------------------------
  통계 조회 (기간별)
  range: STATS_RANGE_TODAY(오늘자) 또는 STATS_RANGE_MONTH(한달/이번 달)
  - 저장된 EntryRecord/ExitRecord 중 기간에 해당하는 것만 걸러서 계산한다.
---------------------------------------------------------------------*/
void show_statistics(const ParkingSystem *sys, StatsRange range)
{
    time_t now = time(NULL);

    printf("\n===== %s 통계 조회 =====\n", range == STATS_RANGE_TODAY ? "오늘자" : "한달(이번 달)");

    /* ---- 입차 기록 기반 : 입차 차량 수 / 시간대별 이용률 ---- */
    int slot_count[3] = { 0, 0, 0 };
    int total_entries = 0;

    for (int i = 0; i < sys->entry_record_count; i++) {
        time_t et = sys->entry_records[i].entry_time;
        int in_range = (range == STATS_RANGE_TODAY) ? is_same_day(et, now)
                                                      : is_same_month(et, now);
        if (!in_range) continue;

        slot_count[time_slot_of(et)]++;
        total_entries++;
    }

    /* ---- 출차 기록 기반 : 총 매출 / 출차 차량 수 / 평균 주차시간 ---- */
    double revenue = 0.0;
    double parked_minutes_sum = 0.0;
    int exit_count = 0;

    for (int i = 0; i < sys->exit_record_count; i++) {
        const ExitRecord *r = &sys->exit_records[i];
        int in_range = (range == STATS_RANGE_TODAY) ? is_same_day(r->exit_time, now)
                                                      : is_same_month(r->exit_time, now);
        if (!in_range) continue;

        revenue += r->fee;
        parked_minutes_sum += r->parked_minutes;
        exit_count++;
    }

    printf("총 매출         : %.0f원\n", revenue);
    printf("입차 차량 수    : %d대\n", total_entries);
    printf("출차 차량 수    : %d대\n", exit_count);

    if (exit_count > 0) {
        printf("평균 주차시간   : %.1f분\n", parked_minutes_sum / exit_count);
    } else {
        printf("평균 주차시간   : 데이터 없음\n");
    }

    /* ---- 시간대별 이용률 (입차 기록 기준) ---- */
    printf("\n[시간대별 이용률]\n");
    if (total_entries == 0) {
        printf("입차 기록이 없습니다.\n");
        return;
    }

    printf("오전  : %d건 (%.1f%%)\n", slot_count[0], slot_count[0] * 100.0 / total_entries);
    printf("오후  : %d건 (%.1f%%)\n", slot_count[1], slot_count[1] * 100.0 / total_entries);
    printf("야간  : %d건 (%.1f%%)\n", slot_count[2], slot_count[2] * 100.0 / total_entries);
}

/*---------------------------------------------------------------------
  통계 조회 메뉴 (관리자모드 3번)
  1. 오늘자 통계 조회   2. 한달 통계 조회
---------------------------------------------------------------------*/
void stats_menu(const ParkingSystem *sys)
{
    printf("\n===== 통계 조회 =====\n");
    printf("1. 오늘자 통계 조회\n");
    printf("2. 한달 통계 조회\n");
    int choice = read_int("선택: ", 1, 2);

    if (choice == 1) {
        show_statistics(sys, STATS_RANGE_TODAY);
    } else {
        show_statistics(sys, STATS_RANGE_MONTH);
    }
}
