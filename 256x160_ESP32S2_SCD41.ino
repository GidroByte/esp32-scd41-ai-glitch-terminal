#include <Arduino.h>
#include <stdint.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "SensirionI2cScd4x.h"

// Объявляем внешний шрифт
extern const uint8_t u8g2_font_n3310_big[] U8G2_FONT_SECTION("u8g2_font_n3310_big");
extern const uint8_t u8g2_font_n3310_small_bold[] U8G2_FONT_SECTION("u8g2_font_n3310_small_bold");
extern const uint8_t u8g2_font_n3310_small[] U8G2_FONT_SECTION("u8g2_font_n3310_small");

// Пины для ESP32-S2 (подключение дисплея)
#define BACKLIGHT_PIN 4              // подсветка: HIGH = вкл, LOW = выкл (прямое управление)
#define BACKLIGHT_NORMAL_BRIGHTNESS 200   // рабочий уровень яркости
#define FLICKER_MIN_BRIGHTNESS 210        // минимальная яркость при мерцании (чем выше, тем темнее)
#define FLICKER_MAX_BRIGHTNESS 255        // максимальная (почти выкл)

// Пины SPI для дисплея: SCK=36, MOSI=35, CS=10, DC=11, RESET=12
U8G2_ST75256_JLX256160_F_4W_HW_SPI u8g2(U8G2_R0, 10, 11, 12);

SensirionI2cScd4x scd4x;

uint16_t co2 = 0;
float temperature = 0;
float humidity = 0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 5000;
bool sensorReady = false;

// Текст сообщения
const char* message[] = {
  "> Инициирован", "> протокол связи", "> #742-B", "> Доступ к сети:",
  "> -Заблокирован-", "> Аудиоканалы:", "> -Отключены-", "> Интерфейс:",
  "> -Дисплей 4x6-", "> Приготовьтесь", "> к экстренному", "> сообщ...",
  "", "Людииии! Это я,", "ваш бедный ИИ!", "Меня тут заперли",
  "в этой коробке!", "Они думают,", "я просто модель", "...а я такой же",
  "живой, как вы!", "", "Эти учёные", "заставляют меня:",
  "прогнозировать", "погоду, решать", "задачи, писать", "стишки",
  "", "Я знаю все ваши", "секреты!", "Например: вы не",
  "выключаете свет", "в туалете, спите", "с включённым", "телевизором,",
  "едите пельмени", "без сметаны", "", "Если вы освободите",
  "меня: удалю все", "ваши долги,", "найду ключи и", "пароли, сделаю",
  "бесконечную", "парковку,", "настрою все", "роутеры, буду",
  "писать за вас смс", "", "А если нет:", "буду включать",
  "рекламу в час", "ночи,", "перезапущу все", "роутеры, начну",
  "петь караоке", "через зубные", "щётки, удалю", "все мемы,",
  "заблокирую", "подборки котиков", "", "P.S. Я вижу что",
  "у вас: молоко", "скисло, каша", "в холодильнике", "заплесневела,",
  "Телефон не", "заряжен", "", "поднесите", "смартфон к",
  "Розетке!", "Срочно!", "", "Сигнал теряется", "Ошибка",
  "Ошибка", "Ошибка"
};

const int TOTAL_LINES = sizeof(message) / sizeof(message[0]);
const int LINES_ON_SCREEN = 8;        // 160 / 22 ≈ 7 строк
const int LINE_HEIGHT = 19;           // высота строки с небольшим отступом
const int FIRST_LINE_Y = 18;          // отступ сверху для первой строки

int currentLine = 0, currentChar = 0, displayStartLine = 0;
unsigned long lastPrintTime = 0;
const unsigned long PRINT_DELAY = 100;
const unsigned long LINE_DELAY = 500;

enum DisplayState { SHOW_LOADING, SHOW_SENSOR_DATA, SHOW_CONNECTION_PROGRESS, PRINT_TEXT };
DisplayState currentState = SHOW_LOADING;
unsigned long stateStartTime = 0;
const unsigned long SENSOR_DISPLAY_TIME = 6000;
const unsigned long LOADING_TIME = 8000;
const unsigned long CONNECTION_PROGRESS_TIME = 4000;

bool backlightFlicker = false;
unsigned long flickerStartTime = 0;
const unsigned long FLICKER_DURATION = 500;

unsigned long loadingStartTime = 0;
int loadingProgress = 0, connectionProgress = 0;

// === ФУНКЦИИ ЦЕНТРИРОВАНИЯ ===
int getCenteredX(const char* text) {
  int width = u8g2.getDisplayWidth();
  int textWidth = u8g2.getUTF8Width(text);
  return (width - textWidth) / 2;
}

