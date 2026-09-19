# Сбежавший ИИ — ESP32-S2 · SCD41 · Nokia 3310 Glitch Terminal

Проект «Сбежавший ИИ» на **ESP32-S2** с датчиком CO₂ **SCD41** и дисплеем **256×160** (контроллер ST75256), 
отображающий уровень CO₂, температуру и влажность в стиле «глитч-терминала» шрифтом, 
стилизованным под **Nokia 3310**.

---

## Аппаратное обеспечение

| Компонент | Модель / примечание |
|-----------|---------------------|
| Микроконтроллер | ESP32-S2 |
| Датчик | Sensirion SCD41 (CO₂, температура, влажность), I²C |
| Дисплей | 256×160, контроллер ST75256 (4-wire SPI) |
| Шрифт | [nokia-3310-fonts-u8g2](https://github.com/GidroByte/nokia-3310-fonts-u8g2) — small / small-bold / big |

---

## Зависимости

Библиотеки Arduino (устанавливаются через **Library Manager** или вручную):

- **U8g2** (автор olikraus) — драйвер дисплея
- **SensirionI2cScd4x** — работа с датчиком SCD41
- **Arduino ESP32 Core** — поддержка ESP32-S2 (Boards Manager → esp32 by Espressif)
- **Шрифт nokia-3310-fonts-u8g2** — подключается как `.c`-файлы в проект

---

## Структура проекта
256x160_ESP32S2_SCD41/
├── 256x160_ESP32S2_SCD41.ino # основной скетч
├── n3310_big.c # крупный шрифт
├── n3310_small.c # мелкий шрифт
├── n3310_small_bold.c # мелкий жирный
├── README.md
└── LICENSE

text

---

## Быстрый старт

### 1. Подготовка шрифтов

Скачайте шрифты из репозитория  
[nokia-3310-fonts-u8g2](https://github.com/GidroByte/nokia-3310-fonts-u8g2)  
и добавьте `.c`-файлы в проект:

> В Arduino IDE: **Скетч → Добавить файл…** → выберите `n3310_big.c` (и остальные).  
> Файлы появятся отдельными вкладками.

### 2. Подключение шрифта в скетче

```cpp
#include <Arduino.h>
#include <U8g2lib.h>

// Объявление внешнего шрифта
extern const uint8_t u8g2_font_n3310_big[] U8G2_FONT_SECTION("u8g2_font_n3310_big");

// Конструктор под ваш дисплей — при необходимости замените на свой
U8G2_ST75256_JLX256160_F_4W_HW_SPI u8g2(U8G2_R0, /* CS */ 10, /* DC */ 11, /* RST */ 12);

void setup() {
  u8g2.begin();
  u8g2.enableUTF8Print();   // обязательно для кириллицы
  u8g2.setContrast(150);    // яркость/контрастность (0–255)
}

void loop() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_n3310_big);
    u8g2.setCursor(0, 20);
    u8g2.print("Привет, мир!");
  } while (u8g2.nextPage());
}