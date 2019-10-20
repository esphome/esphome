#ifndef __FASTPIN_ARM_NRF52_VARIANTS_H
#define __FASTPIN_ARM_NRF52_VARIANTS_H

// use this to determine if found variant or not (avoid multiple boards at once)
#undef __FASTPIN_ARM_NRF52_VARIANT_FOUND

// Adafruit Bluefruit nRF52832 Feather
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52832_FEATHER) 
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Adafruit Bluefruit nRF52832 Feather is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    _DEFPIN_ARM_IDENTITY_P0( 0); // xtal 1
    _DEFPIN_ARM_IDENTITY_P0( 1); // xtal 2
    _DEFPIN_ARM_IDENTITY_P0( 2); // a0
    _DEFPIN_ARM_IDENTITY_P0( 3); // a1
    _DEFPIN_ARM_IDENTITY_P0( 4); // a2
    _DEFPIN_ARM_IDENTITY_P0( 5); // a3
    _DEFPIN_ARM_IDENTITY_P0( 6); // TXD
    _DEFPIN_ARM_IDENTITY_P0( 7); // GPIO #7
    _DEFPIN_ARM_IDENTITY_P0( 8); // RXD
    _DEFPIN_ARM_IDENTITY_P0( 9); // NFC1
    _DEFPIN_ARM_IDENTITY_P0(10); // NFC2
    _DEFPIN_ARM_IDENTITY_P0(11); // GPIO #11
    _DEFPIN_ARM_IDENTITY_P0(12); // SCK
    _DEFPIN_ARM_IDENTITY_P0(13); // MOSI
    _DEFPIN_ARM_IDENTITY_P0(14); // MISO
    _DEFPIN_ARM_IDENTITY_P0(15); // GPIO #15
    _DEFPIN_ARM_IDENTITY_P0(16); // GPIO #16
    _DEFPIN_ARM_IDENTITY_P0(17); // LED #1 (red)
    _DEFPIN_ARM_IDENTITY_P0(18); // SWO
    _DEFPIN_ARM_IDENTITY_P0(19); // LED #2 (blue)
    _DEFPIN_ARM_IDENTITY_P0(20); // DFU
    // _DEFPIN_ARM_IDENTITY_P0(21); // Reset -- not valid to use for FastLED?
    // _DEFPIN_ARM_IDENTITY_P0(22); // Factory Reset -- not vaild to use for FastLED?
    // _DEFPIN_ARM_IDENTITY_P0(23); // N/A
    // _DEFPIN_ARM_IDENTITY_P0(24); // N/A
    _DEFPIN_ARM_IDENTITY_P0(25); // SDA
    _DEFPIN_ARM_IDENTITY_P0(26); // SCL
    _DEFPIN_ARM_IDENTITY_P0(27); // GPIO #27
    _DEFPIN_ARM_IDENTITY_P0(28); // A4
    _DEFPIN_ARM_IDENTITY_P0(29); // A5
    _DEFPIN_ARM_IDENTITY_P0(30); // A6
    _DEFPIN_ARM_IDENTITY_P0(31); // A7
#endif // defined (ARDUINO_NRF52832_FEATHER) 

// Adafruit Bluefruit nRF52840 Feather Express
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_FEATHER)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif

    #define MAX_PIN (33u) // 34 if wanting to use NFC1 test point

    // Arduino pins 0..7
    _DEFPIN_ARM( 0, 0, 25); // D0  is P0.25 -- UART TX
    //_DEFPIN_ARM( 1, 0, 24); // D1  is P0.24 -- UART RX
    _DEFPIN_ARM( 2, 0, 10); // D2  is P0.10 -- NFC2
    _DEFPIN_ARM( 3, 1, 47); // D3  is P1.15 -- PIN_LED1 (red)
    _DEFPIN_ARM( 4, 1, 42); // D4  is P1.10 -- PIN_LED2 (blue)
    _DEFPIN_ARM( 5, 1, 40); // D5  is P1.08 -- SPI/SS
    _DEFPIN_ARM( 6, 0,  7); // D6  is P0.07
    _DEFPIN_ARM( 7, 1, 34); // D7  is P1.02 -- PIN_DFU (Button)
    
    // Arduino pins 8..15
    _DEFPIN_ARM( 8, 0, 16); // D8  is P0.16 -- PIN_NEOPIXEL
    _DEFPIN_ARM( 9, 0, 26); // D9  is P0.26
    _DEFPIN_ARM(10, 0, 27); // D10 is P0.27
    _DEFPIN_ARM(11, 0,  6); // D11 is P0.06
    _DEFPIN_ARM(12, 0,  8); // D12 is P0.08
    _DEFPIN_ARM(13, 1, 41); // D13 is P1.09
    _DEFPIN_ARM(14, 0,  4); // D14 is P0.04 -- A0
    _DEFPIN_ARM(15, 0,  5); // D15 is P0.05 -- A1

    // Arduino pins 16..23
    _DEFPIN_ARM(16, 0, 30); // D16 is P0.30 -- A2
    _DEFPIN_ARM(17, 0, 28); // D17 is P0.28 -- A3
    _DEFPIN_ARM(18, 0,  2); // D18 is P0.02 -- A4
    _DEFPIN_ARM(19, 0,  3); // D19 is P0.03 -- A5
    //_DEFPIN_ARM(20, 0, 29); // D20 is P0.29 -- A6 -- Connected to battery!
    //_DEFPIN_ARM(21, 0, 31); // D21 is P0.31 -- A7 -- AREF
    _DEFPIN_ARM(22, 0, 12); // D22 is P0.12 -- SDA
    _DEFPIN_ARM(23, 0, 11); // D23 is P0.11 -- SCL

    // Arduino pins 24..31
    _DEFPIN_ARM(24, 0, 15); // D24 is P0.15 -- PIN_SPI_MISO
    _DEFPIN_ARM(25, 0, 13); // D25 is P0.13 -- PIN_SPI_MOSI
    _DEFPIN_ARM(26, 0, 14); // D26 is P0.14 -- PIN_SPI_SCK
    //_DEFPIN_ARM(27, 0, 19); // D27 is P0.19 -- PIN_QSPI_SCK
    //_DEFPIN_ARM(28, 0, 20); // D28 is P0.20 -- PIN_QSPI_CS
    //_DEFPIN_ARM(29, 0, 17); // D29 is P0.17 -- PIN_QSPI_DATA0
    //_DEFPIN_ARM(30, 0, 22); // D30 is P0.22 -- PIN_QSPI_DATA1
    //_DEFPIN_ARM(31, 0, 23); // D31 is P0.23 -- PIN_QSPI_DATA2

    // Arduino pins 32..34
    //_DEFPIN_ARM(32, 0, 21); // D32 is P0.21 -- PIN_QSPI_DATA3
    //_DEFPIN_ARM(33, 0,  9); // D33 is NFC1, only accessible via test point