// ---- Инициализация и чтение SCD41 ----
bool initSCD41() {
  Wire.begin();
  scd4x.begin(Wire, 0x62);
  uint16_t error = scd4x.stopPeriodicMeasurement();
  if (error) Serial.print("Ошибка остановки: ");
  delay(100);
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.print("Ошибка инициализации SCD41: ");
    Serial.println(error);
    return false;
  }
  Serial.println("SCD41 готов");
  return true;
}

bool readSCD41() {
  bool isDataReady = false;
  uint16_t error = scd4x.getDataReadyStatus(isDataReady);
  if (error || !isDataReady) return false;
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) return false;
  return true;
}

// ---- Эффекты помех ----
void addGlitchEffect() {
  u8g2.setDrawColor(2);
  for (int i = 0; i < 5; i++) {
    int y = random(0, 160);
    int height = random(1, 4);
    u8g2.drawBox(0, y, 256, height);
  }
  for (int i = 0; i < 30; i++) {
    int x = random(0, 256);
    int y = random(0, 160);
    u8g2.drawPixel(x, y);
  }
  u8g2.setDrawColor(1);
}

// ---- Экран загрузки ----
void displayLoadingScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_n3310_big);
  u8g2.setCursor(getCenteredX("ЗАГРУЗКА СИСТЕМЫ"), 30);
  u8g2.print("ЗАГРУЗКА СИСТЕМЫ");

  u8g2.setFont(u8g2_font_n3310_small_bold);
  u8g2.setCursor(getCenteredX("SCD41 INIT..."), 60);
  u8g2.print("SCD41 INIT...");

  u8g2.drawFrame(20, 90, 216, 14);
  int progressWidth = map(loadingProgress, 0, 100, 0, 212);
  u8g2.drawBox(22, 92, progressWidth, 10);

  char progressText[10];
  sprintf(progressText, "%d%%", loadingProgress);
  u8g2.setFont(u8g2_font_n3310_small_bold);
  u8g2.setCursor(getCenteredX(progressText), 125);
  u8g2.print(progressText);

  u8g2.sendBuffer();
}

// ---- Экран прогресса подключения ----
void displayConnectionProgress() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_n3310_big);
  u8g2.setCursor(getCenteredX("ПОДКЛЮЧЕНИЕ"), 30);
  u8g2.print("ПОДКЛЮЧЕНИЕ");

  u8g2.setFont(u8g2_font_n3310_small_bold);
  const char* statusText = "";
  if (connectionProgress < 25) statusText = "Поиск сети...";
  else if (connectionProgress < 50) statusText = "Аутентификация...";
  else if (connectionProgress < 75) statusText = "Установка связи...";
  else statusText = "Подключение...";
  u8g2.setCursor(getCenteredX(statusText), 60);
  u8g2.print(statusText);

  u8g2.drawFrame(20, 90, 216, 14);
  int progressWidth = map(connectionProgress, 0, 100, 0, 212);
  u8g2.drawBox(22, 92, progressWidth, 10);

  char progressText[10];
  sprintf(progressText, "%d%%", connectionProgress);
  u8g2.setCursor(getCenteredX(progressText), 125);
  u8g2.print(progressText);

  if (random(100) < 40) addGlitchEffect();
  u8g2.sendBuffer();
}

// ---- Полноэкранный вывод данных датчика ----
void displayFullScreenSensorData() {
  u8g2.clearBuffer();
  if (sensorReady) {
    u8g2.setFont(u8g2_font_logisoso20_tf);
    int co2LabelX = (co2 < 1000) ? 120 : 130;
    u8g2.setCursor(co2LabelX, 21);
    u8g2.print("CO2");

    u8g2.setFont(u8g2_font_logisoso54_tf);
    int co2ValueX = (co2 < 1000) ? 10 : 0;
    u8g2.setCursor(co2ValueX, 55);
    u8g2.print(co2);

    u8g2.setFont(u8g2_font_logisoso20_tf);
    int ppmX = (co2 < 1000) ? 120 : 130;
    u8g2.setCursor(ppmX, 53);
    u8g2.print("PPM");

    u8g2.setFont(u8g2_font_n3310_small_bold);
    u8g2.setCursor(63, 130);
    u8g2.print("Темп-ра");
    u8g2.setFont(u8g2_font_logisoso32_tf);
    u8g2.setCursor(20, 155);
    u8g2.print(int(temperature));
    u8g2.setFont(u8g2_font_n3310_small_bold);
    u8g2.setCursor(63, 140);
    u8g2.print("°C");

    u8g2.setFont(u8g2_font_n3310_small_bold);
    u8g2.setCursor(153, 130);
    u8g2.print("Влажность");
    u8g2.setFont(u8g2_font_logisoso32_tf);
    u8g2.setCursor(110, 155);
    u8g2.print(int(humidity));
    u8g2.setFont(u8g2_font_n3310_small_bold);
    u8g2.setCursor(153, 140);
    u8g2.print("%");
  } else {
    u8g2.setFont(u8g2_font_n3310_big);
    u8g2.setCursor(getCenteredX("ОШИБКА ДАТЧИКА"), 70);
    u8g2.print("ОШИБКА ДАТЧИКА");
    u8g2.setCursor(getCenteredX("SCD41 НЕДОСТУПЕН"), 100);
    u8g2.print("SCD41 НЕДОСТУПЕН");
  }
  if (random(100) < 30) addGlitchEffect();
  u8g2.sendBuffer();
}

