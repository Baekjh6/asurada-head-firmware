// ==========================================================================
// 아스라다 헤드 v03 - 조정값 모음
//   동작을 바꾸고 싶을 때는 이 파일만 고치면 됩니다.
// ==========================================================================
#pragma once

// ---------------- 화면 시퀀스 시간 ----------------
//   속도계 3초 + 1단 9초 + 2단 5초 = 17초  ->  /6 음원(16.9초)과 맞음 (사용자 지정, 2026-11-23)
#define SPEED_MAX_NUM       200     // 속도계: 0 에서 이 숫자까지 올라감
#define SPEED_MS           3000     // 속도계 0 -> 200 에 걸리는 시간

#define B1_START_NUM         14     // 1단 시작 숫자. 꽃잎 수 = 시작 - 끝 + 1 = 9장 (최대 16장 = 시작 21)
#define B1_END_NUM            6     // 1단 마지막 숫자 (꽃잎 1장 남음)
#define B1_STEP_MS         1000     // 1단: 숫자 하나(꽃잎 한 장)에 걸리는 시간
#define B1_FADE_MS          260     // 꽃잎이 어두워지며 사라지는 시간

#define B2_SLIDE_MS         450     // 1단 -> 2단: 2단 화면이 오른쪽에서 왼쪽으로 밀고 들어오는 시간 (1단의 마지막 0.45초)
#define B2_START_NUM          5     // 2단 시작 숫자 (1 에서 끝)
#define B2_STEP_MS         1000     // 2단: 숫자 하나에 걸리는 시간 = 흰 점이 한 바퀴 도는 시간
#define B2_ERASE_MS         350     // 빨간 영역 하나가 지워지는 시간

#define SPEED_HOLD_MS      20000    // 터치로 연 속도계 화면에서 이 시간 동안 아무것도 안 하면 대기화면으로. 0 = 계속 둠
#define BLACK_MS            400     // 2단이 끝난 뒤 검은 화면 시간
#define DIGIT_GHOST_MS      110     // 숫자가 바뀔 때 이전 숫자의 획이 겹쳐 보이는 시간

#define ORB_RADIUS          128     // 대기화면 구슬 4개가 놓이는 반지름
#define ORB_SPIN_IN_MS      650     // 구슬이 돌면서 나타나는 시간
#define ORB_TURN_MS         520     // 대기 중 90도 회전에 걸리는 시간
#define ORB_TURN_EVERY_MS 30000     // 대기 중 회전 간격

// ---------------- 소리 (SD 카드 폴더. 폴더 안의 첫 번째 .wav 를 재생) ----------------
#define SND_BOOT   "/1"     // 부팅
#define SND_MAIN   "/2"     // 속도계 화면으로 갈 때 / 대기화면으로 돌아올 때 (터치)
#define SND_LIFT   "/3"     // PWR 버튼
#define SND_B1     "/4"     // 1단 부스터 ("One", 속도계 화면에서 터치)
#define SND_B12    "/5"     // 2단 부스터: 1단 + 2단 ("Two", 1단 중에 터치)
#define SND_FULL   "/6"     // 속도계 + 1단 + 2단 ("Three", 3초 롱터치)
#define BGM_DIR_A  "/7"     // 배경음악 (오른쪽 스와이프, "Four")
#define BGM_DIR_B  "/8"     // 배경음악 (왼쪽 스와이프, "Five")

#define AUDIO_LATENCY_MS     40     // I2S 버퍼 때문에 소리가 늦게 나오는 시간
#define UI_LEAD_MS           80     // 화면이 그려져 나오기까지의 시간만큼 화면을 앞당김
#define SYNC_TRIM_MS          0     // 소리·화면 미세 조정 초기값 (시리얼 '+' / '-' 로 20ms 씩 조정, 저장됨)
#define DEFAULT_VOLUME       15     // 0 ~ 21 (위/아래 스와이프로 조정, 저장됨)
#define SR_SWAP_34            1     // Three/Four 인식 번호가 서로 바뀌어 들어오는 보드에서 1
#define ES7210_MIC_GAIN      10     // 마이크 감도 0 ~ 14

#define BGM_SWIPE_INVERT      0     // 좌우 스와이프가 반대로 동작하면 1
#define BGM_SWIPE_MIN_PX     80
#define LONG_PRESS_MS      3000
#define TOUCH_IGNORE_BOOT_MS 2500   // 켜진 직후 이 시간 동안은 터치를 무시 (부팅 때 들어오는 헛터치 방지)

// ---------------- 디스플레이 통신 속도 ----------------
//   높을수록 화면이 빨리 그려짐 (슬라이드가 부드러워짐). 화면에 점이 튀거나 깨지면 40000000 으로 낮출 것
#define LCD_SPI_HZ    40000000

