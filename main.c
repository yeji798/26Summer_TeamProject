/*=====================================================================
* 테스트!!!!
  <main.c>
  - SystemMode : 현재 시스템 모드 (열거체) (사용자모드, 관리자모드)
  - clear_input_buffer() : 표준 입력 버퍼 비우기
  - read_int() : 범위 검증 정수 입력함수 
  - read_line() : 문자열 입력 함수
  - print_time() : 시각 yyy-mm-dd hh:mm 형식으로 출력
  - print_top_menu() : 메인 메뉴 출력 (1. 사용자모드, 2.관리자모드)
  - print_user_menu() : 사용자모드 메뉴 출력
  - print_admin_menu() : 관리자모드 메뉴 출력
  - check_admin_password() : 관리자 비밀번호 확인
  - run_user_mode() : 사용자모드
  - run_admin_mode() : 관리자모드
  - ParkingSystem : 전체 시스템 상태
=====================================================================*/
#include "parking.h"

/* 현재 시스템이 어떤 모드로 동작 중인지 나타내는 열거형 */
typedef enum {
    MODE_USER = 0,
    MODE_ADMIN
} SystemMode;


//표준입력 버퍼 비우기 (scanf 이후 남은 개행 문자 등 제거용)
void clear_input_buffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* 버림 */ }
}

//범위를 검증하는 정수 입력 함수
int read_int(const char *prompt, int min, int max)
{
    int value;
    while (1) {
        printf("%s", prompt);
        int result = scanf("%d", &value);
        if (result == EOF) {
            /* 표준입력이 종료된 경우(예: 파이프 입력 소진) 무한루프를 막기 위해
               프로그램을 안전하게 종료한다. */
            printf("\n[안내] 입력이 종료되어 프로그램을 종료합니다.\n");
            exit(0);
        }
        if (result != 1) {
            printf("[오류] 숫자를 입력해주세요.\n");
            clear_input_buffer();
            continue;
        }
        clear_input_buffer();
        if (value < min || value > max) {
            printf("[오류] %d ~ %d 사이의 값을 입력해주세요.\n", min, max);
            continue;
        }
        return value;
    }
}

//문자열(한 줄) 입력 함수 - 공백 포함 가능, 개행 제거
void read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else {
        /* 입력이 버퍼보다 길었을 경우 남은 문자 제거 */
        clear_input_buffer();
    }
}

/* 시각(time_t)을 "YYYY-MM-DD HH:MM" 형식으로 출력 */
void print_time(time_t t)
{
    struct tm *lt = localtime(&t);
    printf("%04d-%02d-%02d %02d:%02d\n",
           lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min);
}


/*---------------------------------------------------------------------
  메뉴 출력 함수들
---------------------------------------------------------------------*/
static void print_top_menu(void)
{
    printf("\n=============================\n");
    printf(" Smart Parking System\n");
    printf("=============================\n");
    printf("1. 사용자모드\n");
    printf("2. 관리자모드\n");
    printf("=============================\n");
}

static void print_user_menu(void)
{
    printf("\n=============================\n");
    printf(" Smart Parking System\n");
    printf("=============================\n");
    printf("1. 차량 입차\n");
    printf("2. 차량 출차\n");
    printf("3. 차량 검색\n");
    printf("4. 빈 주차공간 조회\n");
    printf("5. 주차요금 계산\n");
    printf("6. 관리자모드로 변경\n");
    printf("0. 종료\n");
    printf("=============================\n");
}

static void print_admin_menu(void)
{
    printf("\n=============================\n");
    printf(" Smart Parking System\n");
    printf("=============================\n");
    printf("1. 전체 차량 조회\n");
    printf("2. 차량 정렬\n");
    printf("3. 통계 조회\n");
    printf("4. 파일 저장\n");
    printf("5. 사용자모드로 변경\n");
    printf("6. 주차장 배치도 조회\n");
    printf("0. 종료\n");
    printf("=============================\n");
}

/*---------------------------------------------------------------------
  관리자모드 비밀번호 확인
  parking.h 에 정의된 ADMIN_PASSWORD 와 일치하면 1(성공), 아니면 0(실패) 반환
---------------------------------------------------------------------*/
static int check_admin_password(void)
{
    char input[64];
    read_line("관리자 비밀번호를 입력하세요: ", input, sizeof(input));
    return (strcmp(input, ADMIN_PASSWORD) == 0);
}

