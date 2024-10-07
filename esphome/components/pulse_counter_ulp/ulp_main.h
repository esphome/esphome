// This file has been copied into place to satisfy clang-tidy, but it should be
// created at compile-time by IDF. Please synchronise this file if changes are
// made to pulse__cnt.S
// NOLINTBEGIN

// Variable definitions for ESP32ULP
// This file is generated automatically by esp32ulp_mapgen.py utility

#pragma once

// Required to pass linting
namespace pulse_counter_ulp {}  // namespace pulse_counter_ulp

extern uint32_t ulp_changed;
extern uint32_t ulp_debounce_counter;
extern uint32_t ulp_debounce_max_count;
extern uint32_t ulp_edge_count_total;
extern uint32_t ulp_edge_detected;
extern uint32_t ulp_entry;
extern uint32_t ulp_falling;
extern uint32_t ulp_falling_edge_count;
extern uint32_t ulp_io_number;
extern uint32_t ulp_mean_exec_time;
extern uint32_t ulp_next_edge;
extern uint32_t ulp_rising;
extern uint32_t ulp_rising_edge_count;
extern uint32_t ulp_run_count;

// NOLINTEND
