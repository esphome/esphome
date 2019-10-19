from __future__ import division

import logging

import esphome.config_validation as cv
from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER
from esphome.core import CORE
from esphome.util import SimpleRegistry

_LOGGER = logging.getLogger(__name__)

ESP8266_BASE_PINS = {
    'A0': 17, 'SS': 15, 'MOSI': 13, 'MISO': 12, 'SCK': 14, 'SDA': 4, 'SCL': 5, 'RX': 3, 'TX': 1
}

ESP8266_BOARD_PINS = {
    'd1': {'D0': 3, 'D1': 1, 'D2': 16, 'D3': 5, 'D4': 4, 'D5': 14, 'D6': 12, 'D7': 13, 'D8': 0,
           'D9': 2, 'D10': 15, 'D11': 13, 'D12': 14, 'D13': 14, 'D14': 4, 'D15': 5, 'LED': 2},
    'd1_mini': {'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13,
                'D8': 15, 'LED': 2},
    'd1_mini_lite': 'd1_mini',
    'd1_mini_pro': 'd1_mini',
    'esp01': {},
    'esp01_1m': {},
    'esp07': {},
    'esp12e': {},
    'esp210': {},
    'esp8285': {},
    'esp_wroom_02': {},
    'espduino': {'LED': 16},
    'espectro': {'LED': 15, 'BUTTON': 2},
    'espino': {'LED': 2, 'LED_RED': 2, 'LED_GREEN': 4, 'LED_BLUE': 5, 'BUTTON': 0},
    'espinotee': {'LED': 16},
    'espresso_lite_v1': {'LED': 16},
    'espresso_lite_v2': {'LED': 2},
    'gen4iod': {},
    'heltec_wifi_kit_8': 'd1_mini',
    'huzzah': {'LED': 0},
    'inventone': {},
    'modwifi': {},
    'nodemcu': {'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13,
                'D8': 15, 'D9': 3, 'D10': 1, 'LED': 16},
    'nodemcuv2': 'nodemcu',
    'oak': {'P0': 2, 'P1': 5, 'P2': 0, 'P3': 3, 'P4': 1, 'P5': 4, 'P6': 15, 'P7': 13, 'P8': 12,
            'P9': 14, 'P10': 16, 'P11': 17, 'LED': 5},
    'phoenix_v1': {'LED': 16},
    'phoenix_v2': {'LED': 2},
    'sparkfunBlynk': 'thing',
    'thing': {'LED': 5, 'SDA': 2, 'SCL': 14},
    'thingdev': 'thing',
    'wifi_slot': {'LED': 2},
    'wifiduino': {'D0': 3, 'D1': 1, 'D2': 2, 'D3': 0, 'D4': 4, 'D5': 5, 'D6': 16, 'D7': 14,
                  'D8': 12, 'D9': 13, 'D10': 15, 'D11': 13, 'D12': 12, 'D13': 14},
    'wifinfo': {'LED': 12, 'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12,
                'D7': 13, 'D8': 15, 'D9': 3, 'D10': 1},
    'wio_link': {'LED': 2, 'GROVE': 15, 'D0': 14, 'D1': 12, 'D2': 13, 'BUTTON': 0},
    'wio_node': {'LED': 2, 'GROVE': 15, 'D0': 3, 'D1': 5, 'BUTTON': 0},
    'xinabox_cw01': {'SDA': 2, 'SCL': 14, 'LED': 5, 'LED_RED': 12, 'LED_GREEN': 13}
}

FLASH_SIZE_1_MB = 2 ** 20
FLASH_SIZE_512_KB = FLASH_SIZE_1_MB // 2
FLASH_SIZE_2_MB = 2 * FLASH_SIZE_1_MB
FLASH_SIZE_4_MB = 4 * FLASH_SIZE_1_MB
FLASH_SIZE_16_MB = 16 * FLASH_SIZE_1_MB

