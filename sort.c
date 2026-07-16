/*=====================================================================
  <sort.c>
  : 차량 정렬

  ---------------------------------------------------------------------
  [기능 설명]
  현재 주차된 차량들을 다음 기준 중 하나로 정렬하여 출력한다.
    1. 차량번호순
    2. 입차시간순
    3. 예상 주차요금순
    4. 차량 종류순
  정렬 알고리즘은 버블정렬(Bubble Sort)을 사용하며,
  Vehicle 포인터 배열의 순서만 교환하므로 실제 데이터 이동은 없다.
=====================================================================*/
#include "parking.h"

/* 두 Vehicle 포인터의 위치를 교환 */
static void swap_vehicle_ptr(Vehicle **a, Vehicle **b)
{
    Vehicle *tmp = *a;
    *a = *b;
    *b = tmp;
}

/*---------------------------------------------------------------------
  버블정렬 - 비교 기준(criteria)에 따라 오름차순 정렬
  criteria: 1=차량번호, 2=입차시간, 3=예상요금, 4=차량종류
---------------------------------------------------------------------*/
static void bubble_sort_vehicles(ParkingSystem *sys, int criteria)
{
    int n = sys->count;
    time_t now = time(NULL);

    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            Vehicle *a = sys->vehicles[j];
            Vehicle *b = sys->vehicles[j + 1];
            int need_swap = 0;

            switch (criteria) {
                case 1: /* 차량번호순 */
                    if (strcmp(a->plate, b->plate) > 0) need_swap = 1;
                    break;
                case 2: /* 입차시간순 (오래된 순) */
                    if (difftime(a->entry_time, b->entry_time) > 0) need_swap = 1;
                    break;
                case 3: /* 예상 주차요금순 (낮은 순) */
                    if (calculate_fee(a, now) > calculate_fee(b, now)) need_swap = 1;
                    break;
                case 4: /* 차량 종류순 */
                    if (a->car_type > b->car_type) need_swap = 1;
                    break;
                default:
                    break;
            }

            if (need_swap) {
                swap_vehicle_ptr(&sys->vehicles[j], &sys->vehicles[j + 1]);
            }
        }
    }
}

/*---------------------------------------------------------------------
  정렬 메뉴 (관리자모드 2번)
---------------------------------------------------------------------*/
void sort_menu(ParkingSystem *sys)
{
    if (sys->count == 0) {
        printf("\n[안내] 현재 주차된 차량이 없습니다.\n");
        return;
    }

    printf("\n===== 차량 정렬 =====\n");
    printf("1. 차량번호순\n");
    printf("2. 입차시간순\n");
    printf("3. 예상 주차요금순\n");
    printf("4. 차량 종류순\n");
    int choice = read_int("정렬 기준을 선택하세요: ", 1, 4);

    bubble_sort_vehicles(sys, choice);

    printf("\n----- 정렬 결과 -----\n");
    vehicle_list_all(sys);
}
