/*=====================================================================
  <parking_lot.c>
  : 주차 공간 초기화 / 추천 / 빈자리 조회

  - init_parking_spots() : 전체 주차 공간(구조체 배열) 초기화
  - recommend_parking_spot() : 차종에 따른 "스마트 주차공간 추천 알고리즘" 구현
  - view_empty_spots() : 구역별 빈 주차공간 현황 출력


  ---------------------------------------------------------------------
  [기능 설명]
  - 전체 주차 공간(구조체 배열) 초기화
  - 차종에 따른 "스마트 주차공간 추천 알고리즘" 구현
        추천점수 = 거리점수 + 혼잡도점수 + 차량 적합도 점수
        점수가 낮을수록 우선 추천됨
  - 전용구역이 모두 찼을 경우 일반구역으로 자동 대체(fallback) 처리
  - 구역별 빈 주차공간 현황 출력(6번 메뉴)
=====================================================================*/
#include "parking.h"

/* 가중치 상수 (점수 계산에 사용) */
#define WEIGHT_DISTANCE     1.0   /* 거리 1당 가중치 */
#define WEIGHT_CONGESTION   50.0  /* 혼잡도(0~1) 가중치 */
#define PENALTY_FALLBACK    50.0  /* 전용구역이 아닌 일반구역 사용 시 적합도 페널티 */


/*---------------------------------------------------------------------
  구역/차종 이름 문자열 반환 (출력용)
---------------------------------------------------------------------*/
const char *zone_name(ZoneType zone)
{
    switch (zone) {
        case ZONE_NORMAL:   return "일반";
        case ZONE_ELECTRIC: return "전기차";
        case ZONE_COMPACT:  return "경차";
        case ZONE_DISABLED: return "장애인";
        case ZONE_PREGNANT: return "임산부";
        default:            return "알수없음";
    }
}

const char *car_type_name(CarType type)
{
    switch (type) {
        case CAR_NORMAL:    return "일반차량";
        case CAR_ELECTRIC:  return "전기차";
        case CAR_COMPACT:   return "경차";
        case CAR_DISABLED:  return "장애인차량";
        case CAR_PREGNANT:  return "임산부차량";
        default:            return "알수없음";
    }
}


/* 차종에 따른 우선 배정 구역 */
ZoneType preferred_zone_for_car(CarType type)
{
    switch (type) {
        case CAR_NORMAL:   return ZONE_NORMAL;
        case CAR_ELECTRIC: return ZONE_ELECTRIC;
        case CAR_COMPACT:  return ZONE_COMPACT;
        case CAR_DISABLED: return ZONE_DISABLED;
        case CAR_PREGNANT: return ZONE_PREGNANT;
        default:           return ZONE_NORMAL;
    }
}

/*---------------------------------------------------------------------
  구역별 배치 설정(공간 개수, 가로 길이, 입구 좌표) 가져옴
---------------------------------------------------------------------*/
void get_zone_layout(ZoneType zone, int *count, int *width, int *entrance_row, int *entrance_col)
{
    switch (zone) {
        case ZONE_NORMAL:
            *count = NUM_NORMAL;   *width = NORMAL_WIDTH;
            *entrance_row = NORMAL_ENTRANCE_ROW; *entrance_col = NORMAL_ENTRANCE_COL;
            break;
        case ZONE_ELECTRIC:
            *count = NUM_ELECTRIC; *width = ELECTRIC_WIDTH;
            *entrance_row = ELECTRIC_ENTRANCE_ROW; *entrance_col = ELECTRIC_ENTRANCE_COL;
            break;
        case ZONE_COMPACT:
            *count = NUM_COMPACT;  *width = COMPACT_WIDTH;
            *entrance_row = COMPACT_ENTRANCE_ROW; *entrance_col = COMPACT_ENTRANCE_COL;
            break;
        case ZONE_DISABLED:
            *count = NUM_DISABLED; *width = DISABLED_WIDTH;
            *entrance_row = DISABLED_ENTRANCE_ROW; *entrance_col = DISABLED_ENTRANCE_COL;
            break;
        case ZONE_PREGNANT:
            *count = NUM_PREGNANT; *width = PREGNANT_WIDTH;
            *entrance_row = PREGNANT_ENTRANCE_ROW; *entrance_col = PREGNANT_ENTRANCE_COL;
            break;
        default:
            *count = 0; *width = 1; *entrance_row = 0; *entrance_col = 0;
            break;
    }
}