/*---------------------------------------------------------------------
  사용자모드 메뉴 처리
  반환값 : 1이면 프로그램 전체 종료 요청, 0이면 정상 흐름(모드 유지)
---------------------------------------------------------------------*/
static int run_user_mode(ParkingSystem *sys, SystemMode *mode)
{
    print_user_menu();
    int choice = read_int("메뉴를 선택하세요: ", 0, 6);

    switch (choice) {
        case 1: vehicle_entry(sys);        break;
        case 2: vehicle_exit(sys);         break;
        case 3: vehicle_search(sys);       break;
        case 4: view_empty_spots(sys);     break;
        case 5: fee_calculation_menu(sys); break;
        case 6:
            if (check_admin_password()) {
                *mode = MODE_ADMIN;
                printf("\n[안내] 관리자모드로 전환되었습니다.\n");
            } else {
                printf("\n[오류] 비밀번호가 일치하지 않습니다.\n");
            }
            break;
        case 0:
            printf("\n프로그램을 종료합니다. 데이터를 저장합니다...\n");
            save_to_file(sys);
            return 1; /* 전체 종료 */
        default:
            printf("[오류] 잘못된 메뉴입니다.\n");
            break;
    }
    return 0;
}

/*---------------------------------------------------------------------
  관리자모드 메뉴 처리
  반환값 : 1이면 프로그램 전체 종료 요청, 0이면 정상 흐름
  *mode  : "5.사용자모드로 변경" 선택 시 MODE_USER 로 전환
---------------------------------------------------------------------*/
static int run_admin_mode(ParkingSystem *sys, SystemMode *mode)
{
    print_admin_menu();
    int choice = read_int("메뉴를 선택하세요: ", 0, 6);

    switch (choice) {
        case 1: vehicle_list_all(sys);   break;
        case 2: sort_menu(sys);          break;
        case 3: stats_menu(sys);         break;
        case 4: save_to_file(sys);       break;
        case 5:
            *mode = MODE_USER;
            printf("\n[안내] 사용자모드로 전환되었습니다.\n");
            break;
        case 6: view_parking_map(sys);   break;
        case 0:
            printf("\n프로그램을 종료합니다. 데이터를 저장합니다...\n");
            save_to_file(sys);
            return 1; /* 전체 종료 */
        default:
            printf("[오류] 잘못된 메뉴입니다.\n");
            break;
    }
    return 0;
}





/*---------------------------------------------------------------------
  메인
---------------------------------------------------------------------*/
int main(void)
{
    ParkingSystem sys; // 전체 프로그램에서 사용할 시스템 상태 
    memset(&sys, 0, sizeof(ParkingSystem)); //구조체 메모리 전체를 0을 초기화

    init_parking_spots(&sys);
    load_from_file(&sys);   /* 프로그램 실행 시 자동으로 기존 데이터 불러오기 */

    SystemMode mode;
    int program_running = 1;

    /* 최초 진입 시 최상위 메뉴에서 사용자모드/관리자모드 중 하나를 선택 */
    while (program_running) {
        print_top_menu();
        int top_choice = read_int("메뉴를 선택하세요: ", 1, 2);

        if (top_choice == 1) {
            mode = MODE_USER;
        } else {
            if (check_admin_password()) {
                mode = MODE_ADMIN;
                printf("\n[안내] 관리자모드로 진입합니다.\n");
            } else {
                printf("\n[오류] 비밀번호가 일치하지 않습니다.\n");
                continue; /* 다시 최상위 메뉴로 */
            }
        }

        /* 선택된 모드에 진입한 뒤에는, 0(종료)을 누르거나
           관리자모드의 "사용자모드로 변경"을 통해서만 모드가 바뀌거나
           프로그램이 종료된다. 관리자모드 내에서는 계속 관리자모드를
           유지하며, 사용자모드로 전환된 뒤에도 계속 그 안에 머문다.
           (요구사항에 명시된 메뉴 구성을 그대로 따른 흐름) */
        int exit_requested = 0;
        while (!exit_requested) {
            if (mode == MODE_USER) {
                exit_requested = run_user_mode(&sys, &mode);
            } else {
                exit_requested = run_admin_mode(&sys, &mode);
            }
        }
        program_running = 0; /* 0.종료 선택으로만 이 while 을 빠져나옴 */
    }

    free_all_vehicles(&sys);   /* 차량 동적 메모리 누수 방지 */
    free_stats_records(&sys);  /* 통계 기록 동적 메모리 누수 방지 */
    return 0;
}
