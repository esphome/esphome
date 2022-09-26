/**************************************************************************/
/*!
  @file     FT6336U.h
  Author: Atsushi Sasaki(https://github.com/aselectroworks)
  License: MIT (see LICENSE)
*/
/**************************************************************************/

#ifndef _FT6336U_H
#define _FT6336U_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

#define I2C_ADDR_FT6336U 0x38

// Touch Parameter
#define FT6336U_PRES_DOWN 0x2
#define FT6336U_COORD_UD  0x1

// Registers
#define FT6336U_ADDR_DEVICE_MODE 	0x00
typedef enum {
    working_mode = 0b000, 
    factory_mode = 0b100, 
} DEVICE_MODE_Enum; 
#define FT6336U_ADDR_GESTURE_ID     0x01
#define FT6336U_ADDR_TD_STATUS 		0x02

#define FT6336U_ADDR_TOUCH1_EVENT 	0x03
#define FT6336U_ADDR_TOUCH1_ID 		0x05
#define FT6336U_ADDR_TOUCH1_X 		0x03
#define FT6336U_ADDR_TOUCH1_Y 		0x05
#define FT6336U_ADDR_TOUCH1_WEIGHT  0x07
#define FT6336U_ADDR_TOUCH1_MISC    0x08

#define FT6336U_ADDR_TOUCH2_EVENT 	0x09
#define FT6336U_ADDR_TOUCH2_ID 		0x0B
#define FT6336U_ADDR_TOUCH2_X 		0x09
#define FT6336U_ADDR_TOUCH2_Y 		0x0B
#define FT6336U_ADDR_TOUCH2_WEIGHT  0x0D
#define FT6336U_ADDR_TOUCH2_MISC    0x0E

#define FT6336U_ADDR_THRESHOLD          0x80
#define FT6336U_ADDR_FILTER_COE         0x85
#define FT6336U_ADDR_CTRL               0x86
typedef enum {
    keep_active_mode = 0, 
    switch_to_monitor_mode = 1, 
} CTRL_MODE_Enum; 
#define FT6336U_ADDR_TIME_ENTER_MONITOR 0x87
#define FT6336U_ADDR_ACTIVE_MODE_RATE   0x88
#define FT6336U_ADDR_MONITOR_MODE_RATE  0x89

#define FT6336U_ADDR_RADIAN_VALUE           0x91
#define FT6336U_ADDR_OFFSET_LEFT_RIGHT      0x92
#define FT6336U_ADDR_OFFSET_UP_DOWN         0x93
#define FT6336U_ADDR_DISTANCE_LEFT_RIGHT    0x94
#define FT6336U_ADDR_DISTANCE_UP_DOWN       0x95
#define FT6336U_ADDR_DISTANCE_ZOOM          0x96

#define FT6336U_ADDR_LIBRARY_VERSION_H  0xA1
#define FT6336U_ADDR_LIBRARY_VERSION_L  0xA2
#define FT6336U_ADDR_CHIP_ID            0xA3
#define FT6336U_ADDR_G_MODE             0xA4
typedef enum {
    pollingMode = 0, 
    triggerMode = 1, 
} G_MODE_Enum; 
#define FT6336U_ADDR_POWER_MODE         0xA5
#define FT6336U_ADDR_FIRMARE_ID         0xA6
#define FT6336U_ADDR_FOCALTECH_ID       0xA8
#define FT6336U_ADDR_RELEASE_CODE_ID    0xAF
#define FT6336U_ADDR_STATE              0xBC

// Function Specific Type
typedef enum {
    touch = 0, 
    stream, 
    release, 
} TouchStatusEnum; 
typedef struct {
    TouchStatusEnum status; 
    uint16_t x; 
    uint16_t y; 
} TouchPointType;
typedef struct {
    uint8_t touch_count; 
    TouchPointType tp[2]; 
} FT6336U_TouchPointType; 


// Uncomment to enable debug messages
//#define FT6336U_DEBUG

// Define where debug output will be printed
#define DEBUG_PRINTER Serial

// Setup debug printing macros
#ifdef FT6336U_DEBUG
#define DEBUG_PRINT(...)                  \
    {                                     \
        DEBUG_PRINTER.print(__VA_ARGS__); \
    }
#define DEBUG_PRINTLN(...)                  \
    {                                       \
        DEBUG_PRINTER.println(__VA_ARGS__); \
    }
#else
#define DEBUG_PRINT(...) \
    {                    \
    }
#define DEBUG_PRINTLN(...) \
    {                      \
    }
#endif

/**************************************************************************/
/*!
    @brief  FT6336U I2C CTP controller driver
*/
/**************************************************************************/
class FT6336U 
{
public: 
    FT6336U(uint8_t rst_n, uint8_t int_n); 
#if defined(ESP32) || defined(ESP8266)
    FT6336U(int8_t sda, int8_t scl, uint8_t rst_n, uint8_t int_n); 
#endif
    virtual ~FT6336U(); 

    void begin(void);

    uint8_t read_device_mode(void);
    void write_device_mode(DEVICE_MODE_Enum);
    uint8_t read_gesture_id(void); 
    uint8_t read_td_status(void);
    uint8_t read_touch_number(void); 
    uint16_t read_touch1_x(void);
    uint16_t read_touch1_y(void);
    uint8_t read_touch1_event(void);
    uint8_t read_touch1_id(void);
    uint8_t read_touch1_weight(void);
    uint8_t read_touch1_misc(void);
    uint16_t read_touch2_x(void);
    uint16_t read_touch2_y(void);
    uint8_t read_touch2_event(void);
    uint8_t read_touch2_id(void);
    uint8_t read_touch2_weight(void);
    uint8_t read_touch2_misc(void);

    // Mode Parameter Register
    uint8_t read_touch_threshold(void); 
    uint8_t read_filter_coefficient(void); 
    uint8_t read_ctrl_mode(void); 
    void write_ctrl_mode(CTRL_MODE_Enum mode); 
    uint8_t read_time_period_enter_monitor(void); 
    uint8_t read_active_rate(void); 
    uint8_t read_monitor_rate(void); 

    // Gestrue Parameter Register
    uint8_t read_radian_value(void); 
    void write_radian_value(uint8_t val); 
    uint8_t read_offset_left_right(void); 
    void write_offset_left_right(uint8_t val); 
    uint8_t read_offset_up_down(void); 
    void write_offset_up_down(uint8_t val); 
    uint8_t read_distance_left_right(void); 
    void write_distance_left_right(uint8_t val); 
    uint8_t read_distance_up_down(void); 
    void write_distance_up_down(uint8_t val); 
    uint8_t read_distance_zoom(void); 
    void write_distance_zoom(uint8_t val); 

    // System Information
    uint16_t read_library_version(void); 
    uint8_t read_chip_id(void); 
    uint8_t read_g_mode(void); 
    void write_g_mode(G_MODE_Enum mode); 
    uint8_t read_pwrmode(void); 
    uint8_t read_firmware_id(void);
    uint8_t read_focaltech_id(void); 
    uint8_t read_release_code_id(void); 
    uint8_t read_state(void); 

    // Scan Function
    FT6336U_TouchPointType scan(void);

private: 
    int8_t sda = -1; 
    int8_t scl = -1; 
    uint8_t rst_n = -1; 
    uint8_t int_n = -1; 
    
    uint8_t readByte(uint8_t addr); 
    void writeByte(uint8_t addr, uint8_t data); 

    FT6336U_TouchPointType touchPoint; 
}; 
#endif