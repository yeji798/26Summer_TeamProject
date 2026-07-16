/*=====================================================================
  <parking.h>
  - 프로젝트 전역에서 사용하는 구조체, 열거형, 매크로 상수 정의
  - 구현된 함수들 선언
=====================================================================*/

//헤더 파일이 여러번 포함되는 것을 막기 위한!
#ifndef PARKING_H
#define PARKING_H

//비주얼 스튜디오 전용설정
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

//헤더파일
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/*---------------------------------------------------------------------
  1. 매크로 상수
---------------------------------------------------------------------*/
#define PLATE_LEN        20     /* 차량 번호 문자열 최대 길이        */
#define LOC_LEN          10     /* 주차 위치 문자열 최대 길이 ("A1") */
#define FILE_NAME        "parking.txt" /*파일이름*/
#define ADMIN_PASSWORD   "1234" /* 관리자모드 진입 비밀번호 (필요 시 이 값만 수정하면 됩니다) */

#define FEE_PER_UNIT     1000.0 /* 30분당 요금 (원)                  */
#define UNIT_MINUTES     30.0   /* 요금 계산 단위 (분)                */

/* 구역별 총 주차공간 개수 (관리자가 이 값만 바꾸면 공간 수가 자동으로 조정됨) */
#define NUM_NORMAL       40     //일반차량
#define NUM_ELECTRIC     40     //전기차
#define NUM_COMPACT      20     //경차  
#define NUM_DISABLED     20     //장애인 차량  
#define NUM_PREGNANT     10     //임산부차량
#define TOTAL_SPOTS      (NUM_NORMAL + NUM_ELECTRIC + NUM_COMPACT + NUM_DISABLED + NUM_PREGNANT) 


// 구역별 배치 설정 (WIDTH : 해당 구역의 가로길이 / ENTRANCE_ROW/COL : 입구가 위치한 좌표 (0부터 시작))
#define NORMAL_WIDTH          10
#define NORMAL_ENTRANCE_ROW   0
#define NORMAL_ENTRANCE_COL   0

#define ELECTRIC_WIDTH        10
#define ELECTRIC_ENTRANCE_ROW 0
#define ELECTRIC_ENTRANCE_COL 0

#define COMPACT_WIDTH         5
#define COMPACT_ENTRANCE_ROW  0
#define COMPACT_ENTRANCE_COL  0

#define DISABLED_WIDTH        5
#define DISABLED_ENTRANCE_ROW 0
#define DISABLED_ENTRANCE_COL 0

#define PREGNANT_WIDTH        5
#define PREGNANT_ENTRANCE_ROW 0
#define PREGNANT_ENTRANCE_COL 0


/*---------------------------------------------------------------------
  2. 열거형 : 구역(공간) 종류 / 차량 종류 / 할인 종류
---------------------------------------------------------------------*/
typedef enum { //구역 종류
    ZONE_NORMAL = 0,
    ZONE_ELECTRIC,
    ZONE_COMPACT,
    ZONE_DISABLED,
    ZONE_PREGNANT,
    ZONE_COUNT
} ZoneType;

typedef enum { //차량 종류
    CAR_NORMAL = 0,
    CAR_ELECTRIC,
    CAR_COMPACT,
    CAR_DISABLED,
    CAR_PREGNANT,
    CAR_TYPE_COUNT
} CarType;

typedef enum { //할인 종류
    DISCOUNT_NONE = 0,
    DISCOUNT_VETERAN,   /* 국가유공자 50% */
    DISCOUNT_DISABLED,  /* 장애인 40%     */
    DISCOUNT_ELECTRIC,  /* 전기차 30%     */
    DISCOUNT_COMPACT,   /* 경차 20%       */
    DISCOUNT_COUNT
} DiscountType;

typedef enum { /* 통계 조회 범위 : 오늘자 / 한달(이번 달) */
    STATS_RANGE_TODAY = 0,
    STATS_RANGE_MONTH
} StatsRange;

/*---------------------------------------------------------------------
  3. 구조체
---------------------------------------------------------------------*/
//주차공간
typedef struct {
    char     location[LOC_LEN];  /* 예: "A1", "B3" */
    int      row;                 /* 구역 내 좌표(행) - 0부터 시작 */
    int      col;                 /* 구역 내 좌표(열) - 0부터 시작 */
    int      distance;           /* 입구 좌표와의 맨해튼 거리(자동 계산됨) */
    ZoneType zone;                /* 구역 종류 */
    int      occupied;           /* 0 = 비어있음, 1 = 사용중 */
    char     vehicle_plate[PLATE_LEN]; /* 현재 주차된 차량번호(없으면 빈 문자열) */
} ParkingSpot;