#endif // defined (ARDUINO_NRF52840_FEATHER)

// Adafruit Bluefruit nRF52840 Metro Express
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_METRO)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Adafruit Bluefruit nRF52840 Metro Express is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"

    _DEFPIN_ARM( 0, 0, 25); // D0  is P0.25 (UART TX)
    _DEFPIN_ARM( 1, 0, 24); // D1  is P0.24 (UART RX)
    _DEFPIN_ARM( 2, 1, 10); // D2  is P1.10 
    _DEFPIN_ARM( 3, 1,  4); // D3  is P1.04 
    _DEFPIN_ARM( 4, 1, 11); // D4  is P1.11 
    _DEFPIN_ARM( 5, 1, 12); // D5  is P1.12 
    _DEFPIN_ARM( 6, 1, 14); // D6  is P1.14
    _DEFPIN_ARM( 7, 0, 26); // D7  is P0.26
    _DEFPIN_ARM( 8, 0, 27); // D8  is P0.27
    _DEFPIN_ARM( 9, 0, 12); // D9  is P0.12
    _DEFPIN_ARM(10, 0,  6); // D10 is P0.06 
    _DEFPIN_ARM(11, 0,  8); // D11 is P0.08 
    _DEFPIN_ARM(12, 1,  9); // D12 is P1.09 
    _DEFPIN_ARM(13, 0, 14); // D13 is P0.14 
    _DEFPIN_ARM(14, 0,  4); // D14 is P0.04 (A0)
    _DEFPIN_ARM(15, 0,  5); // D15 is P0.05 (A1)
    _DEFPIN_ARM(16, 0, 28); // D16 is P0.28 (A2)
    _DEFPIN_ARM(17, 0, 30); // D17 is P0.30 (A3)
    _DEFPIN_ARM(18, 0,  2); // D18 is P0.02 (A4)
    _DEFPIN_ARM(19, 0,  3); // D19 is P0.03 (A5)
    _DEFPIN_ARM(20, 0, 29); // D20 is P0.29 (A6, battery)
    _DEFPIN_ARM(21, 0, 31); // D21 is P0.31 (A7, ARef)
    _DEFPIN_ARM(22, 0, 15); // D22 is P0.15 (SDA)
    _DEFPIN_ARM(23, 0, 16); // D23 is P0.16 (SCL)
    _DEFPIN_ARM(24, 0, 11); // D24 is P0.11 (SPI MISO)
    _DEFPIN_ARM(25, 1,  8); // D25 is P1.08 (SPI MOSI)
    _DEFPIN_ARM(26, 0,  7); // D26 is P0.07 (SPI SCK )
    //_DEFPIN_ARM(27, 0, 19); // D27 is P0.19 (QSPI CLK   )
    //_DEFPIN_ARM(28, 0, 20); // D28 is P0.20 (QSPI CS    )
    //_DEFPIN_ARM(29, 0, 17); // D29 is P0.17 (QSPI Data 0)
    //_DEFPIN_ARM(30, 0, 23); // D30 is P0.23 (QSPI Data 1)
    //_DEFPIN_ARM(31, 0, 22); // D31 is P0.22 (QSPI Data 2)
    //_DEFPIN_ARM(32, 0, 21); // D32 is P0.21 (QSPI Data 3)
    _DEFPIN_ARM(33, 1, 13); // D33 is P1.13 LED1
    _DEFPIN_ARM(34, 1, 15); // D34 is P1.15 LED2
    _DEFPIN_ARM(35, 0, 13); // D35 is P0.13 NeoPixel
    _DEFPIN_ARM(36, 1,  0); // D36 is P1.02 Switch
    _DEFPIN_ARM(37, 1,  0); // D37 is P1.00 SWO/DFU
    _DEFPIN_ARM(38, 0,  9); // D38 is P0.09 NFC1
    _DEFPIN_ARM(39, 0, 10); // D39 is P0.10 NFC2
