#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/progmem.h"

#ifdef USE_MQTT

// MQTT JSON Key Constants for Home Assistant Discovery
//
// This file defines string constants used as JSON keys in MQTT discovery payloads
// for Home Assistant integration. These are used exclusively with ArduinoJson as:
//   root[MQTT_DEVICE_CLASS] = "temperature";
//
// Implementation:
// - ESP8266: Stores strings in PROGMEM (flash) using __FlashStringHelper* pointers.
//   ArduinoJson recognizes this type and reads from flash memory.
// - Other platforms: Uses constexpr const char* for compile-time optimization.
// - USE_MQTT_ABBREVIATIONS: When defined, uses shortened key names to reduce message size.
//
// Adding new keys:
//   Add a single line to MQTT_KEYS_LIST: X(MQTT_NEW_KEY, "abbr", "full_name")
//   The X-macro will generate the appropriate constants for each platform.
//
// Note: Other MQTT_* constants (e.g., MQTT_CLIENT_CONNECTED, MQTT_LEGACY_UNIQUE_ID_GENERATOR)
// are C++ enums defined in mqtt_client.h and mqtt_backend*.h - unrelated to these JSON keys.

// X-macro list: MQTT_KEYS_LIST(X) calls X(name, abbr, full) for each key
// clang-format off
#define MQTT_KEYS_LIST(X) \
  X(MQTT_ACTION_TEMPLATE, "act_tpl", "action_template") \
  X(MQTT_ACTION_TOPIC, "act_t", "action_topic") \
  X(MQTT_AUTOMATION_TYPE, "atype", "automation_type") \
  X(MQTT_AUX_COMMAND_TOPIC, "aux_cmd_t", "aux_command_topic") \
  X(MQTT_AUX_STATE_TEMPLATE, "aux_stat_tpl", "aux_state_template") \
  X(MQTT_AUX_STATE_TOPIC, "aux_stat_t", "aux_state_topic") \
  X(MQTT_AVAILABILITY, "avty", "availability") \
  X(MQTT_AVAILABILITY_MODE, "avty_mode", "availability_mode") \
  X(MQTT_AVAILABILITY_TOPIC, "avty_t", "availability_topic") \
  X(MQTT_AWAY_MODE_COMMAND_TOPIC, "away_mode_cmd_t", "away_mode_command_topic") \
  X(MQTT_AWAY_MODE_STATE_TEMPLATE, "away_mode_stat_tpl", "away_mode_state_template") \
  X(MQTT_AWAY_MODE_STATE_TOPIC, "away_mode_stat_t", "away_mode_state_topic") \
  X(MQTT_BATTERY_LEVEL_TEMPLATE, "bat_lev_tpl", "battery_level_template") \
  X(MQTT_BATTERY_LEVEL_TOPIC, "bat_lev_t", "battery_level_topic") \
  X(MQTT_BLUE_TEMPLATE, "b_tpl", "blue_template") \
  X(MQTT_BRIGHTNESS_COMMAND_TOPIC, "bri_cmd_t", "brightness_command_topic") \
  X(MQTT_BRIGHTNESS_SCALE, "bri_scl", "brightness_scale") \
  X(MQTT_BRIGHTNESS_STATE_TOPIC, "bri_stat_t", "brightness_state_topic") \
  X(MQTT_BRIGHTNESS_TEMPLATE, "bri_tpl", "brightness_template") \
  X(MQTT_BRIGHTNESS_VALUE_TEMPLATE, "bri_val_tpl", "brightness_value_template") \
  X(MQTT_CHARGING_TEMPLATE, "chrg_tpl", "charging_template") \
  X(MQTT_CHARGING_TOPIC, "chrg_t", "charging_topic") \
  X(MQTT_CLEANING_TEMPLATE, "cln_tpl", "cleaning_template") \
  X(MQTT_CLEANING_TOPIC, "cln_t", "cleaning_topic") \
  X(MQTT_CODE_ARM_REQUIRED, "cod_arm_req", "code_arm_required") \
  X(MQTT_CODE_DISARM_REQUIRED, "cod_dis_req", "code_disarm_required") \
  X(MQTT_COLOR_MODE, "clrm", "color_mode") \
  X(MQTT_COLOR_MODE_STATE_TOPIC, "clrm_stat_t", "color_mode_state_topic") \
  X(MQTT_COLOR_MODE_VALUE_TEMPLATE, "clrm_val_tpl", "color_mode_value_template") \
  X(MQTT_COLOR_TEMP_COMMAND_TEMPLATE, "clr_temp_cmd_tpl", "color_temp_command_template") \
  X(MQTT_COLOR_TEMP_COMMAND_TOPIC, "clr_temp_cmd_t", "color_temp_command_topic") \
  X(MQTT_COLOR_TEMP_STATE_TOPIC, "clr_temp_stat_t", "color_temp_state_topic") \
  X(MQTT_COLOR_TEMP_TEMPLATE, "clr_temp_tpl", "color_temp_template") \
  X(MQTT_COLOR_TEMP_VALUE_TEMPLATE, "clr_temp_val_tpl", "color_temp_value_template") \
  X(MQTT_COMMAND_OFF_TEMPLATE, "cmd_off_tpl", "command_off_template") \
  X(MQTT_COMMAND_ON_TEMPLATE, "cmd_on_tpl", "command_on_template") \
  X(MQTT_COMMAND_RETAIN, "ret", "retain") \
  X(MQTT_COMMAND_TEMPLATE, "cmd_tpl", "command_template") \
  X(MQTT_COMMAND_TOPIC, "cmd_t", "command_topic") \
  X(MQTT_CONFIGURATION_URL, "cu", "configuration_url") \
  X(MQTT_CURRENT_HUMIDITY_TEMPLATE, "curr_hum_tpl", "current_humidity_template") \
  X(MQTT_CURRENT_HUMIDITY_TOPIC, "curr_hum_t", "current_humidity_topic") \
  X(MQTT_CURRENT_TEMPERATURE_STEP, "precision", "precision") \
  X(MQTT_CURRENT_TEMPERATURE_TEMPLATE, "curr_temp_tpl", "current_temperature_template") \
  X(MQTT_CURRENT_TEMPERATURE_TOPIC, "curr_temp_t", "current_temperature_topic") \
  X(MQTT_DEVICE, "dev", "device") \
  X(MQTT_DEVICE_CLASS, "dev_cla", "device_class") \
  X(MQTT_DEVICE_CONNECTIONS, "cns", "connections") \
  X(MQTT_DEVICE_IDENTIFIERS, "ids", "identifiers") \
  X(MQTT_DEVICE_MANUFACTURER, "mf", "manufacturer") \
  X(MQTT_DEVICE_MODEL, "mdl", "model") \
  X(MQTT_DEVICE_NAME, "name", "name") \
  X(MQTT_DEVICE_SUGGESTED_AREA, "sa", "suggested_area") \
  X(MQTT_DEVICE_SW_VERSION, "sw", "sw_version") \
  X(MQTT_DEVICE_HW_VERSION, "hw", "hw_version") \
  X(MQTT_DIRECTION_COMMAND_TOPIC, "dir_cmd_t", "direction_command_topic") \
  X(MQTT_DIRECTION_STATE_TOPIC, "dir_stat_t", "direction_state_topic") \
  X(MQTT_DOCKED_TEMPLATE, "dock_tpl", "docked_template") \
  X(MQTT_DOCKED_TOPIC, "dock_t", "docked_topic") \
  X(MQTT_EFFECT_COMMAND_TOPIC, "fx_cmd_t", "effect_command_topic") \
  X(MQTT_EFFECT_LIST, "fx_list", "effect_list") \
  X(MQTT_EFFECT_STATE_TOPIC, "fx_stat_t", "effect_state_topic") \
  X(MQTT_EFFECT_TEMPLATE, "fx_tpl", "effect_template") \
  X(MQTT_EFFECT_VALUE_TEMPLATE, "fx_val_tpl", "effect_value_template") \
  X(MQTT_ENABLED_BY_DEFAULT, "en", "enabled_by_default") \
  X(MQTT_ENTITY_CATEGORY, "ent_cat", "entity_category") \
  X(MQTT_ERROR_TEMPLATE, "err_tpl", "error_template") \
  X(MQTT_ERROR_TOPIC, "err_t", "error_topic") \
  X(MQTT_EVENT_TYPE, "event_type", "event_type") \
  X(MQTT_EVENT_TYPES, "evt_typ", "event_types") \
  X(MQTT_EXPIRE_AFTER, "exp_aft", "expire_after") \
  X(MQTT_FAN_MODE_COMMAND_TEMPLATE, "fan_mode_cmd_tpl", "fan_mode_command_template") \
  X(MQTT_FAN_MODE_COMMAND_TOPIC, "fan_mode_cmd_t", "fan_mode_command_topic") \
  X(MQTT_FAN_MODE_STATE_TEMPLATE, "fan_mode_stat_tpl", "fan_mode_state_template") \
  X(MQTT_FAN_MODE_STATE_TOPIC, "fan_mode_stat_t", "fan_mode_state_topic") \
  X(MQTT_FAN_SPEED_LIST, "fanspd_lst", "fan_speed_list") \
  X(MQTT_FAN_SPEED_TEMPLATE, "fanspd_tpl", "fan_speed_template") \
  X(MQTT_FAN_SPEED_TOPIC, "fanspd_t", "fan_speed_topic") \
  X(MQTT_FLASH_TIME_LONG, "flsh_tlng", "flash_time_long") \
  X(MQTT_FLASH_TIME_SHORT, "flsh_tsht", "flash_time_short") \
  X(MQTT_FORCE_UPDATE, "frc_upd", "force_update") \
  X(MQTT_GREEN_TEMPLATE, "g_tpl", "green_template") \
  X(MQTT_HOLD_COMMAND_TEMPLATE, "hold_cmd_tpl", "hold_command_template") \
  X(MQTT_HOLD_COMMAND_TOPIC, "hold_cmd_t", "hold_command_topic") \
  X(MQTT_HOLD_STATE_TEMPLATE, "hold_stat_tpl", "hold_state_template") \
  X(MQTT_HOLD_STATE_TOPIC, "hold_stat_t", "hold_state_topic") \
  X(MQTT_HS_COMMAND_TOPIC, "hs_cmd_t", "hs_command_topic") \
  X(MQTT_HS_STATE_TOPIC, "hs_stat_t", "hs_state_topic") \
  X(MQTT_HS_VALUE_TEMPLATE, "hs_val_tpl", "hs_value_template") \
  X(MQTT_ICON, "ic", "icon") \
  X(MQTT_INITIAL, "init", "initial") \
  X(MQTT_JSON_ATTRIBUTES, "json_attr", "json_attributes") \
  X(MQTT_JSON_ATTRIBUTES_TEMPLATE, "json_attr_tpl", "json_attributes_template") \
  X(MQTT_JSON_ATTRIBUTES_TOPIC, "json_attr_t", "json_attributes_topic") \
  X(MQTT_LAST_RESET_TOPIC, "lrst_t", "last_reset_topic") \
  X(MQTT_LAST_RESET_VALUE_TEMPLATE, "lrst_val_tpl", "last_reset_value_template") \
  X(MQTT_MAX, "max", "max") \
  X(MQTT_MAX_HUMIDITY, "max_hum", "max_humidity") \
  X(MQTT_MAX_MIREDS, "max_mirs", "max_mireds") \
  X(MQTT_MAX_TEMP, "max_temp", "max_temp") \
  X(MQTT_MIN, "min", "min") \
  X(MQTT_MIN_HUMIDITY, "min_hum", "min_humidity") \
  X(MQTT_MIN_MIREDS, "min_mirs", "min_mireds") \
  X(MQTT_MIN_TEMP, "min_temp", "min_temp") \
  X(MQTT_MODE, "mode", "mode") \
  X(MQTT_MODE_COMMAND_TEMPLATE, "mode_cmd_tpl", "mode_command_template") \
  X(MQTT_MODE_COMMAND_TOPIC, "mode_cmd_t", "mode_command_topic") \
  X(MQTT_MODE_STATE_TEMPLATE, "mode_stat_tpl", "mode_state_template") \
  X(MQTT_MODE_STATE_TOPIC, "mode_stat_t", "mode_state_topic") \
  X(MQTT_MODES, "modes", "modes") \
  X(MQTT_NAME, "name", "name") \
  X(MQTT_OBJECT_ID, "obj_id", "object_id") \
  X(MQTT_OFF_DELAY, "off_dly", "off_delay") \
  X(MQTT_ON_COMMAND_TYPE, "on_cmd_type", "on_command_type") \
  X(MQTT_OPTIMISTIC, "opt", "optimistic") \
  X(MQTT_OPTIONS, "ops", "options") \
  X(MQTT_OSCILLATION_COMMAND_TEMPLATE, "osc_cmd_tpl", "oscillation_command_template") \
  X(MQTT_OSCILLATION_COMMAND_TOPIC, "osc_cmd_t", "oscillation_command_topic") \
  X(MQTT_OSCILLATION_STATE_TOPIC, "osc_stat_t", "oscillation_state_topic") \
  X(MQTT_OSCILLATION_VALUE_TEMPLATE, "osc_val_tpl", "oscillation_value_template") \
  X(MQTT_PAYLOAD, "pl", "payload") \
  X(MQTT_PAYLOAD_ARM_AWAY, "pl_arm_away", "payload_arm_away") \
  X(MQTT_PAYLOAD_ARM_CUSTOM_BYPASS, "pl_arm_custom_b", "payload_arm_custom_bypass") \
  X(MQTT_PAYLOAD_ARM_HOME, "pl_arm_home", "payload_arm_home") \
  X(MQTT_PAYLOAD_ARM_NIGHT, "pl_arm_nite", "payload_arm_night") \
  X(MQTT_PAYLOAD_ARM_VACATION, "pl_arm_vacation", "payload_arm_vacation") \
  X(MQTT_PAYLOAD_AVAILABLE, "pl_avail", "payload_available") \
  X(MQTT_PAYLOAD_CLEAN_SPOT, "pl_cln_sp", "payload_clean_spot") \
  X(MQTT_PAYLOAD_CLOSE, "pl_cls", "payload_close") \
  X(MQTT_PAYLOAD_DISARM, "pl_disarm", "payload_disarm") \
  X(MQTT_PAYLOAD_HIGH_SPEED, "pl_hi_spd", "payload_high_speed") \
  X(MQTT_PAYLOAD_HOME, "pl_home", "payload_home") \
  X(MQTT_PAYLOAD_INSTALL, "pl_inst", "payload_install") \
  X(MQTT_PAYLOAD_LOCATE, "pl_loc", "payload_locate") \
  X(MQTT_PAYLOAD_LOCK, "pl_lock", "payload_lock") \
  X(MQTT_PAYLOAD_LOW_SPEED, "pl_lo_spd", "payload_low_speed") \
  X(MQTT_PAYLOAD_MEDIUM_SPEED, "pl_med_spd", "payload_medium_speed") \
  X(MQTT_PAYLOAD_NOT_AVAILABLE, "pl_not_avail", "payload_not_available") \
  X(MQTT_PAYLOAD_NOT_HOME, "pl_not_home", "payload_not_home") \
  X(MQTT_PAYLOAD_OFF, "pl_off", "payload_off") \
  X(MQTT_PAYLOAD_OFF_SPEED, "pl_off_spd", "payload_off_speed") \
  X(MQTT_PAYLOAD_ON, "pl_on", "payload_on") \
  X(MQTT_PAYLOAD_OPEN, "pl_open", "payload_open") \
  X(MQTT_PAYLOAD_OSCILLATION_OFF, "pl_osc_off", "payload_oscillation_off") \
  X(MQTT_PAYLOAD_OSCILLATION_ON, "pl_osc_on", "payload_oscillation_on") \
  X(MQTT_PAYLOAD_PAUSE, "pl_paus", "payload_pause") \
  X(MQTT_PAYLOAD_RESET, "pl_rst", "payload_reset") \
  X(MQTT_PAYLOAD_RESET_HUMIDITY, "pl_rst_hum", "payload_reset_humidity") \
  X(MQTT_PAYLOAD_RESET_MODE, "pl_rst_mode", "payload_reset_mode") \
  X(MQTT_PAYLOAD_RESET_PERCENTAGE, "pl_rst_pct", "payload_reset_percentage") \
  X(MQTT_PAYLOAD_RESET_PRESET_MODE, "pl_rst_pr_mode", "payload_reset_preset_mode") \
  X(MQTT_PAYLOAD_RETURN_TO_BASE, "pl_ret", "payload_return_to_base") \
  X(MQTT_PAYLOAD_START, "pl_strt", "payload_start") \
  X(MQTT_PAYLOAD_START_PAUSE, "pl_stpa", "payload_start_pause") \
  X(MQTT_PAYLOAD_STOP, "pl_stop", "payload_stop") \
  X(MQTT_PAYLOAD_TURN_OFF, "pl_toff", "payload_turn_off") \
  X(MQTT_PAYLOAD_TURN_ON, "pl_ton", "payload_turn_on") \
  X(MQTT_PAYLOAD_UNLOCK, "pl_unlk", "payload_unlock") \
  X(MQTT_PERCENTAGE_COMMAND_TEMPLATE, "pct_cmd_tpl", "percentage_command_template") \
  X(MQTT_PERCENTAGE_COMMAND_TOPIC, "pct_cmd_t", "percentage_command_topic") \
  X(MQTT_PERCENTAGE_STATE_TOPIC, "pct_stat_t", "percentage_state_topic") \
  X(MQTT_PERCENTAGE_VALUE_TEMPLATE, "pct_val_tpl", "percentage_value_template") \
  X(MQTT_POSITION_CLOSED, "pos_clsd", "position_closed") \
  X(MQTT_POSITION_OPEN, "pos_open", "position_open") \
  X(MQTT_POSITION_TEMPLATE, "pos_tpl", "position_template") \
  X(MQTT_POSITION_TOPIC, "pos_t", "position_topic") \
  X(MQTT_POWER_COMMAND_TOPIC, "pow_cmd_t", "power_command_topic") \
  X(MQTT_POWER_STATE_TEMPLATE, "pow_stat_tpl", "power_state_template") \
  X(MQTT_POWER_STATE_TOPIC, "pow_stat_t", "power_state_topic") \
  X(MQTT_PRESET_MODE_COMMAND_TEMPLATE, "pr_mode_cmd_tpl", "preset_mode_command_template") \
  X(MQTT_PRESET_MODE_COMMAND_TOPIC, "pr_mode_cmd_t", "preset_mode_command_topic") \
  X(MQTT_PRESET_MODE_STATE_TOPIC, "pr_mode_stat_t", "preset_mode_state_topic") \
  X(MQTT_PRESET_MODE_VALUE_TEMPLATE, "pr_mode_val_tpl", "preset_mode_value_template") \
  X(MQTT_PRESET_MODES, "pr_modes", "preset_modes") \
  X(MQTT_QOS, "qos", "qos") \
  X(MQTT_RED_TEMPLATE, "r_tpl", "red_template") \
  X(MQTT_RETAIN, "ret", "retain") \
  X(MQTT_RGB_COMMAND_TEMPLATE, "rgb_cmd_tpl", "rgb_command_template") \
  X(MQTT_RGB_COMMAND_TOPIC, "rgb_cmd_t", "rgb_command_topic") \
  X(MQTT_RGB_STATE_TOPIC, "rgb_stat_t", "rgb_state_topic") \
  X(MQTT_RGB_VALUE_TEMPLATE, "rgb_val_tpl", "rgb_value_template") \
  X(MQTT_RGBW_COMMAND_TEMPLATE, "rgbw_cmd_tpl", "rgbw_command_template") \
  X(MQTT_RGBW_COMMAND_TOPIC, "rgbw_cmd_t", "rgbw_command_topic") \
  X(MQTT_RGBW_STATE_TOPIC, "rgbw_stat_t", "rgbw_state_topic") \
  X(MQTT_RGBW_VALUE_TEMPLATE, "rgbw_val_tpl", "rgbw_value_template") \
  X(MQTT_RGBWW_COMMAND_TEMPLATE, "rgbww_cmd_tpl", "rgbww_command_template") \
  X(MQTT_RGBWW_COMMAND_TOPIC, "rgbww_cmd_t", "rgbww_command_topic") \
  X(MQTT_RGBWW_STATE_TOPIC, "rgbww_stat_t", "rgbww_state_topic") \
  X(MQTT_RGBWW_VALUE_TEMPLATE, "rgbww_val_tpl", "rgbww_value_template") \
  X(MQTT_SEND_COMMAND_TOPIC, "send_cmd_t", "send_command_topic") \
  X(MQTT_SEND_IF_OFF, "send_if_off", "send_if_off") \
  X(MQTT_SET_FAN_SPEED_TOPIC, "set_fan_spd_t", "set_fan_speed_topic") \
  X(MQTT_SET_POSITION_TEMPLATE, "set_pos_tpl", "set_position_template") \
  X(MQTT_SET_POSITION_TOPIC, "set_pos_t", "set_position_topic") \
  X(MQTT_SOURCE_TYPE, "src_type", "source_type") \
  X(MQTT_SPEED_COMMAND_TOPIC, "spd_cmd_t", "speed_command_topic") \
  X(MQTT_SPEED_RANGE_MAX, "spd_rng_max", "speed_range_max") \
  X(MQTT_SPEED_RANGE_MIN, "spd_rng_min", "speed_range_min") \
  X(MQTT_SPEED_STATE_TOPIC, "spd_stat_t", "speed_state_topic") \
  X(MQTT_SPEED_VALUE_TEMPLATE, "spd_val_tpl", "speed_value_template") \
  X(MQTT_SPEEDS, "spds", "speeds") \
  X(MQTT_STATE_CLASS, "stat_cla", "state_class") \
  X(MQTT_STATE_CLOSED, "stat_clsd", "state_closed") \
  X(MQTT_STATE_CLOSING, "stat_closing", "state_closing") \
  X(MQTT_STATE_LOCKED, "stat_locked", "state_locked") \
  X(MQTT_STATE_OFF, "stat_off", "state_off") \
  X(MQTT_STATE_ON, "stat_on", "state_on") \
  X(MQTT_STATE_OPEN, "stat_open", "state_open") \
  X(MQTT_STATE_OPENING, "stat_opening", "state_opening") \
  X(MQTT_STATE_STOPPED, "stat_stopped", "state_stopped") \
  X(MQTT_STATE_TEMPLATE, "stat_tpl", "state_template") \
  X(MQTT_STATE_TOPIC, "stat_t", "state_topic") \
  X(MQTT_STATE_UNLOCKED, "stat_unlocked", "state_unlocked") \
  X(MQTT_STATE_VALUE_TEMPLATE, "stat_val_tpl", "state_value_template") \
  X(MQTT_STEP, "step", "step") \
  X(MQTT_SUBTYPE, "stype", "subtype") \
  X(MQTT_SUPPORTED_COLOR_MODES, "sup_clrm", "supported_color_modes") \
  X(MQTT_SUPPORTED_FEATURES, "sup_feat", "supported_features") \
  X(MQTT_SWING_MODE_COMMAND_TEMPLATE, "swing_mode_cmd_tpl", "swing_mode_command_template") \
  X(MQTT_SWING_MODE_COMMAND_TOPIC, "swing_mode_cmd_t", "swing_mode_command_topic") \
  X(MQTT_SWING_MODE_STATE_TEMPLATE, "swing_mode_stat_tpl", "swing_mode_state_template") \
  X(MQTT_SWING_MODE_STATE_TOPIC, "swing_mode_stat_t", "swing_mode_state_topic") \
  X(MQTT_TARGET_HUMIDITY_COMMAND_TEMPLATE, "hum_cmd_tpl", "target_humidity_command_template") \
  X(MQTT_TARGET_HUMIDITY_COMMAND_TOPIC, "hum_cmd_t", "target_humidity_command_topic") \
  X(MQTT_TARGET_HUMIDITY_STATE_TEMPLATE, "hum_state_tpl", "target_humidity_state_template") \
  X(MQTT_TARGET_HUMIDITY_STATE_TOPIC, "hum_stat_t", "target_humidity_state_topic") \
  X(MQTT_TARGET_TEMPERATURE_STEP, "temp_step", "temp_step") \
  X(MQTT_TEMPERATURE_COMMAND_TEMPLATE, "temp_cmd_tpl", "temperature_command_template") \
  X(MQTT_TEMPERATURE_COMMAND_TOPIC, "temp_cmd_t", "temperature_command_topic") \
  X(MQTT_TEMPERATURE_HIGH_COMMAND_TEMPLATE, "temp_hi_cmd_tpl", "temperature_high_command_template") \
  X(MQTT_TEMPERATURE_HIGH_COMMAND_TOPIC, "temp_hi_cmd_t", "temperature_high_command_topic") \
  X(MQTT_TEMPERATURE_HIGH_STATE_TEMPLATE, "temp_hi_stat_tpl", "temperature_high_state_template") \
  X(MQTT_TEMPERATURE_HIGH_STATE_TOPIC, "temp_hi_stat_t", "temperature_high_state_topic") \
  X(MQTT_TEMPERATURE_LOW_COMMAND_TEMPLATE, "temp_lo_cmd_tpl", "temperature_low_command_template") \
  X(MQTT_TEMPERATURE_LOW_COMMAND_TOPIC, "temp_lo_cmd_t", "temperature_low_command_topic") \
  X(MQTT_TEMPERATURE_LOW_STATE_TEMPLATE, "temp_lo_stat_tpl", "temperature_low_state_template") \
  X(MQTT_TEMPERATURE_LOW_STATE_TOPIC, "temp_lo_stat_t", "temperature_low_state_topic") \
  X(MQTT_TEMPERATURE_STATE_TEMPLATE, "temp_stat_tpl", "temperature_state_template") \
  X(MQTT_TEMPERATURE_STATE_TOPIC, "temp_stat_t", "temperature_state_topic") \
  X(MQTT_TEMPERATURE_UNIT, "temp_unit", "temperature_unit") \
  X(MQTT_TILT_CLOSED_VALUE, "tilt_clsd_val", "tilt_closed_value") \
  X(MQTT_TILT_COMMAND_TEMPLATE, "tilt_cmd_tpl", "tilt_command_template") \
  X(MQTT_TILT_COMMAND_TOPIC, "tilt_cmd_t", "tilt_command_topic") \
  X(MQTT_TILT_INVERT_STATE, "tilt_inv_stat", "tilt_invert_state") \
  X(MQTT_TILT_MAX, "tilt_max", "tilt_max") \
  X(MQTT_TILT_MIN, "tilt_min", "tilt_min") \
  X(MQTT_TILT_OPENED_VALUE, "tilt_opnd_val", "tilt_opened_value") \
  X(MQTT_TILT_OPTIMISTIC, "tilt_opt", "tilt_optimistic") \
  X(MQTT_TILT_STATUS_TEMPLATE, "tilt_status_tpl", "tilt_status_template") \
  X(MQTT_TILT_STATUS_TOPIC, "tilt_status_t", "tilt_status_topic") \
  X(MQTT_TOPIC, "t", "topic") \
  X(MQTT_UNIQUE_ID, "uniq_id", "unique_id") \
  X(MQTT_UNIT_OF_MEASUREMENT, "unit_of_meas", "unit_of_measurement") \
  X(MQTT_VALUE_TEMPLATE, "val_tpl", "value_template") \
  X(MQTT_WHITE_COMMAND_TOPIC, "whit_cmd_t", "white_command_topic") \
  X(MQTT_WHITE_SCALE, "whit_scl", "white_scale") \
  X(MQTT_WHITE_VALUE_COMMAND_TOPIC, "whit_val_cmd_t", "white_value_command_topic") \
  X(MQTT_WHITE_VALUE_SCALE, "whit_val_scl", "white_value_scale") \
  X(MQTT_WHITE_VALUE_STATE_TOPIC, "whit_val_stat_t", "white_value_state_topic") \
  X(MQTT_WHITE_VALUE_TEMPLATE, "whit_val_tpl", "white_value_template") \
  X(MQTT_XY_COMMAND_TOPIC, "xy_cmd_t", "xy_command_topic") \
  X(MQTT_XY_STATE_TOPIC, "xy_stat_t", "xy_state_topic") \
  X(MQTT_XY_VALUE_TEMPLATE, "xy_val_tpl", "xy_value_template")