//차량
typedef struct {
    char         plate[PLATE_LEN];   /* 차량번호            */
    CarType      car_type;           /* 차종                */
    DiscountType discount;           /* 적용 할인 종류      */
    time_t       entry_time;         /* 입차 시각            */
    char         location[LOC_LEN];  /* 배정된 주차 위치     */
    int          is_settled;         /* 요금 정산 완료 여부(출차 직전 플래그) */
    int          recommend_score;    /* 입차 시 계산된 추천 점수(정렬 기준용) */
} Vehicle;

//통계용
typedef struct {
    time_t entry_time;   /* 입차 시각 */
} EntryRecord;

typedef struct {
    time_t entry_time;      /* 입차 시각(참고용) */
    time_t exit_time;       /* 출차(정산) 시각 */
    double fee;              /* 정산된 요금 */
    double parked_minutes;   /* 총 주차 시간(분) */
} ExitRecord;



// 전체 시스템 상태
typedef struct {
    Vehicle **vehicles;     /* Vehicle 포인터의 동적 배열 */
    int       count;        /* 현재 주차된 차량 수         */
    int       capacity;     /* 배열에 할당된 용량           */

    ParkingSpot spots[TOTAL_SPOTS]; /* 전체 주차 공간(고정 배열) */

    /* 통계용 기록 배열 (동적 메모리, realloc으로 확장) */
    EntryRecord *entry_records;
    int          entry_record_count;
    int          entry_record_capacity;

    ExitRecord  *exit_records;
    int          exit_record_count;
    int          exit_record_capacity;
} ParkingSystem;


/*=====================================================================
  4. 함수 프로토타입
=====================================================================*/

/* ---------- parking_lot.c : 주차 공간 초기화 / 추천 / 빈자리 조회 ---------- */
void   init_parking_spots(ParkingSystem *sys);
const char *zone_name(ZoneType zone);
const char *car_type_name(CarType type);
ZoneType preferred_zone_for_car(CarType type);
void   get_zone_layout(ZoneType zone, int *count, int *width, int *entrance_row, int *entrance_col);
int    recommend_spot(ParkingSystem *sys, CarType type, int *out_score, char *reason_buf, size_t reason_buf_size);
void   view_empty_spots(const ParkingSystem *sys);
void   print_zone_map(const ParkingSystem *sys, ZoneType zone);
void   print_zone_map_highlight(const ParkingSystem *sys, ZoneType zone, const char *highlight_location);
void   view_parking_map(const ParkingSystem *sys);
int    find_spot_index_by_location(const ParkingSystem *sys, const char *location);

/* ---------- vehicle_manager.c : 입차/출차/검색/조회 ---------- */
void   vehicle_entry(ParkingSystem *sys);
void   vehicle_exit(ParkingSystem *sys);
void   vehicle_search(ParkingSystem *sys);
void   vehicle_list_all(const ParkingSystem *sys);
void   add_vehicle_to_system(ParkingSystem *sys, Vehicle *v);
void   remove_vehicle_from_system(ParkingSystem *sys, int index);
int    find_vehicle_index_by_plate(const ParkingSystem *sys, const char *plate);
void   free_all_vehicles(ParkingSystem *sys);

/* ---------- fee.c : 요금 계산 ---------- */
double calc_discount_rate(DiscountType d);
double calculate_fee(const Vehicle *v, time_t now);
void   fee_calculation_menu(ParkingSystem *sys);
const char *discount_name(DiscountType d);

/* ---------- sort.c : 정렬 ---------- */
void   sort_menu(ParkingSystem *sys);

/* ---------- stats.c : 통계 ---------- */
void   stats_menu(const ParkingSystem *sys);
void   show_statistics(const ParkingSystem *sys, StatsRange range);
void   record_exit_stats(ParkingSystem *sys, time_t entry_time, time_t exit_time, double fee, double parked_minutes);
void   record_entry_time_slot(ParkingSystem *sys, time_t entry_time);
void   free_stats_records(ParkingSystem *sys);

/* ---------- file_io.c : 파일 저장/불러오기 ---------- */
void   save_to_file(const ParkingSystem *sys);
void   load_from_file(ParkingSystem *sys);

/* ---------- 공용 유틸(main.c 에 구현) ---------- */
void   clear_input_buffer(void);
int    read_int(const char *prompt, int min, int max);
void   read_line(const char *prompt, char *buf, size_t size);
void   print_time(time_t t);

#endif /* PARKING_H */
