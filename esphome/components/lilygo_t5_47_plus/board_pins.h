#pragma once

#ifdef __cplusplus
namespace esphome {
namespace lilygo_t5_47_plus {
extern "C" {
#endif

#if defined(CONFIG_IDF_TARGET_ESP32)

static const int BUTTON_1 = 34;
static const int BUTTON_2 = 35;
static const int BUTTON_3 = 39;

static const int BATT_PIN = 36;

static const int SD_MISO = 12;
static const int SD_MOSI = 13;
static const int SD_SCLK = 14;
static const int SD_CS = 15;

static const int BOARD_SCL = 14;
static const int BOARD_SDA = 15;
static const int TOUCH_INT = 13;

static const int GPIO_MISO = 12;
static const int GPIO_MOSI = 13;
static const int GPIO_SCLK = 14;
static const int GPIO_CS = 15;

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

static const int BUTTON_1 = 21;

static const int BATT_PIN = 14;

static const int SD_MISO = 16;
static const int SD_MOSI = 15;
static const int SD_SCLK = 11;
static const int SD_CS = 42;

static const int BOARD_SCL = 17;
static const int BOARD_SDA = 18;
static const int TOUCH_INT = 47;

static const int GPIO_MISO = 45;
static const int GPIO_MOSI = 10;
static const int GPIO_SCLK = 48;
static const int GPIO_CS = 39;

#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace lilygo_t5_47_plus
}  // namespace esphome
#endif
