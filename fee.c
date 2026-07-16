/*=====================================================================
  <fee.c>
  : 요금 계산
  
  - calc_discount_rate() : 할인 종류에 따른 할인율 반환
  - discount_name() : 할인 종류 한글이름 반환
  - calculate_fee() : 요금 계산
  - fee_calculation_menu() : 메뉴 7번 "주차요금 계산" 처리


  [기능 설명]
  - 기본요금(최초 30분 1,000원, 이후 30분마다 1,000원 추가) 계산
  - 할인율 계산 (국가유공자 50%, 장애인 40%, 전기차 30%, 경차 20%)
    복수 할인 조건이 있어도 차량에는 하나의 할인만 저장되므로
    저장된 할인율을 그대로 적용한다.
  - 메뉴 7번 "주차요금 계산" 처리
=====================================================================*/
#include "parking.h"
#include <math.h>

/* 할인 종류에 따른 할인율 반환 */
double calc_discount_rate(DiscountType d) // 열거형을 매개변수로 받음
{
    switch (d) {
        case DISCOUNT_VETERAN:  return 0.50;
        case DISCOUNT_DISABLED: return 0.40;
        case DISCOUNT_ELECTRIC: return 0.30;
        case DISCOUNT_COMPACT:  return 0.20;
        case DISCOUNT_NONE:
        default:                return 0.0;
    }
}

// 할인 종류 한글이름 반환 
const char *discount_name(DiscountType d)
{
    switch (d) {
        case DISCOUNT_VETERAN:  return "국가유공자";
        case DISCOUNT_DISABLED: return "장애인";
        case DISCOUNT_ELECTRIC: return "전기차";
        case DISCOUNT_COMPACT:  return "경차";
        case DISCOUNT_NONE:
        default:                return "없음";
    }
}

/*---------------------------------------------------------------------
  요금 계산
  - 최초 30분 : 1,000원
  - 이후 30분마다 1,000원씩 추가 (30분 미만 초과분도 한 단위로 계산 - 올림)
  - 할인 적용 : 저장된 할인 종류의 할인율 적용
---------------------------------------------------------------------*/
double calculate_fee(const Vehicle *v, time_t now)
{
    double minutes = difftime(now, v->entry_time) / 60.0; // 입차시간부터 지금까지 얼마나 시간이 지났는지 (분)
	if (minutes < 0) minutes = 0; // 혹시라도 입차시간이 미래로 설정되어 있는 경우를 대비하여 음수 방지

    int units = (int)ceil(minutes / UNIT_MINUTES); // 30분 단위 소수점 올림!!
    if (units < 1) units = 1; /* 최소 최초 30분 요금은 부과 */

    double base_fee = units * FEE_PER_UNIT; // 할인 전 기본요금
    double rate = calc_discount_rate(v->discount); //이 차량의 할인 정보 가져옴

    return base_fee * (1.0 - rate); // 요금 반환
}

/*---------------------------------------------------------------------
  주차요금 계산 메뉴 (메뉴 7번)
  차량번호를 입력받아 현재까지의 예상 요금을 계산하여 출력한다.
---------------------------------------------------------------------*/
void fee_calculation_menu(ParkingSystem *sys)
{
    char plate[PLATE_LEN];
    printf("\n===== 주차요금 계산 =====\n");
    read_line("차량번호를 입력하세요: ", plate, sizeof(plate));

    int idx = find_vehicle_index_by_plate(sys, plate);
    if (idx == -1) {
        printf("[오류] 해당 차량번호를 찾을 수 없습니다.\n");
        return;
    }

    Vehicle *v = sys->vehicles[idx]; // 찾은 인덱스로 실제 Vehicle 구조체 포인터 v를 꺼냄
	time_t now = time(NULL); // 지금 시각
	double minutes = difftime(now, v->entry_time) / 60.0; // 입차시간부터 지금까지 얼마나 시간이 지났는지 (분)
	double fee = calculate_fee(v, now); // 요금 계산

    printf("\n차량번호      : %s\n", v->plate);
    printf("경과 시간     : 약 %.0f분\n", minutes);
    printf("할인 적용     : %s (%.0f%%)\n", discount_name(v->discount), calc_discount_rate(v->discount) * 100.0);
    printf("현재까지 요금 : %.0f원\n", fee);
}