ESP8266_FLASH_SIZES = {
    'd1': FLASH_SIZE_4_MB,
    'd1_mini': FLASH_SIZE_4_MB,
    'd1_mini_lite': FLASH_SIZE_1_MB,
    'd1_mini_pro': FLASH_SIZE_16_MB,
    'esp01': FLASH_SIZE_512_KB,
    'esp01_1m': FLASH_SIZE_1_MB,
    'esp07': FLASH_SIZE_4_MB,
    'esp12e': FLASH_SIZE_4_MB,
    'esp210': FLASH_SIZE_4_MB,
    'esp8285': FLASH_SIZE_1_MB,
    'esp_wroom_02': FLASH_SIZE_2_MB,
    'espduino': FLASH_SIZE_4_MB,
    'espectro': FLASH_SIZE_4_MB,
    'espino': FLASH_SIZE_4_MB,
    'espinotee': FLASH_SIZE_4_MB,
    'espresso_lite_v1': FLASH_SIZE_4_MB,
    'espresso_lite_v2': FLASH_SIZE_4_MB,
    'gen4iod': FLASH_SIZE_512_KB,
    'heltec_wifi_kit_8': FLASH_SIZE_4_MB,
    'huzzah': FLASH_SIZE_4_MB,
    'inventone': FLASH_SIZE_4_MB,
    'modwifi': FLASH_SIZE_2_MB,
    'nodemcu': FLASH_SIZE_4_MB,
    'nodemcuv2': FLASH_SIZE_4_MB,
    'oak': FLASH_SIZE_4_MB,
    'phoenix_v1': FLASH_SIZE_4_MB,
    'phoenix_v2': FLASH_SIZE_4_MB,
    'sparkfunBlynk': FLASH_SIZE_4_MB,
    'thing': FLASH_SIZE_512_KB,
    'thingdev': FLASH_SIZE_512_KB,
    'wifi_slot': FLASH_SIZE_1_MB,
    'wifiduino': FLASH_SIZE_4_MB,
    'wifinfo': FLASH_SIZE_1_MB,
    'wio_link': FLASH_SIZE_4_MB,
    'wio_node': FLASH_SIZE_4_MB,
    'xinabox_cw01': FLASH_SIZE_4_MB,
}

ESP8266_LD_SCRIPTS = {
    FLASH_SIZE_512_KB: ('eagle.flash.512k0.ld', 'eagle.flash.512k.ld'),
    FLASH_SIZE_1_MB: ('eagle.flash.1m0.ld', 'eagle.flash.1m.ld'),
    FLASH_SIZE_2_MB: ('eagle.flash.2m.ld', 'eagle.flash.2m.ld'),
    FLASH_SIZE_4_MB: ('eagle.flash.4m.ld', 'eagle.flash.4m.ld'),
    FLASH_SIZE_16_MB: ('eagle.flash.16m.ld', 'eagle.flash.16m14m.ld'),
}

ESP32_BASE_PINS = {
    'TX': 1, 'RX': 3, 'SDA': 21, 'SCL': 22, 'SS': 5, 'MOSI': 23, 'MISO': 19, 'SCK': 18, 'A0': 36,
    'A3': 39, 'A4': 32, 'A5': 33, 'A6': 34, 'A7': 35, 'A10': 4, 'A11': 0, 'A12': 2, 'A13': 15,
    'A14': 13, 'A15': 12, 'A16': 14, 'A17': 27, 'A18': 25, 'A19': 26, 'T0': 4, 'T1': 0, 'T2': 2,
    'T3': 15, 'T4': 13, 'T5': 12, 'T6': 14, 'T7': 27, 'T8': 33, 'T9': 32, 'DAC1': 25, 'DAC2': 26,
    'SVP': 36, 'SVN': 39,
}