/*---------------------------------------------------------------------
  한 구역의 주차 공간들을 자동으로 생성한다.
  - count(예: NUM_NORMAL)개의 공간을, 가로 길이 width 기준으로
    왼쪽 위 칸부터 순서대로 배치한다.
  - 입구 좌표(entrance_row, entrance_col)에 해당하는 칸은 건너뛰고
    그 다음 칸부터 이어서 배치한다. (입구도 격자상의 한 칸을 차지)
  - 각 공간의 distance(입구까지의 거리)는 좌표 차이의 맨해튼 거리로
    자동 계산되므로, NUM_* 이나 WIDTH/ENTRANCE 값을 바꾸면 거리도
    자동으로 다시 계산된다.
---------------------------------------------------------------------*/
static void create_zone_spots(ParkingSystem *sys, int *idx, ZoneType zone, char prefix,
                               int count, int width, int entrance_row, int entrance_col)
{
    int assigned = 0; // 지금까지 몇 개의 "실제 주차 공간"을 배치했는지
    int spot_number = 1; // "A1", "A2"... 뒤에 붙는 번호
    int total_cells = count + 1; /* 실제 주차 공간 + 입구 칸 1개 */
    int rows = (total_cells + width - 1) / width; /* 올림 나눗셈으로 필요한 줄 수 계산 */

    for (int cell = 0; cell < rows * width && assigned < count; cell++) {
        int row = cell / width; // 0번째 칸부터 순서대로 세면서 몇 번째 줄인지
        int col = cell % width; // 그 줄에서 몇 번째 칸인지

        if (row == entrance_row && col == entrance_col) continue; /* 입구 칸은 제외 */

        sprintf(sys->spots[*idx].location, "%c%d", prefix, spot_number++); // "A1","A2".. 문자열 생성(prefix='A', 번호는 매번 1씩 증가)
        sys->spots[*idx].row = row;
        sys->spots[*idx].col = col;
        sys->spots[*idx].distance = abs(row - entrance_row) + abs(col - entrance_col); /* 맨해튼 거리 */
        sys->spots[*idx].zone = zone; 
        sys->spots[*idx].occupied = 0; // 처음엔 무조건 빈 자리로 시작
        sys->spots[*idx].vehicle_plate[0] = '\0';

        (*idx)++; // sys->spots 배열 전체에서 "다음에 채울 위치"를 가리키는 전역 인덱스 1 증가
        assigned++; // 이 구역에서 배치한 개수 1 증가
    }
}

/*---------------------------------------------------------------------
  전체 주차 공간 초기화
  - 각 구역의 NUM_*(개수), WIDTH(가로 길이), ENTRANCE(입구 좌표) 설정에
    맞춰 자동으로 공간을 생성한다. parking.h 의 상수만 바꾸면 이 함수는
    수정할 필요가 없다.
---------------------------------------------------------------------*/
void init_parking_spots(ParkingSystem *sys)
{
    int idx = 0;

    create_zone_spots(sys, &idx, ZONE_NORMAL,   'A', NUM_NORMAL,   NORMAL_WIDTH,   NORMAL_ENTRANCE_ROW,   NORMAL_ENTRANCE_COL);
    create_zone_spots(sys, &idx, ZONE_ELECTRIC, 'B', NUM_ELECTRIC, ELECTRIC_WIDTH, ELECTRIC_ENTRANCE_ROW, ELECTRIC_ENTRANCE_COL);
    create_zone_spots(sys, &idx, ZONE_COMPACT,  'C', NUM_COMPACT,  COMPACT_WIDTH,  COMPACT_ENTRANCE_ROW,  COMPACT_ENTRANCE_COL);
    create_zone_spots(sys, &idx, ZONE_DISABLED, 'D', NUM_DISABLED, DISABLED_WIDTH, DISABLED_ENTRANCE_ROW, DISABLED_ENTRANCE_COL);
    create_zone_spots(sys, &idx, ZONE_PREGNANT, 'E', NUM_PREGNANT, PREGNANT_WIDTH, PREGNANT_ENTRANCE_ROW, PREGNANT_ENTRANCE_COL);
}

/* 위치 문자열로 spots 배열의 인덱스 찾기 (없으면 -1) */
int find_spot_index_by_location(const ParkingSystem *sys, const char *location)
{
    for (int i = 0; i < TOTAL_SPOTS; i++) {
        if (strcmp(sys->spots[i].location, location) == 0) return i;
    }
    return -1;
}