#endif // defined (ARDUINO_NRF52840_METRO)

// Adafruit Bluefruit on nRF52840DK PCA10056
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_PCA10056)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Adafruit Bluefruit on nRF52840DK PCA10056 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    
    #if defined(USE_ARDUINO_PIN_NUMBERING)
        /* pca10056_schematic_and_pcb.pdf
           Page 3 shows the Arduino Pin to GPIO Px.xx mapping
        */
        _DEFPIN_ARM( 0, 1,  1); // D0  is P1.01 
        _DEFPIN_ARM( 1, 1,  2); // D1  is P1.02 
        _DEFPIN_ARM( 2, 1,  3); // D2  is P1.03
        _DEFPIN_ARM( 3, 1,  4); // D3  is P1.04 
        _DEFPIN_ARM( 4, 1,  5); // D4  is P1.05 
        _DEFPIN_ARM( 5, 1,  6); // D5  is P1.06 
        _DEFPIN_ARM( 6, 1,  7); // D6  is P1.07 (BUTTON1 option)
        _DEFPIN_ARM( 7, 1,  8); // D7  is P1.08 (BUTTON2 option)
        _DEFPIN_ARM( 8, 1, 10); // D8  is P1.10 
        _DEFPIN_ARM( 9, 1, 11); // D9  is P1.11 
        _DEFPIN_ARM(10, 1, 12); // D10 is P1.12 
        _DEFPIN_ARM(11, 1, 13); // D11 is P1.13 
        _DEFPIN_ARM(12, 1, 14); // D12 is P1.14
        _DEFPIN_ARM(13, 1, 15); // D13 is P1.15 
        _DEFPIN_ARM(14, 0,  0); // D14 is P0.00 (if SB4 bridged)
        _DEFPIN_ARM(15, 0,  1); // D15 is P0.01 (if SB3 bridged)
        _DEFPIN_ARM(16, 0,  5); // D16 is P0.05 (aka AIN3, aka UART RTS)
        _DEFPIN_ARM(17, 0,  6); // D17 is P0.06 (UART TxD)
        _DEFPIN_ARM(18, 0,  7); // D18 is P0.07 (UART CTS default)
        _DEFPIN_ARM(19, 0,  8); // D19 is P0.08 (UART RxD)
        _DEFPIN_ARM(20, 0,  9); // D20 is P0.09 (NFC1)
        _DEFPIN_ARM(21, 0, 10); // D21 is P0.10 (NFC2)
        _DEFPIN_ARM(22, 0, 11); // D22 is P0.11 (TRACEDATA2 / BUTTON1 default)
        _DEFPIN_ARM(23, 0, 12); // D23 is P0.12 (TRACEDATA1 / BUTTON2 default)
        _DEFPIN_ARM(24, 0, 13); // D24 is P0.13 (LED1)
        _DEFPIN_ARM(25, 0, 14); // D25 is P0.14 (LED2)
        _DEFPIN_ARM(26, 0, 15); // D26 is P0.15 (LED3)
        _DEFPIN_ARM(27, 0, 16); // D27 is P0.16 (LED4)
        _DEFPIN_ARM(28, 0, 17); // D28 is P0.17 (QSPI !CS , unless SB13 cut)
        // _DEFPIN_ARM(29, 0, 18); // D29 is P0.18 (RESET)
        _DEFPIN_ARM(30, 0, 19); // D30 is P0.19 (QSPI CLK , unless SB11 cut)
        _DEFPIN_ARM(31, 0, 20); // D31 is P0.20 (QSPI DIO0, unless SB12 cut)
        _DEFPIN_ARM(32, 0, 21); // D32 is P0.21 (QSPI DIO1, unless SB14 cut)
        _DEFPIN_ARM(33, 0, 22); // D33 is P0.22 (QSPI DIO2, unless SB15 cut)
        _DEFPIN_ARM(34, 0, 23); // D34 is P0.23 (QSPI DIO3, unless SB10 cut)
        _DEFPIN_ARM(35, 0, 24); // D35 is P0.24 (BUTTON3)
        _DEFPIN_ARM(36, 0, 25); // D36 is P0.25 (BUTTON4)
        _DEFPIN_ARM(37, 1, 00); // D37 is P1.00 (TRACEDATA0 / SWO)
        _DEFPIN_ARM(38, 1, 09); // D38 is P1.09 (TRACEDATA3)
        //_DEFPIN_ARM(??, 0,  2); // D?? is P0.02 (AREF, aka AIN0)
        //_DEFPIN_ARM(??, 0,  3); // D?? is P0.03 (A0,   aka AIN1)
        //_DEFPIN_ARM(??, 0,  4); // D?? is P0.04 (A1,   aka AIN2, aka UART CTS option)
        //_DEFPIN_ARM(??, 0, 28); // D?? is P0.28 (A2,   aka AIN4)
        //_DEFPIN_ARM(??, 0, 29); // D?? is P0.29 (A3,   aka AIN5)
        //_DEFPIN_ARM(??, 0, 30); // D?? is P0.30 (A4,   aka AIN6)
        //_DEFPIN_ARM(??, 0, 31); // D?? is P0.31 (A5,   aka AIN7)

    #else
        /* 48 pins, defined using natural mapping in Adafruit's variant.cpp (!) */
        _DEFPIN_ARM_IDENTITY_P0( 0); // P0.00 (XL1 .. ensure SB4 bridged, SB2 cut)
        _DEFPIN_ARM_IDENTITY_P0( 1); // P0.01 (XL2 .. ensure SB3 bridged, SB1 cut)
        _DEFPIN_ARM_IDENTITY_P0( 2); // P0.02 (AIN0)
        _DEFPIN_ARM_IDENTITY_P0( 3); // P0.03 (AIN1)
        _DEFPIN_ARM_IDENTITY_P0( 4); // P0.04 (AIN2 / UART CTS option)
        _DEFPIN_ARM_IDENTITY_P0( 5); // P0.05 (AIN3 / UART RTS)
        _DEFPIN_ARM_IDENTITY_P0( 6); // P0.06 (UART TxD)
        _DEFPIN_ARM_IDENTITY_P0( 7); // P0.07 (TRACECLK / UART CTS default)
        _DEFPIN_ARM_IDENTITY_P0( 8); // P0.08 (UART RxD)
        _DEFPIN_ARM_IDENTITY_P0( 9); // P0.09 (NFC1)
        _DEFPIN_ARM_IDENTITY_P0(10); // P0.10 (NFC2)
        _DEFPIN_ARM_IDENTITY_P0(11); // P0.11 (TRACEDATA2 / BUTTON1 default)
        _DEFPIN_ARM_IDENTITY_P0(12); // P0.12 (TRACEDATA1 / BUTTON2 default)
        _DEFPIN_ARM_IDENTITY_P0(13); // P0.13 (LED1)
        _DEFPIN_ARM_IDENTITY_P0(14); // P0.14 (LED2)
        _DEFPIN_ARM_IDENTITY_P0(15); // P0.15 (LED3)
        _DEFPIN_ARM_IDENTITY_P0(16); // P0.16 (LED4)
        //_DEFPIN_ARM_IDENTITY_P0(17); // P0.17 (QSPI !CS )
        //_DEFPIN_ARM_IDENTITY_P0(18); // P0.18 (RESET)
        //_DEFPIN_ARM_IDENTITY_P0(19); // P0.19 (QSPI CLK )
        //_DEFPIN_ARM_IDENTITY_P0(20); // P0.20 (QSPI DIO0)
        //_DEFPIN_ARM_IDENTITY_P0(21); // P0.21 (QSPI DIO1)
        //_DEFPIN_ARM_IDENTITY_P0(22); // P0.22 (QSPI DIO2)
        //_DEFPIN_ARM_IDENTITY_P0(23); // P0.23 (QSPI DIO3)
        _DEFPIN_ARM_IDENTITY_P0(24); // P0.24 (BUTTON3)
        _DEFPIN_ARM_IDENTITY_P0(25); // P0.25 (BUTTON4)
        _DEFPIN_ARM_IDENTITY_P0(26); // P0.26
        _DEFPIN_ARM_IDENTITY_P0(27); // P0.27
        _DEFPIN_ARM_IDENTITY_P0(28); // P0.28 (AIN4)
        _DEFPIN_ARM_IDENTITY_P0(29); // P0.29 (AIN5)
        _DEFPIN_ARM_IDENTITY_P0(30); // P0.30 (AIN6)
        _DEFPIN_ARM_IDENTITY_P0(31); // P0.31 (AIN7)
        _DEFPIN_ARM_IDENTITY_P0(32); // P1.00 (SWO / TRACEDATA0)
        _DEFPIN_ARM_IDENTITY_P0(33); // P1.01 
        _DEFPIN_ARM_IDENTITY_P0(34); // P1.02
        _DEFPIN_ARM_IDENTITY_P0(35); // P1.03
        _DEFPIN_ARM_IDENTITY_P0(36); // P1.04
        _DEFPIN_ARM_IDENTITY_P0(37); // P1.05
        _DEFPIN_ARM_IDENTITY_P0(38); // P1.06
        _DEFPIN_ARM_IDENTITY_P0(39); // P1.07 (BUTTON1 option)
        _DEFPIN_ARM_IDENTITY_P0(40); // P1.08 (BUTTON2 option)
        _DEFPIN_ARM_IDENTITY_P0(41); // P1.09 (TRACEDATA3)
        _DEFPIN_ARM_IDENTITY_P0(42); // P1.10
        _DEFPIN_ARM_IDENTITY_P0(43); // P1.11
        _DEFPIN_ARM_IDENTITY_P0(44); // P1.12
        _DEFPIN_ARM_IDENTITY_P0(45); // P1.13
        _DEFPIN_ARM_IDENTITY_P0(46); // P1.14
        _DEFPIN_ARM_IDENTITY_P0(47); // P1.15
    #endif