ESP32_BOARD_PINS = {
    'alksesp32': {'A0': 32, 'A1': 33, 'A2': 25, 'A3': 26, 'A4': 27, 'A5': 14, 'A6': 12, 'A7': 15,
                  'D0': 40, 'D1': 41, 'D10': 19, 'D11': 21, 'D12': 22, 'D13': 23, 'D2': 15,
                  'D3': 2, 'D4': 0, 'D5': 4, 'D6': 16, 'D7': 17, 'D8': 5, 'D9': 18, 'DHT_PIN': 26,
                  'LED': 23, 'L_B': 5, 'L_G': 17, 'L_R': 22, 'L_RGB_B': 16, 'L_RGB_G': 21,
                  'L_RGB_R': 4, 'L_Y': 23, 'MISO': 22, 'MOSI': 21, 'PHOTO': 25, 'PIEZO1': 19,
                  'PIEZO2': 18, 'POT1': 32, 'POT2': 33, 'S1': 4, 'S2': 16, 'S3': 18, 'S4': 19,
                  'S5': 21, 'SCK': 23, 'SCL': 14, 'SDA': 27, 'SS': 19, 'SW1': 15, 'SW2': 2,
                  'SW3': 0},
    'bpi-bit': {'BUTTON_A': 35, 'BUTTON_B': 27, 'BUZZER': 25, 'LIGHT_SENSOR1': 36,
                'LIGHT_SENSOR2': 39, 'MPU9250_INT': 0, 'P0': 25, 'P1': 32, 'P10': 26, 'P11': 27,
                'P12': 2, 'P13': 18, 'P14': 19, 'P15': 23, 'P16': 5, 'P19': 22, 'P2': 33,
                'P20': 21, 'P3': 13, 'P4': 15, 'P5': 35, 'P6': 12, 'P7': 14, 'P8': 16, 'P9': 17,
                'RGB_LED': 4, 'TEMPERATURE_SENSOR': 34},
    'd-duino-32': {'D1': 5, 'D10': 1, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13,
                   'D8': 15, 'D9': 3, 'MISO': 12, 'MOSI': 13, 'SCK': 14, 'SCL': 4, 'SDA': 5,
                   'SS': 15},
    'esp-wrover-kit': {},
    'esp32-devkitlipo': {},
    'esp32-evb': {'BUTTON': 34, 'MISO': 15, 'MOSI': 2, 'SCK': 14, 'SCL': 16, 'SDA': 13, 'SS': 17},
    'esp32-gateway': {'BUTTON': 34, 'LED': 33, 'SCL': 16, 'SDA': 32},
    'esp32-poe-iso': {'BUTTON': 34, 'MISO': 15, 'MOSI': 2, 'SCK': 14, 'SCL': 16, 'SDA': 13},
    'esp32-poe': {'BUTTON': 34, 'MISO': 15, 'MOSI': 2, 'SCK': 14, 'SCL': 16, 'SDA': 13},
    'esp32-pro': {'BUTTON': 34, 'MISO': 15, 'MOSI': 2, 'SCK': 14, 'SCL': 16, 'SDA': 13, 'SS': 17},
    'esp320': {'LED': 5, 'MISO': 12, 'MOSI': 13, 'SCK': 14, 'SCL': 14, 'SDA': 2, 'SS': 15},
    'esp32cam': {},
    'esp32dev': {},
    'esp32doit-devkit-v1': {'LED': 2},
    'esp32thing': {'BUTTON': 0, 'LED': 5, 'SS': 2},
    'esp32vn-iot-uno': {},
    'espea32': {'BUTTON': 0, 'LED': 5},
    'espectro32': {'LED': 15, 'SD_SS': 33},
    'espino32': {'BUTTON': 0, 'LED': 16},
    'featheresp32': {'A0': 26, 'A1': 25, 'A10': 27, 'A11': 12, 'A12': 13, 'A13': 35, 'A2': 34,
                     'A4': 36, 'A5': 4, 'A6': 14, 'A7': 32, 'A8': 15, 'A9': 33, 'Ax': 2,
                     'LED': 13, 'MOSI': 18, 'RX': 16, 'SCK': 5, 'SDA': 23, 'SS': 33, 'TX': 17},
    'firebeetle32': {'LED': 2},
    'fm-devkit': {'D0': 34, 'D1': 35, 'D10': 0, 'D2': 32, 'D3': 33, 'D4': 27, 'D5': 14, 'D6': 12,
                  'D7': 13, 'D8': 15, 'D9': 23, 'I2S_DOUT': 22, 'I2S_LRCLK': 25, 'I2S_MCLK': 2,
                  'I2S_SCLK': 26, 'LED': 5, 'SCL': 17, 'SDA': 16, 'SW1': 4, 'SW2': 18, 'SW3': 19,
                  'SW4': 21},
    'frogboard': {},
    'heltec_wifi_kit_32': {'A1': 37, 'A2': 38, 'BUTTON': 0, 'LED': 25, 'RST_OLED': 16,
                           'SCL_OLED': 15, 'SDA_OLED': 4, 'Vext': 21},
    'heltec_wifi_lora_32': {'BUTTON': 0, 'DIO0': 26, 'DIO1': 33, 'DIO2': 32, 'LED': 25,
                            'MOSI': 27, 'RST_LoRa': 14, 'RST_OLED': 16, 'SCK': 5, 'SCL_OLED': 15,
                            'SDA_OLED': 4, 'SS': 18, 'Vext': 21},
    'heltec_wifi_lora_32_V2': {'BUTTON': 0, 'DIO0': 26, 'DIO1': 35, 'DIO2': 34, 'LED': 25,
                               'MOSI': 27, 'RST_LoRa': 14, 'RST_OLED': 16, 'SCK': 5,
                               'SCL_OLED': 15, 'SDA_OLED': 4, 'SS': 18, 'Vext': 21},
    'heltec_wireless_stick': {'BUTTON': 0, 'DIO0': 26, 'DIO1': 35, 'DIO2': 34, 'LED': 25,
                              'MOSI': 27, 'RST_LoRa': 14, 'RST_OLED': 16, 'SCK': 5,
                              'SCL_OLED': 15, 'SDA_OLED': 4, 'SS': 18, 'Vext': 21},
    'hornbill32dev': {'BUTTON': 0, 'LED': 13},
    'hornbill32minima': {'SS': 2},
    'intorobot': {'A1': 39, 'A2': 35, 'A3': 25, 'A4': 26, 'A5': 14, 'A6': 12, 'A7': 15, 'A8': 13,
                  'A9': 2, 'BUTTON': 0, 'D0': 19, 'D1': 23, 'D2': 18, 'D3': 17, 'D4': 16, 'D5': 5,
                  'D6': 4, 'LED': 4, 'MISO': 17, 'MOSI': 16, 'RGB_B_BUILTIN': 22,
                  'RGB_G_BUILTIN': 21, 'RGB_R_BUILTIN': 27, 'SCL': 19, 'SDA': 23, 'T0': 19,
                  'T1': 23, 'T2': 18, 'T3': 17, 'T4': 16, 'T5': 5, 'T6': 4},
    'iotaap_magnolia': {},
    'iotbusio': {},
    'iotbusproteus': {},
    'lolin32': {'LED': 5},
    'lolin_d32': {'LED': 5, '_VBAT': 35},
    'lolin_d32_pro': {'LED': 5, '_VBAT': 35},
    'lopy': {'A1': 37, 'A2': 38, 'LED': 0, 'MISO': 37, 'MOSI': 22, 'SCK': 13, 'SCL': 13,
             'SDA': 12, 'SS': 17},
    'lopy4': {'A1': 37, 'A2': 38, 'LED': 0, 'MISO': 37, 'MOSI': 22, 'SCK': 13, 'SCL': 13,
              'SDA': 12, 'SS': 18},
    'm5stack-core-esp32': {'ADC1': 35, 'ADC2': 36, 'G0': 0, 'G1': 1, 'G12': 12, 'G13': 13,
                           'G15': 15, 'G16': 16, 'G17': 17, 'G18': 18, 'G19': 19, 'G2': 2,
                           'G21': 21, 'G22': 22, 'G23': 23, 'G25': 25, 'G26': 26, 'G3': 3,
                           'G34': 34, 'G35': 35, 'G36': 36, 'G5': 5, 'RXD2': 16, 'TXD2': 17},
    'm5stack-fire': {'ADC1': 35, 'ADC2': 36, 'G0': 0, 'G1': 1, 'G12': 12, 'G13': 13, 'G15': 15,
                     'G16': 16, 'G17': 17, 'G18': 18, 'G19': 19, 'G2': 2, 'G21': 21, 'G22': 22,
                     'G23': 23, 'G25': 25, 'G26': 26, 'G3': 3, 'G34': 34, 'G35': 35, 'G36': 36,
                     'G5': 5},
    'm5stack-grey': {'ADC1': 35, 'ADC2': 36, 'G0': 0, 'G1': 1, 'G12': 12, 'G13': 13, 'G15': 15,
                     'G16': 16, 'G17': 17, 'G18': 18, 'G19': 19, 'G2': 2, 'G21': 21, 'G22': 22,
                     'G23': 23, 'G25': 25, 'G26': 26, 'G3': 3, 'G34': 34, 'G35': 35, 'G36': 36,
                     'G5': 5, 'RXD2': 16, 'TXD2': 17},
    'm5stick-c': {'ADC1': 35, 'ADC2': 36, 'G0': 0, 'G10': 10, 'G26': 26, 'G32': 32, 'G33': 33,
                  'G36': 36, 'G37': 37, 'G39': 39, 'G9': 9, 'MISO': 36, 'MOSI': 15, 'SCK': 13,
                  'SCL': 33, 'SDA': 32},
    'magicbit': {'BLUE_LED': 17, 'BUZZER': 25, 'GREEN_LED': 16, 'LDR': 36, 'LED': 16,
                 'LEFT_BUTTON': 35, 'MOTOR1A': 27, 'MOTOR1B': 18, 'MOTOR2A': 16, 'MOTOR2B': 17,
                 'POT': 39, 'RED_LED': 27, 'RIGHT_PUTTON': 34, 'YELLOW_LED': 18},
    'mhetesp32devkit': {'LED': 2},
    'mhetesp32minikit': {'LED': 2},
    'microduino-core-esp32': {'A0': 12, 'A1': 13, 'A10': 25, 'A11': 26, 'A12': 27, 'A13': 14,
                              'A2': 15, 'A3': 4, 'A6': 38, 'A7': 37, 'A8': 32, 'A9': 33, 'D0': 3,
                              'D1': 1, 'D10': 5, 'D11': 23, 'D12': 19, 'D13': 18, 'D14': 12,
                              'D15': 13, 'D16': 15, 'D17': 4, 'D18': 22, 'D19': 21, 'D2': 16,
                              'D20': 38, 'D21': 37, 'D3': 17, 'D4': 32, 'D5': 33, 'D6': 25,
                              'D7': 26, 'D8': 27, 'D9': 14, 'SCL': 21, 'SCL1': 13, 'SDA': 22,
                              'SDA1': 12},
    'nano32': {'BUTTON': 0, 'LED': 16},
    'nina_w10': {'D0': 3, 'D1': 1, 'D10': 5, 'D11': 19, 'D12': 23, 'D13': 18, 'D14': 13,
                 'D15': 12, 'D16': 32, 'D17': 33, 'D18': 21, 'D19': 34, 'D2': 26, 'D20': 36,
                 'D21': 39, 'D3': 25, 'D4': 35, 'D5': 27, 'D6': 22, 'D7': 0, 'D8': 15, 'D9': 14,
                 'LED_BLUE': 21, 'LED_GREEN': 33, 'LED_RED': 23, 'SCL': 13, 'SDA': 12, 'SW1': 33,
                 'SW2': 27},
    'node32s': {},
    'nodemcu-32s': {'BUTTON': 0, 'LED': 2},
    'odroid_esp32': {'ADC1': 35, 'ADC2': 36, 'LED': 2, 'SCL': 4, 'SDA': 15, 'SS': 22},
    'onehorse32dev': {'A1': 37, 'A2': 38, 'BUTTON': 0, 'LED': 5},
    'oroca_edubot': {'A0': 34, 'A1': 39, 'A2': 36, 'A3': 33, 'D0': 4, 'D1': 16, 'D2': 17,
                     'D3': 22, 'D4': 23, 'D5': 5, 'D6': 18, 'D7': 19, 'D8': 33, 'LED': 13,
                     'MOSI': 18, 'RX': 16, 'SCK': 5, 'SDA': 23, 'SS': 2, 'TX': 17, 'VBAT': 35},
    'pico32': {},
    'pocket_32': {'LED': 16},
    'pycom_gpy': {'A1': 37, 'A2': 38, 'LED': 0, 'MISO': 37, 'MOSI': 22, 'SCK': 13, 'SCL': 13,
                  'SDA': 12, 'SS': 17},
    'quantum': {},
    'sparkfun_lora_gateway_1-channel': {'MISO': 12, 'MOSI': 13, 'SCK': 14, 'SS': 16},
    'tinypico': {},
    'ttgo-lora32-v1': {'A1': 37, 'A2': 38, 'BUTTON': 0, 'LED': 2, 'MOSI': 27, 'SCK': 5, 'SS': 18},
    'ttgo-t-beam': {'BUTTON': 39, 'LED': 14, 'MOSI': 27, 'SCK': 5, 'SS': 18},
    'ttgo-t-watch': {'BUTTON': 36, 'MISO': 2, 'MOSI': 15, 'SCK': 14, 'SS': 13},
    'ttgo-t1': {'LED': 22, 'MISO': 2, 'MOSI': 15, 'SCK': 14, 'SCL': 23, 'SS': 13},
    'turta_iot_node': {},
    'vintlabs-devkit-v1': {'LED': 2, 'PWM0': 12, 'PWM1': 13, 'PWM2': 14, 'PWM3': 15, 'PWM4': 16,
                           'PWM5': 17, 'PWM6': 18, 'PWM7': 19},
    'wemos_d1_mini32': {'D0': 26, 'D1': 22, 'D2': 21, 'D3': 17, 'D4': 16, 'D5': 18, 'D6': 19,
                        'D7': 23, 'D8': 5, 'LED': 2, 'RXD': 3, 'TXD': 1, '_VBAT': 35},
    'wemosbat': {'LED': 16},
    'wesp32': {'MISO': 32, 'SCL': 4, 'SDA': 15},
    'widora-air': {'A1': 39, 'A2': 35, 'A3': 25, 'A4': 26, 'A5': 14, 'A6': 12, 'A7': 15, 'A8': 13,
                   'A9': 2, 'BUTTON': 0, 'D0': 19, 'D1': 23, 'D2': 18, 'D3': 17, 'D4': 16,
                   'D5': 5, 'D6': 4, 'LED': 25, 'MISO': 17, 'MOSI': 16, 'SCL': 19, 'SDA': 23,
                   'T0': 19, 'T1': 23, 'T2': 18, 'T3': 17, 'T4': 16, 'T5': 5, 'T6': 4},
    'xinabox_cw02': {'LED': 27},
}


