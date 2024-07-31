#pragma once

#include <cstdint>

static inline bool parse_flag8_lb_0(const unsigned long response) {
    return response & 0b0000000000000001;
}

static inline bool parse_flag8_lb_1(const unsigned long response) {
    return response & 0b0000000000000010;
}

static inline bool parse_flag8_lb_2(const unsigned long response) {
    return response & 0b0000000000000100;
}

static inline bool parse_flag8_lb_3(const unsigned long response) {
    return response & 0b0000000000001000;
}

static inline bool parse_flag8_lb_4(const unsigned long response) {
    return response & 0b0000000000010000;
}

static inline bool parse_flag8_lb_5(const unsigned long response) {
    return response & 0b0000000000100000;
}

static inline bool parse_flag8_lb_6(const unsigned long response) {
    return response & 0b0000000001000000;
}

static inline bool parse_flag8_lb_7(const unsigned long response) {
    return response & 0b0000000010000000;
}

static inline bool parse_flag8_hb_0(const unsigned long response) {
    return response & 0b0000000100000000;
}

static inline bool parse_flag8_hb_1(const unsigned long response) {
    return response & 0b0000001000000000;
}

static inline bool parse_flag8_hb_2(const unsigned long response) {
    return response & 0b0000010000000000;
}

static inline bool parse_flag8_hb_3(const unsigned long response) {
    return response & 0b0000100000000000;
}

static inline bool parse_flag8_hb_4(const unsigned long response) {
    return response & 0b0001000000000000;
}

static inline bool parse_flag8_hb_5(const unsigned long response) {
    return response & 0b0010000000000000;
}

static inline bool parse_flag8_hb_6(const unsigned long response) {
    return response & 0b0100000000000000;
}

static inline bool parse_flag8_hb_7(const unsigned long response) {
    return response & 0b1000000000000000;
}

static inline uint8_t parse_u8_lb(const unsigned long response) {
    return (uint8_t)(response & 0xff);
}

static inline uint8_t parse_u8_hb(const unsigned long response) {
    return (uint8_t)((response >> 8) & 0xff);
}

static inline int8_t parse_s8_lb(const unsigned long response) {
    return (int8_t)(response & 0xff);
}

static inline int8_t parse_s8_hb(const unsigned long response) {
    return (int8_t)((response >> 8) & 0xff);
}

static inline uint16_t parse_u16(const unsigned long response) {
    return (uint16_t)(response & 0xffff);
}

static inline int16_t parse_s16(const unsigned long response) {
    return (int16_t)(response & 0xffff);
}

static inline float parse_f88(const unsigned long response) {
    unsigned int data = response & 0xffff;
    return (data & 0x8000) ? -(0x10000L - data) / 256.0f : data / 256.0f;
}

static inline unsigned int write_flag8_lb_0(const bool value, const unsigned int data) {
    return value ? data | 0b0000000000000001 : data & 0b1111111111111110;
}

static inline unsigned int write_flag8_lb_1(const bool value, const unsigned int data) {
    return value ? data | 0b0000000000000010 : data & 0b1111111111111101;
}

static inline unsigned int write_flag8_lb_2(const bool value, const unsigned int data) {
    return value ? data | 0b0000000000000100 : data & 0b1111111111111011;
}

static inline unsigned int write_flag8_lb_3(const bool value, const unsigned int data) {
    return value ? data | 0b0000000000001000 : data & 0b1111111111110111;
}

static inline unsigned int write_flag8_lb_4(const bool value, const unsigned int data) {
    return value ? data | 0b0000000000010000 : data & 0b1111111111101111;
}

static inline unsigned int write_flag8_lb_5(const bool value, const unsigned int data) {
    return value ? data | 0b0000000000100000 : data & 0b1111111111011111;
}

static inline unsigned int write_flag8_lb_6(const bool value, const unsigned int data) {
    return value ? data | 0b0000000001000000 : data & 0b1111111110111111;
}

static inline unsigned int write_flag8_lb_7(const bool value, const unsigned int data) {
    return value ? data | 0b0000000010000000 : data & 0b1111111101111111;
}

static inline unsigned int write_flag8_hb_0(const bool value, const unsigned int data) {
    return value ? data | 0b0000000100000000 : data & 0b1111111011111111;
}

static inline unsigned int write_flag8_hb_1(const bool value, const unsigned int data) {
    return value ? data | 0b0000001000000000 : data & 0b1111110111111111;
}

static inline unsigned int write_flag8_hb_2(const bool value, const unsigned int data) {
    return value ? data | 0b0000010000000000 : data & 0b1111101111111111;
}

static inline unsigned int write_flag8_hb_3(const bool value, const unsigned int data) {
    return value ? data | 0b0000100000000000 : data & 0b1111011111111111;
}

static inline unsigned int write_flag8_hb_4(const bool value, const unsigned int data) {
    return value ? data | 0b0001000000000000 : data & 0b1110111111111111;
}

static inline unsigned int write_flag8_hb_5(const bool value, const unsigned int data) {
    return value ? data | 0b0010000000000000 : data & 0b1101111111111111;
}

static inline unsigned int write_flag8_hb_6(const bool value, const unsigned int data) {
    return value ? data | 0b0100000000000000 : data & 0b1011111111111111;
}

static inline unsigned int write_flag8_hb_7(const bool value, const unsigned int data) {
    return value ? data | 0b1000000000000000 : data & 0b0111111111111111;
}

static inline unsigned int write_u8_lb(const uint8_t value, const unsigned int data) {
    return (data & 0xff00) | value;
}

static inline unsigned int write_u8_hb(const uint8_t value, const unsigned int data) {
    return (data & 0x00ff) | (value << 8);
}

static inline unsigned int write_s8_lb(const int8_t value, const unsigned int data) {
    return (data & 0xff00) | value;
}

static inline unsigned int write_s8_hb(const int8_t value, const unsigned int data) {
    return (data & 0x00ff) | (value << 8);
}

static inline unsigned int write_u16(const uint16_t value, const unsigned int data) {
    return value;
}

static inline unsigned int write_s16(const int16_t value, const unsigned int data) {
    return value;
}

static inline unsigned int write_f88(const float value, const unsigned int data) {
    return (unsigned int) (value * 256.0f);
}