// clang-format on

#ifdef USE_ESP8266
// ESP8266: Store strings in PROGMEM (flash) and expose as __FlashStringHelper* pointers.
// ArduinoJson recognizes this type and reads from flash memory.
namespace esphome::mqtt {

// Generate PROGMEM data arrays
#ifdef USE_MQTT_ABBREVIATIONS
#define MQTT_DATA(name, abbr, full) static const char name##_data[] PROGMEM = abbr;
#else
#define MQTT_DATA(name, abbr, full) static const char name##_data[] PROGMEM = full;
#endif
MQTT_KEYS_LIST(MQTT_DATA)
#undef MQTT_DATA

// Generate flash string pointers from the PROGMEM data
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define MQTT_PTR(name, abbr, full) \
  static const __FlashStringHelper *const name = reinterpret_cast<const __FlashStringHelper *>(name##_data);
MQTT_KEYS_LIST(MQTT_PTR)
#undef MQTT_PTR

}  // namespace esphome::mqtt
#else
// Other platforms: constexpr in namespace
namespace esphome::mqtt {
#ifdef USE_MQTT_ABBREVIATIONS
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define MQTT_CONST(name, abbr, full) constexpr const char *name = abbr;
#else
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define MQTT_CONST(name, abbr, full) constexpr const char *name = full;
#endif
MQTT_KEYS_LIST(MQTT_CONST)
#undef MQTT_CONST
}  // namespace esphome::mqtt
#endif

#endif  // USE_MQTT