#endif // defined (ARDUINO_NRF52840_PCA10056)

// Electronut labs bluey
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/bluey/variant.cpp
#if defined(ARDUINO_ELECTRONUT_BLUEY)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Electronut labs bluey is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"

    _DEFPIN_ARM( 0, 0, 26); // D0  is P0.26
    _DEFPIN_ARM( 1, 0, 27); // D1  is P0.27
    _DEFPIN_ARM( 2, 0, 22); // D2  is P0.22 (SPI SS  )
    _DEFPIN_ARM( 3, 0, 23); // D3  is P0.23 (SPI MOSI)
    _DEFPIN_ARM( 4, 0, 24); // D4  is P0.24 (SPI MISO, also A3)
    _DEFPIN_ARM( 5, 0, 25); // D5  is P0.25 (SPI SCK )
    _DEFPIN_ARM( 6, 0, 16); // D6  is P0.16 (Button)
    _DEFPIN_ARM( 7, 0, 19); // D7  is P0.19 (R)
    _DEFPIN_ARM( 8, 0, 18); // D8  is P0.18 (G)
    _DEFPIN_ARM( 9, 0, 17); // D9  is P0.17 (B)
    _DEFPIN_ARM(10, 0, 11); // D10 is P0.11 (SCL)
    _DEFPIN_ARM(11, 0, 12); // D11 is P0.12 (DRDYn)
    _DEFPIN_ARM(12, 0, 13); // D12 is P0.13 (SDA)
    _DEFPIN_ARM(13, 0, 14); // D13 is P0.17 (INT)
    _DEFPIN_ARM(14, 0, 15); // D14 is P0.15 (INT1)
    _DEFPIN_ARM(15, 0, 20); // D15 is P0.20 (INT2)
    _DEFPIN_ARM(16, 0,  2); // D16 is P0.02 (A0)
    _DEFPIN_ARM(17, 0,  3); // D17 is P0.03 (A1)
    _DEFPIN_ARM(18, 0,  4); // D18 is P0.04 (A2)
    _DEFPIN_ARM(19, 0, 24); // D19 is P0.24 (A3, also D4/SPI MISO) -- is this right?
    _DEFPIN_ARM(20, 0, 29); // D20 is P0.29 (A4)
    _DEFPIN_ARM(21, 0, 30); // D21 is P0.30 (A5)
    _DEFPIN_ARM(22, 0, 31); // D22 is P0.31 (A6)
    _DEFPIN_ARM(23, 0,  8); // D23 is P0.08 (RX)
    _DEFPIN_ARM(24, 0,  6); // D24 is P0.06 (TX)
    _DEFPIN_ARM(25, 0,  5); // D25 is P0.05 (RTS)
    _DEFPIN_ARM(26, 0,  7); // D26 is P0.07 (CTS)