/* 특정 구역의 혼잡도(0.0~1.0) 계산: ( 사용중인 공간 수 / 전체 공간 수 ) */
static double zone_congestion(const ParkingSystem *sys, ZoneType zone)
{
    int total = 0, occ = 0;
    for (int i = 0; i < TOTAL_SPOTS; i++) {
        if (sys->spots[i].zone == zone) {
            total++;
            if (sys->spots[i].occupied) occ++;
        }
    }
    if (total == 0) return 0.0;
    return (double)occ / (double)total;
}

/*---------------------------------------------------------------------
  **** 스마트 주차공간 추천 알고리즘 ****
 
  - 우선 차종에 맞는 전용구역에서 빈 자리를 탐색
  - 전용구역이 모두 찼다면 일반구역에서 탐색(단, 적합도 페널티 부여)
  - 각 후보 공간에 대해 
        [   추천점수 = 거리점수 + 혼잡도점수 + 적합도점수     ]       를 계산하여 점수가 가장 "낮은" 공간을 선택
  - 반환값 : 선택된 spot의 배열 인덱스(없으면 -1)
  - out_score : 최종 선택된 공간의 추천 점수(정수로 반올림)
  - reason_buf : 추천 이유를 사람이 읽을 수 있는 문자열로 채움
---------------------------------------------------------------------*/
int recommend_spot(ParkingSystem *sys, CarType type, int *out_score, char *reason_buf, size_t reason_buf_size)
{
    ZoneType preferred = preferred_zone_for_car(type); // 이 차종의 "원래 우선구역"이 뭔지 먼저 확인(②)
    int best_idx = -1; // 지금까지 찾은 최적 후보의 배열 인덱스
    double best_score = 1e18; // 아주 큰 값으로 초기화(뭐가 오든 처음엔 무조건 이 값보다 작을 수밖에 없게)
    int used_fallback = 0; // 전용구역이 다 차서 일반구역으로 대체했는지 여부


    /* 1차 시도 : 전용구역에서 탐색 */
    for (int i = 0; i < TOTAL_SPOTS; i++) {
        if (sys->spots[i].zone == preferred && !sys->spots[i].occupied) { // 원하는 구역이면서 비어있는 자리만
            double congestion = zone_congestion(sys, preferred); // 이 구역의 혼잡도(구역 전체가 같은 값을 공유)
            double score = sys->spots[i].distance * WEIGHT_DISTANCE // 거리점수 = 거리 × 1.0
                          + congestion * WEIGHT_CONGESTION          // 혼잡도점수 = 혼잡도(0~1) × 50.0
                          + 0.0; /* 전용구역이므로 적합도 페널티 없음 */
            if (score < best_score) {
                best_score = score;
                best_idx = i;
                used_fallback = 0;
            }
        }
    }

    /* 2차 시도 : 전용구역이 모두 찼다면(전용구역이 일반구역이 아닌 경우) 일반구역으로 대체 */
    if (best_idx == -1 && preferred != ZONE_NORMAL) {
        for (int i = 0; i < TOTAL_SPOTS; i++) {
            if (sys->spots[i].zone == ZONE_NORMAL && !sys->spots[i].occupied) {
                double congestion = zone_congestion(sys, ZONE_NORMAL);
                double score = sys->spots[i].distance * WEIGHT_DISTANCE
                              + congestion * WEIGHT_CONGESTION
                              + PENALTY_FALLBACK; // 여기서만 50점 페널티 추가! (전용구역이 아니라서)
                if (score < best_score) {
                    best_score = score;
                    best_idx = i;
                    used_fallback = 1;
                }
            }
        }
    }
     
    if (best_idx == -1) { // 1차, 2차 다 실패 = 주차장에 빈자리가 아예 없음
        if (reason_buf && reason_buf_size > 0) reason_buf[0] = '\0';
        return -1;
    }

    if (out_score) *out_score = (int)(best_score + 0.5); // 최종 점수를 정수로 반올림해서 out-parameter로 돌려줌

    /* 추천 이유 문자열 구성 */
    if (reason_buf && reason_buf_size > 0) {
        char line1[128], line2[128], line3[128];

        if (used_fallback) {
            snprintf(line1, sizeof(line1), "- 전용구역이 만차라 일반구역으로 배정됨");
        } else {
            snprintf(line1, sizeof(line1), "- 차량 종류에 적합한 구역(%s구역)", zone_name(sys->spots[best_idx].zone));
        }
        snprintf(line2, sizeof(line2), "- 입구에서의 거리: %d (가까울수록 좋음)", sys->spots[best_idx].distance);
        snprintf(line3, sizeof(line3), "- 현재 %s구역 혼잡도: %.0f%%",
                 zone_name(sys->spots[best_idx].zone),
                 zone_congestion(sys, sys->spots[best_idx].zone) * 100.0);

        snprintf(reason_buf, reason_buf_size, "%s\n%s\n%s", line1, line2, line3);
    }

    return best_idx;
}