def _lookup_pin(value):
    if CORE.is_esp8266:
        board_pins = ESP8266_BOARD_PINS.get(CORE.board, {})
        base_pins = ESP8266_BASE_PINS
    elif CORE.is_esp32:
        board_pins = ESP32_BOARD_PINS.get(CORE.board, {})
        base_pins = ESP32_BASE_PINS
    else:
        raise NotImplementedError

    while isinstance(board_pins, str):
        board_pins = ESP8266_BOARD_PINS.get(board_pins, {})
    if value in board_pins:
        return board_pins[value]
    if value in base_pins:
        return base_pins[value]
    raise cv.Invalid(u"Cannot resolve pin name '{}' for board {}.".format(value, CORE.board))


def _translate_pin(value):
    if isinstance(value, dict) or value is None:
        raise cv.Invalid(u"This variable only supports pin numbers, not full pin schemas "
                         u"(with inverted and mode).")
    if isinstance(value, int):
        return value
    try:
        return int(value)
    except ValueError:
        pass
    if value.startswith('GPIO'):
        return cv.Coerce(int)(value[len('GPIO'):].strip())
    return _lookup_pin(value)


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if CORE.is_esp32:
        if value < 0 or value > 39:
            raise cv.Invalid(u"ESP32: Invalid pin number: {}".format(value))
        if 6 <= value <= 11:
            _LOGGER.warning(u"ESP32: Pin %s (6-11) might already be used by the "
                            u"flash interface. Be warned.", value)
        if value in (20, 24, 28, 29, 30, 31):
            _LOGGER.warning(u"ESP32: Pin %s (20, 24, 28-31) can usually not be used. "
                            u"Be warned.", value)
        return value
    if CORE.is_esp8266:
        if 6 <= value <= 11:
            _LOGGER.warning(u"ESP8266: Pin %s (6-11) might already be used by the "
                            u"flash interface. Be warned.", value)
        if value < 0 or value > 17:
            raise cv.Invalid(u"ESP8266: Invalid pin number: {}".format(value))
        return value
    raise NotImplementedError


