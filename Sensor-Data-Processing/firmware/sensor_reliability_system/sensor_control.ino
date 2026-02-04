#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

// [하드웨어 추상화 계층 - Hardware Abstraction Layer]
#define DHTPIN 13
#define DHTTYPE DHT22
#define POT_PIN 34

// [객체 생성 - Object Instantiation]
DHT dht(DHTPIN, DHTTYPE);
Servo myMotor;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// [메모리 아키텍처 - Global Memory Allocation]
const int FILTER_SIZE = 10;   // 필터의 깊이 (배열 크기)
float history[FILTER_SIZE] = {0, }; // 링 버퍼 (데이터 저장소)
int count = 0;                // 총 데이터 유입 횟수

// [진단용 변수 - Diagnostic Variables]
float lastRawVib = -1.0;      // 자가 진단용: 이전 값 기억
int stuckCounter = 0;         // 센서 고착(Stuck) 감지 카운터
int outlierCounter = 0;       // 특이점(Outlier) 감지 카운터

void setup() {
  Serial.begin(115200);
  dht.begin();
  myMotor.attach(18);
  lcd.init();
  lcd.backlight();
  
  // 초기화 완료 메시지
  Serial.println(">>> SYSTEM BOOT COMPLETE <<<");
  Serial.println(">>> ARCHITECT: GEMINI & USER <<<");
}

void loop() {
  // ==========================================
  // 1. 데이터 수집 및 전처리 (Data Acquisition)
  // ==========================================
  int potValue = analogRead(POT_PIN);
  float rawVib = (potValue / 4095.0) * 100.0;
  float rawTemp = dht.readTemperature();

  // ==========================================
  // 2. 센서 자가 진단 (Self-Diagnostics)
  // ==========================================
  // [로직] 완벽하게 똑같은 값이 50번 반복되면 센서가 죽은 것(Stuck)으로 판단
  if (rawVib == lastRawVib) {
    stuckCounter++;
  } else {
    stuckCounter = 0; // 값이 변하면 카운터 리셋 (살아있음)
  }
  lastRawVib = rawVib; // 현재 값을 '과거'로 저장

  // ==========================================
  // 3. 특이점 감지 (Outlier Detection)
  // ==========================================
  // [로직] 95% 이상의 충격이 5번 이상 누적되면 기계적 결함 의심
  if (rawVib >= 95.0) {
    outlierCounter++;
  } else if (outlierCounter > 0) {
    outlierCounter--; // 정상일 때는 카운트를 서서히 줄여줌 (Leaky Bucket)
  }

  // ==========================================
  // 4. 필터링 알고리즘 (Ring Buffer & Moving Average)
  // ==========================================
  // [포인터 개념] 배열의 인덱스를 순환시켜 메모리 효율 극대화
  int index = count % FILTER_SIZE; 
  history[index] = rawVib; 
  count++;

  float sum = 0;
  // [포인터 개념] history라는 주소에서 0~9칸 떨어진 값들을 가져옴
  for (int i = 0; i < FILTER_SIZE; i++) {
    sum += history[i];
  }

  // [동적 분모] 초기 데이터 부족 시, 현재 개수만큼만 나눔
  int divisor = (count < FILTER_SIZE) ? count : FILTER_SIZE;
  float filteredVib = sum / divisor;

  // ==========================================
  // 5. 최종 판단 및 제어 (Decision Logic)
  // ==========================================
  String status = "NORMAL";
  int motorAngle = 180;

  // [우선순위 1: 센서 고장]
  if (stuckCounter >= 50) {
    status = "ERR: SENSOR DIED";
    motorAngle = 90; // 안전 모드
  }
  // [우선순위 2: 비상 정지 (특이점 누적 OR 과열 OR 과진동)]
  else if (outlierCounter >= 5 || rawTemp >= 60.0 || filteredVib >= 80.0) {
    status = "EMERGENCY STOP";
    motorAngle = 0;
  }
  // [우선순위 3: 경고]
  else if (filteredVib >= 50.0 || rawTemp >= 45.0) {
    status = "WARNING";
    motorAngle = 90;
  }

  // ==========================================
  // 6. 액추에이터 실행 (Actuation)
  // ==========================================
  myMotor.write(motorAngle);

  // 정보 표시
  lcd.setCursor(0, 0);
  lcd.print("V:" + String(filteredVib, 0) + "% T:" + String(rawTemp, 0) + "C");
  lcd.setCursor(0, 1);
  lcd.print(status);

  // 엔지니어링 로그 (디버깅용)
  Serial.print("Raw:"); Serial.print(rawVib);
  Serial.print(" Filtered:"); Serial.print(filteredVib);
  Serial.print(" StuckCnt:"); Serial.print(stuckCounter);
  Serial.print(" OutlierCnt:"); Serial.println(outlierCounter);

  delay(100); 
}