#endif // defined(ARDUINO_ELECTRONUT_BLUEY)

// Electronut labs hackaBLE
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/hackaBLE/variant.cpp
#if defined(ARDUINO_ELECTRONUT_HACKABLE)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Electronut labs hackaBLE is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    _DEFPIN_ARM( 0, 0, 14); // D0  is P0.14 (RX)
    _DEFPIN_ARM( 1, 0, 13); // D1  is P0.13 (TX)
    _DEFPIN_ARM( 2, 0, 12); // D2  is P0.12
    _DEFPIN_ARM( 3, 0, 11); // D3  is P0.11 (SPI MOSI)
    _DEFPIN_ARM( 4, 0,  8); // D4  is P0.08 (SPI MISO)
    _DEFPIN_ARM( 5, 0,  7); // D5  is P0.07 (SPI SCK )
    _DEFPIN_ARM( 6, 0,  6); // D6  is P0.06
    _DEFPIN_ARM( 7, 0, 27); // D7  is P0.27
    _DEFPIN_ARM( 8, 0, 26); // D8  is P0.26
    _DEFPIN_ARM( 9, 0, 25); // D9  is P0.25
    _DEFPIN_ARM(10, 0,  5); // D10 is P0.05 (A3)
    _DEFPIN_ARM(11, 0,  4); // D11 is P0.04 (A2)
    _DEFPIN_ARM(12, 0,  3); // D12 is P0.03 (A1)
    _DEFPIN_ARM(13, 0,  2); // D13 is P0.02 (A0 / AREF)
    _DEFPIN_ARM(14, 0, 23); // D14 is P0.23
    _DEFPIN_ARM(15, 0, 22); // D15 is P0.22
    _DEFPIN_ARM(16, 0, 18); // D16 is P0.18
    _DEFPIN_ARM(17, 0, 16); // D17 is P0.16
    _DEFPIN_ARM(18, 0, 15); // D18 is P0.15
    _DEFPIN_ARM(19, 0, 24); // D19 is P0.24
    _DEFPIN_ARM(20, 0, 28); // D20 is P0.28 (A4)
    _DEFPIN_ARM(21, 0, 29); // D21 is P0.29 (A5)
    _DEFPIN_ARM(22, 0, 30); // D22 is P0.30 (A6)
    _DEFPIN_ARM(23, 0, 31); // D23 is P0.31 (A7)
    _DEFPIN_ARM(24, 0, 19); // D24 is P0.19 (RED LED)
    _DEFPIN_ARM(25, 0, 20); // D25 is P0.20 (GREEN LED)
    _DEFPIN_ARM(26, 0, 17); // D26 is P0.17 (BLUE LED)
#endif // defined(ARDUINO_ELECTRONUT_HACKABLE)