// ---------------- 화면 보호 (AMOLED 번인 방지) ----------------
#define SCREEN_BRIGHT       255     // 평소 밝기 (0 ~ 255)
#define DIM_AFTER_MS      60000     // 대기화면에서 이 시간 동안 입력이 없으면 어둡게. 0 = 사용 안 함
#define DIM_LEVEL            70     // 어두울 때 밝기
#define OFF_AFTER_MS          0     // 이 시간 뒤 화면을 끔 (터치/PWR/"Hi Asurada" 로 깨움). 0 = 끄지 않음
#define SHIFT_EVERY_MS    60000     // 대기화면 구슬 위치를 조금씩 옮기는 간격. 0 = 사용 안 함
#define SHIFT_RADIUS          4     // 옮기는 범위 (px)

// ---------------- 서보 (고개 돌리기) ----------------
// SERVO_MODE
//   0 = 시간으로 제어 (원래 방식). 360도 서보는 갔다가 돌아온 자리가 조금씩 어긋납니다.
//   1 = 자이로로 각도를 재면서 제어. 360도 서보를 그대로 쓰면서 처음 자리로 돌아옵니다.
//       (보드가 머리와 함께 돌아야 합니다. 회전이 측정되지 않으면 그 동작은 자동으로 시간 방식으로 돌아옵니다)
//   2 = 각도 서보(180도 SG90 등)로 바꿨을 때. 각도를 직접 지정하므로 가장 정확합니다.
#define SERVO_MODE            1
#define SERVO_INTERVAL_MS 30000     // 동작 간격
#define SERVO_HOLD_MS      3000     // 돌아간 자리에서 머무는 시간 (눈 깜빡임)
#define SERVO_DETACH_IDLE     1     // 1 = 쉬는 동안 펄스를 끔 (360도 서보가 혼자 조금씩 도는 것을 막음)

#define SERVO_STOP_US      1500     // 360도 서보 정지 펄스
#define SERVO_RIGHT_US     1300     // (모드 0) 오른쪽으로 돌 때 펄스
#define SERVO_LEFT_US      1680     // (모드 0) 왼쪽으로 돌 때 펄스
#define SERVO_RIGHT_MS      150     // (모드 0) 오른쪽 회전 시간
#define SERVO_LEFT_MS       150     // (모드 0) 왼쪽 회전 시간

// (모드 1) 자이로로 각도를 재면서 움직임
//   이 모터(SG90 360)는 세기(정지 펄스에서 벗어난 양, us) 34~40 아래에서는 안 돌고, 돌기 시작하면 바로 초당 70도쯤으로 돕니다.
//   그래서 "20ms 돌리고 잠깐 쉬기"를 반복해 평균 속도를 낮추고, 실제 속도를 재서 SERVO_DPS 가 되도록 쉬는 시간을 스스로 맞춥니다.
//   시리얼 모니터(115200)에서 바로 바꿔 볼 수 있고, 바꾼 값은 저장되어 이 파일보다 우선합니다:
//     D25 = 속도 25도/초,  A30 = 각도 30도,  M34 = 시작 세기 34,  T = 지금 한 번 동작,  R = 이 파일의 값으로 되돌림
#define SERVO_DPS            35     // 고개 돌리는 속도 (도/초)  <- 빠르면 줄이고 느리면 늘림. 30도를 약 0.9초에
#define SERVO_ON_MS          20     // 한 번에 돌리는 시간 (서보 신호 한 주기 = 20ms)
#define SERVO_OFF_MS         30     // 처음 쉬는 시간 (동작하면서 스스로 고침)
#define SERVO_MIN_US         34     // 모터가 움직이기 시작하는 세기 (안 움직이면 자동으로 조금씩 올려서 찾음)
#define SERVO_RIGHT_SIGN     -1     // 갈 때 방향: -1 = 펄스를 줄이는 쪽(원래의 오른쪽), +1 = 반대
#define SERVO_TURN_DEG     30.0f    // 돌아갈 각도
#define SERVO_SLOW_DEG      8.0f    // 목표까지 이 각도 안으로 들어오면 더 천천히
#define SERVO_STOP_DEG      0.8f    // 목표와 이만큼 차이 나면 도착으로 봄
#define SERVO_LEAD_DEG      1.5f    // 돌아올 때 이만큼 못 미친 곳에서 멈춤 (관성으로 더 도는 만큼)
#define SERVO_NUDGE_MAX        8    // 미세 조정(짧게 밀기) 최대 횟수
#define SERVO_MAX_MS       6000     // 한 번 움직임의 최대 시간 (안전장치)
#define SERVO_MIN_DEG       3.0f    // 2.5초 동안 이보다 적게 측정되면 "보드가 같이 돌지 않는다"고 봄

#define SERVO_CENTER_DEG     90     // (모드 2) 가운데 각도
#define SERVO_SIDE_DEG       60     // (모드 2) 돌아간 각도
#define SERVO_STEP_MS        12     // (모드 2) 1도 움직이는 간격 (클수록 천천히)

// ---------------- 색 ----------------
#define COL_RED     0xF2360F
#define COL_GREEN   0x46E622
#define COL_YELLOW  0xF6E71D
#define COL_CYAN    0x19D3F3
#define COL_ARROW   0x8FB8FF
#define COL_ARROW_E 0xD2E4FF
#define COL_DIVIDER 0x280600
#define COL_GHOST   0x969696