def input_pin(value):
    return validate_gpio_pin(value)


def input_pullup_pin(value):
    value = input_pin(value)
    if CORE.is_esp32:
        return output_pin(value)
    if CORE.is_esp8266:
        if value == 0:
            raise cv.Invalid("GPIO Pin 0 does not support pullup pin mode. "
                             "Please choose another pin.")
        return value
    raise NotImplementedError


def output_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp32:
        if 34 <= value <= 39:
            raise cv.Invalid(u"ESP32: GPIO{} (34-39) can only be used as an "
                             u"input pin.".format(value))
        return value
    if CORE.is_esp8266:
        return value
    raise NotImplementedError


def analog_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp32:
        if 32 <= value <= 39:  # ADC1
            return value
        raise cv.Invalid(u"ESP32: Only pins 32 though 39 support ADC.")
    if CORE.is_esp8266:
        if value == 17:  # A0
            return value
        raise cv.Invalid(u"ESP8266: Only pin A0 (17) supports ADC.")
    raise NotImplementedError


input_output_pin = cv.All(input_pin, output_pin)

PIN_MODES_ESP8266 = [
    'INPUT', 'OUTPUT', 'INPUT_PULLUP', 'OUTPUT_OPEN_DRAIN', 'SPECIAL', 'FUNCTION_1',
    'FUNCTION_2', 'FUNCTION_3', 'FUNCTION_4',
    'FUNCTION_0', 'WAKEUP_PULLUP', 'WAKEUP_PULLDOWN', 'INPUT_PULLDOWN_16',
]
PIN_MODES_ESP32 = [
    'INPUT', 'OUTPUT', 'INPUT_PULLUP', 'OUTPUT_OPEN_DRAIN', 'SPECIAL', 'FUNCTION_1',
    'FUNCTION_2', 'FUNCTION_3', 'FUNCTION_4',
    'PULLUP', 'PULLDOWN', 'INPUT_PULLDOWN', 'OPEN_DRAIN', 'FUNCTION_5',
    'FUNCTION_6', 'ANALOG',
]