// Electronut labs hackaBLE_v2
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/hackaBLE_v2/variant.cpp
// (32 pins, natural mapping)
#if defined(ARDUINO_ELECTRONUT_hackaBLE_v2)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Electronut labs hackaBLE_v2 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    _DEFPIN_ARM_IDENTITY_P0( 0); // P0.00
    _DEFPIN_ARM_IDENTITY_P0( 1); // P0.01
    _DEFPIN_ARM_IDENTITY_P0( 2); // P0.02 (A0 / SDA / AREF)
    _DEFPIN_ARM_IDENTITY_P0( 3); // P0.03 (A1 / SCL )
    _DEFPIN_ARM_IDENTITY_P0( 4); // P0.04 (A2)
    _DEFPIN_ARM_IDENTITY_P0( 5); // P0.05 (A3)
    _DEFPIN_ARM_IDENTITY_P0( 6); // P0.06
    _DEFPIN_ARM_IDENTITY_P0( 7); // P0.07 (RX)
    _DEFPIN_ARM_IDENTITY_P0( 8); // P0.08 (TX)
    _DEFPIN_ARM_IDENTITY_P0( 9); // P0.09
    _DEFPIN_ARM_IDENTITY_P0(10); // P0.10
    _DEFPIN_ARM_IDENTITY_P0(11); // P0.11 (SPI MISO)
    _DEFPIN_ARM_IDENTITY_P0(12); // P0.12 (SPI MOSI)
    _DEFPIN_ARM_IDENTITY_P0(13); // P0.13 (SPI SCK )
    _DEFPIN_ARM_IDENTITY_P0(14); // P0.14 (SPI SS  )
    _DEFPIN_ARM_IDENTITY_P0(15); // P0.15
    _DEFPIN_ARM_IDENTITY_P0(16); // P0.16
    _DEFPIN_ARM_IDENTITY_P0(17); // P0.17 (BLUE LED)
    _DEFPIN_ARM_IDENTITY_P0(18); // P0.18
    _DEFPIN_ARM_IDENTITY_P0(19); // P0.19 (RED LED)
    _DEFPIN_ARM_IDENTITY_P0(20); // P0.20 (GREEN LED)
    // _DEFPIN_ARM_IDENTITY_P0(21); // P0.21 (RESET)
    _DEFPIN_ARM_IDENTITY_P0(22); // P0.22
    _DEFPIN_ARM_IDENTITY_P0(23); // P0.23
    _DEFPIN_ARM_IDENTITY_P0(24); // P0.24
    _DEFPIN_ARM_IDENTITY_P0(25); // P0.25
    _DEFPIN_ARM_IDENTITY_P0(26); // P0.26
    _DEFPIN_ARM_IDENTITY_P0(27); // P0.27
    _DEFPIN_ARM_IDENTITY_P0(28); // P0.28 (A4)
    _DEFPIN_ARM_IDENTITY_P0(29); // P0.29 (A5)
    _DEFPIN_ARM_IDENTITY_P0(30); // P0.30 (A6)
    _DEFPIN_ARM_IDENTITY_P0(31); // P0.31 (A7)
#endif // defined(ARDUINO_ELECTRONUT_hackaBLE_v2)

// RedBear Blend 2
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/RedBear_Blend2/variant.cpp
#if defined(ARDUINO_RB_BLEND_2)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "RedBear Blend 2 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    _DEFPIN_ARM( 0, 0, 11); // D0  is P0.11
    _DEFPIN_ARM( 1, 0, 12); // D1  is P0.12
    _DEFPIN_ARM( 2, 0, 13); // D2  is P0.13
    _DEFPIN_ARM( 3, 0, 14); // D3  is P0.14
    _DEFPIN_ARM( 4, 0, 15); // D4  is P0.15
    _DEFPIN_ARM( 5, 0, 16); // D5  is P0.16
    _DEFPIN_ARM( 6, 0, 17); // D6  is P0.17
    _DEFPIN_ARM( 7, 0, 18); // D7  is P0.18
    _DEFPIN_ARM( 8, 0, 19); // D8  is P0.19
    _DEFPIN_ARM( 9, 0, 20); // D9  is P0.20
    _DEFPIN_ARM(10, 0, 22); // D10 is P0.22 (SPI SS  )
    _DEFPIN_ARM(11, 0, 23); // D11 is P0.23 (SPI MOSI)
    _DEFPIN_ARM(12, 0, 24); // D12 is P0.24 (SPI MISO)
    _DEFPIN_ARM(13, 0, 25); // D13 is P0.25 (SPI SCK / LED)
    _DEFPIN_ARM(14, 0,  3); // D14 is P0.03 (A0)
    _DEFPIN_ARM(15, 0,  4); // D15 is P0.04 (A1)
    _DEFPIN_ARM(16, 0, 28); // D16 is P0.28 (A2)
    _DEFPIN_ARM(17, 0, 29); // D17 is P0.29 (A3)
    _DEFPIN_ARM(18, 0, 30); // D18 is P0.30 (A4)
    _DEFPIN_ARM(19, 0, 31); // D19 is P0.31 (A5)
    _DEFPIN_ARM(20, 0, 26); // D20 is P0.26 (SDA)
    _DEFPIN_ARM(21, 0, 27); // D21 is P0.27 (SCL)
    _DEFPIN_ARM(22, 0,  8); // D22 is P0.08 (RX)
    _DEFPIN_ARM(23, 0,  6); // D23 is P0.06 (TX)
    _DEFPIN_ARM(24, 0,  2); // D24 is P0.02 (AREF)
#endif // defined(ARDUINO_RB_BLEND_2)