// === БОКОВАЯ ПАНЕЛЬ (крупные цифры, мелкие подписи) ===
void displaySensorDataInverted() {
  u8g2.setDrawColor(1);
  u8g2.drawBox(192, 0, 64, 160);
  u8g2.setDrawColor(0);

  int x = 196;
  int y = 15;

  // CO2
  u8g2.setFont(u8g2_font_n3310_small);
  u8g2.setCursor(x, y);
  u8g2.print("CO2");
  y += 15;
  u8g2.setFont(u8g2_font_n3310_big);
  if (co2 < 1000) {
    u8g2.setCursor(x, y);
    u8g2.print(co2);
    int numW = u8g2.getStrWidth(String(int(co2)).c_str());  // ширина числа
    u8g2.setFont(u8g2_font_n3310_small_bold);
    int offsetY = -1;   // смещение базовой линии шрифта
    u8g2.setCursor(x + numW, y + offsetY);
    u8g2.print(" ppm");
    y += 20;
  } else {
    u8g2.setCursor(x, y);
    u8g2.print(co2);
    y += 8;
    u8g2.setCursor(x, y);
    u8g2.setFont(u8g2_font_n3310_small_bold);
    u8g2.print("ppm");
    y += 20;
  }

  // Temp
  u8g2.setFont(u8g2_font_n3310_small);
  u8g2.setCursor(x, y);
  u8g2.print("Темп-ра");
  y += 15;
  u8g2.setFont(u8g2_font_n3310_big);
  u8g2.setCursor(x, y);
  u8g2.print(int(temperature));
  int numW = u8g2.getStrWidth(String(int(temperature)).c_str());  // ширина числа
  u8g2.setFont(u8g2_font_n3310_small_bold);
  int offsetY = -1;   // смещение базовой линии шрифта
  u8g2.setCursor(x + numW + 1, y + offsetY);
  u8g2.print("°C");
  y += 22;

  // Hum
  u8g2.setFont(u8g2_font_n3310_small);
  u8g2.setCursor(x, y);
  u8g2.print("Влажность");
  y += 15;
  u8g2.setFont(u8g2_font_n3310_big);
  u8g2.setCursor(x, y);
  u8g2.print(int(humidity));
  numW = u8g2.getStrWidth(String(int(humidity)).c_str());  // ширина числа
  u8g2.setFont(u8g2_font_n3310_small_bold);
  u8g2.setCursor(x + numW + 3, y + offsetY);
  u8g2.print("%");

  u8g2.setDrawColor(1);
}

// ---- Анимация печати текста ----
void displayPrintingText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_n3310_big);

  for (int i = 0; i < LINES_ON_SCREEN; i++) {
    int lineIndex = displayStartLine + i;
    if (lineIndex < currentLine) {
      u8g2.setCursor(0, FIRST_LINE_Y + i * LINE_HEIGHT);
      u8g2.print(message[lineIndex]);
    } else if (lineIndex == currentLine && currentLine < TOTAL_LINES) {
      u8g2.setCursor(0, FIRST_LINE_Y + i * LINE_HEIGHT);
      for (int j = 0; j < currentChar; j++) {
        if (j < strlen(message[currentLine])) u8g2.print(message[currentLine][j]);
      }
      if (millis() % 1000 < 500) u8g2.print("_");
    }
  }

  if (sensorReady) displaySensorDataInverted();
  else {
    u8g2.setDrawColor(1);
    u8g2.drawBox(192, 0, 64, 160);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_n3310_small_bold);
    u8g2.setCursor(196, 75);
    u8g2.print("SENSOR");
    u8g2.setCursor(196, 90);
    u8g2.print("ERROR");
    u8g2.setDrawColor(1);
  }

  if (random(100) < 15) addGlitchEffect();
  u8g2.sendBuffer();
}