def pin_mode(value):
    if CORE.is_esp32:
        return cv.one_of(*PIN_MODES_ESP32, upper=True)(value)
    if CORE.is_esp8266:
        return cv.one_of(*PIN_MODES_ESP8266, upper=True)(value)
    raise NotImplementedError


GPIO_FULL_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_NUMBER): output_pin,
    cv.Optional(CONF_MODE, default='OUTPUT'): pin_mode,
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})

GPIO_FULL_INPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_NUMBER): input_pin,
    cv.Optional(CONF_MODE, default='INPUT'): pin_mode,
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})

GPIO_FULL_INPUT_PULLUP_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_NUMBER): input_pin,
    cv.Optional(CONF_MODE, default='INPUT_PULLUP'): pin_mode,
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})

GPIO_FULL_ANALOG_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_NUMBER): analog_pin,
    cv.Optional(CONF_MODE, default='INPUT'): pin_mode,
})


def shorthand_output_pin(value):
    value = output_pin(value)
    return GPIO_FULL_OUTPUT_PIN_SCHEMA({CONF_NUMBER: value})


def shorthand_input_pin(value):
    value = input_pin(value)
    return GPIO_FULL_INPUT_PIN_SCHEMA({CONF_NUMBER: value})


def shorthand_input_pullup_pin(value):
    value = input_pullup_pin(value)
    return GPIO_FULL_INPUT_PIN_SCHEMA({
        CONF_NUMBER: value,
        CONF_MODE: 'INPUT_PULLUP',
    })