// RedBear BLE Nano 2
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/RedBear_BLENano2/variant.cpp
#if defined(ARDUINO_RB_BLE_NANO_2)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "RedBear BLE Nano 2 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    _DEFPIN_ARM( 0, 0, 30); // D0  is P0.30 (A0 / RX)
    _DEFPIN_ARM( 1, 0, 29); // D1  is P0.29 (A1 / TX)
    _DEFPIN_ARM( 2, 0, 28); // D2  is P0.28 (A2 / SDA)
    _DEFPIN_ARM( 3, 0,  2); // D3  is P0.02 (A3 / SCL)
    _DEFPIN_ARM( 4, 0,  5); // D4  is P0.05 (A4)
    _DEFPIN_ARM( 5, 0,  4); // D5  is P0.04 (A5)
    _DEFPIN_ARM( 6, 0,  3); // D6  is P0.03 (SPI SS  )
    _DEFPIN_ARM( 7, 0,  6); // D7  is P0.06 (SPI MOSI)
    _DEFPIN_ARM( 8, 0,  7); // D8  is P0.07 (SPI MISO)
    _DEFPIN_ARM( 9, 0,  8); // D9  is P0.08 (SPI SCK )
    // _DEFPIN_ARM(10, 0, 21); // D10 is P0.21 (RESET)
    _DEFPIN_ARM(13, 0, 11); // D11 is P0.11 (LED)
#endif // defined(ARDUINO_RB_BLE_NANO_2)

// Nordic Semiconductor nRF52 DK
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/nRF52DK/variant.cpp
#if defined(ARDUINO_NRF52_DK)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Nordic Semiconductor nRF52 DK is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    _DEFPIN_ARM( 0, 0, 11); // D0  is P0.11
    _DEFPIN_ARM( 1, 0, 12); // D1  is P0.12
    _DEFPIN_ARM( 2, 0, 13); // D2  is P0.13 (BUTTON1)
    _DEFPIN_ARM( 3, 0, 14); // D3  is P0.14 (BUTTON2)
    _DEFPIN_ARM( 4, 0, 15); // D4  is P0.15 (BUTTON3)
    _DEFPIN_ARM( 5, 0, 16); // D5  is P0.16 (BUTTON4)
    _DEFPIN_ARM( 6, 0, 17); // D6  is P0.17 (LED1)
    _DEFPIN_ARM( 7, 0, 18); // D7  is P0.18 (LED2)
    _DEFPIN_ARM( 8, 0, 19); // D8  is P0.19 (LED3)
    _DEFPIN_ARM( 9, 0, 20); // D9  is P0.20 (LED4)
    _DEFPIN_ARM(10, 0, 22); // D10 is P0.22 (SPI SS  )
    _DEFPIN_ARM(11, 0, 23); // D11 is P0.23 (SPI MOSI)
    _DEFPIN_ARM(12, 0, 24); // D12 is P0.24 (SPI MISO)
    _DEFPIN_ARM(13, 0, 25); // D13 is P0.25 (SPI SCK / LED)
    _DEFPIN_ARM(14, 0,  3); // D14 is P0.03 (A0)
    _DEFPIN_ARM(15, 0,  4); // D15 is P0.04 (A1)
    _DEFPIN_ARM(16, 0, 28); // D16 is P0.28 (A2)
    _DEFPIN_ARM(17, 0, 29); // D17 is P0.29 (A3)
    _DEFPIN_ARM(18, 0, 30); // D18 is P0.30 (A4)
    _DEFPIN_ARM(19, 0, 31); // D19 is P0.31 (A5)
    _DEFPIN_ARM(20, 0,  5); // D20 is P0.05 (A6)
    _DEFPIN_ARM(21, 0,  2); // D21 is P0.02 (A7 / AREF)
    _DEFPIN_ARM(22, 0, 26); // D22 is P0.26 (SDA)
    _DEFPIN_ARM(23, 0, 27); // D23 is P0.27 (SCL)
    _DEFPIN_ARM(24, 0,  8); // D24 is P0.08 (RX)
    _DEFPIN_ARM(25, 0,  6); // D25 is P0.06 (TX)
#endif // defined(ARDUINO_NRF52_DK)