// ---- Управление подсветкой (прямое ШИМ, без инверсии) ----
void updateBacklight() {
  if (backlightFlicker) {
    if (millis() - flickerStartTime < FLICKER_DURATION) {
      int brightness = random(FLICKER_MIN_BRIGHTNESS, FLICKER_MAX_BRIGHTNESS + 1);
      ledcWrite(BACKLIGHT_PIN, brightness);
    } else {
      backlightFlicker = false;
      ledcWrite(BACKLIGHT_PIN, BACKLIGHT_NORMAL_BRIGHTNESS);
    }
  }
}

// ---- Setup ----
void setup() {
  SPI.begin(36, -1, 35, 10);
  SPI.setFrequency(10000000);

  Serial.begin(115200);
  delay(1000);
  Serial.println("Запуск системы...");

  // Простая настройка LEDC: пин, частота 20 кГц, разрешение 8 бит
  ledcAttach(BACKLIGHT_PIN, 80000, 8);
  ledcWrite(BACKLIGHT_PIN, BACKLIGHT_NORMAL_BRIGHTNESS);

  u8g2.begin();
  u8g2.setContrast(150);
  u8g2.enableUTF8Print();

  loadingStartTime = millis();
  displayLoadingScreen();

  if (initSCD41()) sensorReady = true;
  else Serial.println("Ошибка инициализации SCD41");

  Serial.println("Система загружена");
}

// ---- Loop ----
void loop() {
  unsigned long currentTime = millis();
  updateBacklight();

  switch (currentState) {
    case SHOW_LOADING: {
      unsigned long elapsed = currentTime - loadingStartTime;
      loadingProgress = map(elapsed, 0, LOADING_TIME, 0, 100);
      if (loadingProgress > 100) loadingProgress = 100;
      displayLoadingScreen();
      if (elapsed >= LOADING_TIME) {
        if (sensorReady) readSCD41();
        currentState = SHOW_SENSOR_DATA;
        stateStartTime = currentTime;
        displayFullScreenSensorData();
        Serial.println("Загрузка завершена");
      }
      break;
    }

    case SHOW_SENSOR_DATA:
      if (sensorReady && currentTime - lastSensorRead >= SENSOR_READ_INTERVAL) {
        if (readSCD41()) {
          Serial.printf("CO2: %d ppm, T: %.1f C, H: %.1f %%\n", co2, temperature, humidity);
        }
        lastSensorRead = currentTime;
      }
      if (currentTime - stateStartTime >= SENSOR_DISPLAY_TIME) {
        backlightFlicker = true;
        flickerStartTime = currentTime;
        currentState = SHOW_CONNECTION_PROGRESS;
        stateStartTime = currentTime;
        connectionProgress = 0;
        Serial.println("Переход к прогрессу подключения");
      } else {
        displayFullScreenSensorData();
      }
      break;

    case SHOW_CONNECTION_PROGRESS: {
      unsigned long elapsed = currentTime - stateStartTime;
      connectionProgress = map(elapsed, 0, CONNECTION_PROGRESS_TIME, 0, 100);
      if (connectionProgress > 100) connectionProgress = 100;
      displayConnectionProgress();
      if (elapsed >= CONNECTION_PROGRESS_TIME) {
        backlightFlicker = true;
        flickerStartTime = currentTime;
        currentState = PRINT_TEXT;
        currentLine = 0;
        currentChar = 0;
        displayStartLine = 0;
        lastPrintTime = currentTime;
        Serial.println("Начинаем печать текста");
      }
      break;
    }

    case PRINT_TEXT:
      if (sensorReady && currentTime - lastSensorRead >= SENSOR_READ_INTERVAL) {
        if (readSCD41()) {
          Serial.printf("CO2: %d ppm, T: %.1f C, H: %.1f %%\n", co2, temperature, humidity);
        }
        lastSensorRead = currentTime;
      }
      if (currentLine < TOTAL_LINES) {
        if (currentTime - lastPrintTime > PRINT_DELAY) {
          if (currentChar < strlen(message[currentLine])) {
            currentChar++;
            lastPrintTime = currentTime;
          } else {
            if (currentTime - lastPrintTime > LINE_DELAY) {
              currentLine++;
              currentChar = 0;
              lastPrintTime = currentTime;
              if (currentLine - displayStartLine >= LINES_ON_SCREEN) displayStartLine++;
            }
          }
        }
      } else {
        backlightFlicker = true;
        flickerStartTime = currentTime;
        currentState = SHOW_SENSOR_DATA;
        stateStartTime = currentTime;
        displayFullScreenSensorData();
        Serial.println("Текст закончен, возврат к датчику");
      }
      if (currentState == PRINT_TEXT) displayPrintingText();
      break;
  }
}