/*---------------------------------------------------------------------
  빈 주차공간 조회 (사용자모드 4번)
  구역별 사용중/빈자리 목록 대신, 한눈에 보기 쉬운 "주차장 배치도"만
  보여준다. (view_parking_map 참고)
---------------------------------------------------------------------*/
void view_empty_spots(const ParkingSystem *sys)
{
    view_parking_map(sys);
}

/*---------------------------------------------------------------------
  특정 구역 하나를 격자(가로 길이 WIDTH 기준) 형태로 출력
  - 입구 좌표에는 △ 표시
  - 실제 주차 공간은 사용중(■)/빈자리(□)로 표시
  - highlight_location 이 NULL이 아니면, 해당 위치의 칸만 하트(♥)로
    표시하여 어느 자리인지 한눈에 알아볼 수 있게 한다. (배정된 자리 강조용)
  - init_parking_spots()와 완전히 동일한 순서/좌표 규칙으로 순회하므로
    실제 배정된 좌표와 항상 일치한다.
---------------------------------------------------------------------*/
static void print_zone_map_internal(const ParkingSystem *sys, ZoneType zone, const char *highlight_location)
{
    int count, width, entrance_row, entrance_col;
    get_zone_layout(zone, &count, &width, &entrance_row, &entrance_col);

    if (count == 0) {
        printf("(공간 없음)\n");
        return;
    }

    /* 해당 구역 공간의 인덱스를 생성 순서(=좌표 배치 순서) 그대로 모은다
       (배열을 0으로 초기화해서 Visual Studio 정적분석기가 "초기화되지 않은
        메모리 사용"으로 오탐(C6001)하는 것을 방지한다) */
    int idxs[TOTAL_SPOTS] = { 0 };
    int n = 0;
    for (int i = 0; i < TOTAL_SPOTS; i++) {
        if (sys->spots[i].zone == zone) idxs[n++] = i;
    }

    int total_cells = count + 1; /* 주차 공간 + 입구 칸 */
    int rows = (total_cells + width - 1) / width;
    int spot_counter = 0;

    for (int cell = 0; cell < rows * width; cell++) {
        int row = cell / width;
        int col = cell % width;

        if (row == entrance_row && col == entrance_col) {
            printf("△ ");
        } else if (cell < total_cells && spot_counter < n) {
            int sidx = idxs[spot_counter++];
            if (highlight_location && strcmp(sys->spots[sidx].location, highlight_location) == 0) {
                printf("♥ ");
            } else {
                printf("%s ", sys->spots[sidx].occupied ? "■" : "□");
            }
        }
        /* cell >= total_cells 인 칸은 실제로 쓰이지 않는 여분 칸이므로 아무것도 그리지 않는다 */

        if (col == width - 1) printf("\n"); /* 한 줄(가로 길이)이 끝나면 줄바꿈 */
    }
}

void print_zone_map(const ParkingSystem *sys, ZoneType zone)
{
    print_zone_map_internal(sys, zone, NULL);
}

/* 특정 위치(예: "B1")만 하트(♥)로 강조하여 해당 구역의 배치도를 출력 */
void print_zone_map_highlight(const ParkingSystem *sys, ZoneType zone, const char *highlight_location)
{
    print_zone_map_internal(sys, zone, highlight_location);
}

/*---------------------------------------------------------------------
  전체 구역의 주차장 배치도 출력
  - 실제 주차장 센서/카메라와 연동할 수 없는 학교 프로젝트 특성상,
    사용자가 눈으로 보기 쉬운 가상의 배치도를 텍스트로 그려서 보여준다.
---------------------------------------------------------------------*/
void view_parking_map(const ParkingSystem *sys)
{
    const ZoneType zones[ZONE_COUNT] = {ZONE_NORMAL, ZONE_ELECTRIC, ZONE_COMPACT, ZONE_DISABLED, ZONE_PREGNANT};

    printf("\n===== 주차장 배치도 (△ = 입구) =====\n");
    for (int z = 0; z < ZONE_COUNT; z++) {
        printf("\n[%s구역]\n", zone_name(zones[z]));
        print_zone_map(sys, zones[z]);
    }
    printf("\n(■ = 사용중, □ = 빈자리, △ = 입구)\n");
}