// Taida Century nRF52 mini board
// https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/Taida_Century_nRF52_minidev/variant.cpp
#if defined(ARDUINO_STCT_NRF52_minidev)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Taida Century nRF52 mini board is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    //_DEFPIN_ARM( 0, 0, 25); // D0  is P0.xx (near radio!)
    //_DEFPIN_ARM( 1, 0, 26); // D1  is P0.xx (near radio!)
    //_DEFPIN_ARM( 2, 0, 27); // D2  is P0.xx (near radio!)
    //_DEFPIN_ARM( 3, 0, 28); // D3  is P0.xx (near radio!)
    //_DEFPIN_ARM( 4, 0, 29); // D4  is P0.xx (Not connected, near radio!)
    //_DEFPIN_ARM( 5, 0, 30); // D5  is P0.xx (LED1, near radio!)
    //_DEFPIN_ARM( 6, 0, 31); // D6  is P0.xx (LED2, near radio!)
    _DEFPIN_ARM( 7, 0,  2); // D7  is P0.xx (SDA)
    _DEFPIN_ARM( 8, 0,  3); // D8  is P0.xx (SCL)
    _DEFPIN_ARM( 9, 0,  4); // D9  is P0.xx (BUTTON1 / NFC1)
    _DEFPIN_ARM(10, 0,  5); // D10 is P0.xx
    //_DEFPIN_ARM(11, 0,  0); // D11 is P0.xx (Not connected)
    //_DEFPIN_ARM(12, 0,  1); // D12 is P0.xx (Not connected)
    _DEFPIN_ARM(13, 0,  6); // D13 is P0.xx
    _DEFPIN_ARM(14, 0,  7); // D14 is P0.xx
    _DEFPIN_ARM(15, 0,  8); // D15 is P0.xx
    //_DEFPIN_ARM(16, 0,  9); // D16 is P0.xx (Not connected)
    //_DEFPIN_ARM(17, 0, 10); // D17 is P0.xx (NFC2, Not connected)
    _DEFPIN_ARM(18, 0, 11); // D18 is P0.xx (RXD)
    _DEFPIN_ARM(19, 0, 12); // D19 is P0.xx (TXD)
    _DEFPIN_ARM(20, 0, 13); // D20 is P0.xx (SPI SS  )
    _DEFPIN_ARM(21, 0, 14); // D21 is P0.xx (SPI MISO)
    _DEFPIN_ARM(22, 0, 15); // D22 is P0.xx (SPI MOSI)
    _DEFPIN_ARM(23, 0, 16); // D23 is P0.xx (SPI SCK )
    _DEFPIN_ARM(24, 0, 17); // D24 is P0.xx (A0)
    _DEFPIN_ARM(25, 0, 18); // D25 is P0.xx (A1)
    _DEFPIN_ARM(26, 0, 19); // D26 is P0.xx (A2)
    _DEFPIN_ARM(27, 0, 20); // D27 is P0.xx (A3)
    //_DEFPIN_ARM(28, 0, 22); // D28 is P0.xx (A4, near radio!)
    //_DEFPIN_ARM(29, 0, 23); // D29 is P0.xx (A5, near radio!)
    _DEFPIN_ARM(30, 0, 24); // D30 is P0.xx
    // _DEFPIN_ARM(31, 0, 21); // D31 is P0.21 (RESET)
#endif // defined(ARDUINO_STCT_NRF52_minidev)

// Generic nRF52832
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/boards.txt
#if defined(ARDUINO_GENERIC) && (\
    defined(NRF52832_XXAA) || defined(NRF52832_XXAB)\
    )
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #warning "Using `generic` NRF52832 board is an untested configuration -- test and let use know your results via https://github.com/FastLED/FastLED/issues"

    _DEFPIN_ARM_IDENTITY_P0( 0); // P0.00 (    UART RX
    _DEFPIN_ARM_IDENTITY_P0( 1); // P0.01 (A0, UART TX)
    _DEFPIN_ARM_IDENTITY_P0( 2); // P0.02 (A1)
    _DEFPIN_ARM_IDENTITY_P0( 3); // P0.03 (A2)
    _DEFPIN_ARM_IDENTITY_P0( 4); // P0.04 (A3)
    _DEFPIN_ARM_IDENTITY_P0( 5); // P0.05 (A4)
    _DEFPIN_ARM_IDENTITY_P0( 6); // P0.06 (A5)
    _DEFPIN_ARM_IDENTITY_P0( 7); // P0.07
    _DEFPIN_ARM_IDENTITY_P0( 8); // P0.08
    _DEFPIN_ARM_IDENTITY_P0( 9); // P0.09
    _DEFPIN_ARM_IDENTITY_P0(10); // P0.10
    _DEFPIN_ARM_IDENTITY_P0(11); // P0.11
    _DEFPIN_ARM_IDENTITY_P0(12); // P0.12
    _DEFPIN_ARM_IDENTITY_P0(13); // P0.13 (LED)
    _DEFPIN_ARM_IDENTITY_P0(14); // P0.14
    _DEFPIN_ARM_IDENTITY_P0(15); // P0.15
    _DEFPIN_ARM_IDENTITY_P0(16); // P0.16
    _DEFPIN_ARM_IDENTITY_P0(17); // P0.17
    _DEFPIN_ARM_IDENTITY_P0(18); // P0.18
    _DEFPIN_ARM_IDENTITY_P0(19); // P0.19
    _DEFPIN_ARM_IDENTITY_P0(20); // P0.20 (I2C SDA)
    _DEFPIN_ARM_IDENTITY_P0(21); // P0.21 (I2C SCL)
    _DEFPIN_ARM_IDENTITY_P0(22); // P0.22 (SPI MISO)
    _DEFPIN_ARM_IDENTITY_P0(23); // P0.23 (SPI MOSI)
    _DEFPIN_ARM_IDENTITY_P0(24); // P0.24 (SPI SCK )
    _DEFPIN_ARM_IDENTITY_P0(25); // P0.25 (SPI SS  )
    _DEFPIN_ARM_IDENTITY_P0(26); // P0.26
    _DEFPIN_ARM_IDENTITY_P0(27); // P0.27
    _DEFPIN_ARM_IDENTITY_P0(28); // P0.28
    _DEFPIN_ARM_IDENTITY_P0(29); // P0.29
    _DEFPIN_ARM_IDENTITY_P0(30); // P0.30
    _DEFPIN_ARM_IDENTITY_P0(31); // P0.31
#endif // defined(ARDUINO_GENERIC)


#endif // __FASTPIN_ARM_NRF52_VARIANTS_H