def shorthand_analog_pin(value):
    value = analog_pin(value)
    return GPIO_FULL_INPUT_PIN_SCHEMA({CONF_NUMBER: value})


def validate_has_interrupt(value):
    if CORE.is_esp8266:
        if value[CONF_NUMBER] >= 16:
            raise cv.Invalid("Pins GPIO16 and GPIO17 do not support interrupts and cannot be used "
                             "here, got {}".format(value[CONF_NUMBER]))
    return value


PIN_SCHEMA_REGISTRY = SimpleRegistry()


def internal_gpio_output_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_OUTPUT_PIN_SCHEMA(value)
    return shorthand_output_pin(value)


def gpio_output_pin_schema(value):
    if isinstance(value, dict):
        for key, entry in PIN_SCHEMA_REGISTRY.items():
            if key in value:
                return entry[1][0](value)
    return internal_gpio_output_pin_schema(value)


def internal_gpio_input_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_INPUT_PIN_SCHEMA(value)
    return shorthand_input_pin(value)


def internal_gpio_analog_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_ANALOG_PIN_SCHEMA(value)
    return shorthand_analog_pin(value)


def gpio_input_pin_schema(value):
    if isinstance(value, dict):
        for key, entry in PIN_SCHEMA_REGISTRY.items():
            if key in value:
                return entry[1][1](value)
    return internal_gpio_input_pin_schema(value)


def internal_gpio_input_pullup_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_INPUT_PULLUP_PIN_SCHEMA(value)
    return shorthand_input_pullup_pin(value)


def gpio_input_pullup_pin_schema(value):
    if isinstance(value, dict):
        for key, entry in PIN_SCHEMA_REGISTRY.items():
            if key in value:
                return entry[1][1](value)
    return internal_gpio_input_pullup_pin_schema(value)
