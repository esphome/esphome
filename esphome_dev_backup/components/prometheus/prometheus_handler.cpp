#include "prometheus_handler.h"
#ifdef USE_NETWORK
#include "esphome/core/application.h"

namespace esphome {
namespace prometheus {

void PrometheusHandler::handleRequest(AsyncWebServerRequest *req) {
  AsyncResponseStream *stream = req->beginResponseStream("text/plain; version=0.0.4; charset=utf-8");
  std::string area = App.get_area();
  std::string node = App.get_name();
  std::string friendly_name = App.get_friendly_name();

#ifdef USE_SENSOR
  this->sensor_type_(stream);
  for (auto *obj : App.get_sensors())
    this->sensor_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_BINARY_SENSOR
  this->binary_sensor_type_(stream);
  for (auto *obj : App.get_binary_sensors())
    this->binary_sensor_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_FAN
  this->fan_type_(stream);
  for (auto *obj : App.get_fans())
    this->fan_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_LIGHT
  this->light_type_(stream);
  for (auto *obj : App.get_lights())
    this->light_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_COVER
  this->cover_type_(stream);
  for (auto *obj : App.get_covers())
    this->cover_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_SWITCH
  this->switch_type_(stream);
  for (auto *obj : App.get_switches())
    this->switch_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_LOCK
  this->lock_type_(stream);
  for (auto *obj : App.get_locks())
    this->lock_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_TEXT_SENSOR
  this->text_sensor_type_(stream);
  for (auto *obj : App.get_text_sensors())
    this->text_sensor_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_NUMBER
  this->number_type_(stream);
  for (auto *obj : App.get_numbers())
    this->number_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_SELECT
  this->select_type_(stream);
  for (auto *obj : App.get_selects())
    this->select_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_MEDIA_PLAYER
  this->media_player_type_(stream);
  for (auto *obj : App.get_media_players())
    this->media_player_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_UPDATE
  this->update_entity_type_(stream);
  for (auto *obj : App.get_updates())
    this->update_entity_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_VALVE
  this->valve_type_(stream);
  for (auto *obj : App.get_valves())
    this->valve_row_(stream, obj, area, node, friendly_name);
#endif

#ifdef USE_CLIMATE
  this->climate_type_(stream);
  for (auto *obj : App.get_climates())
    this->climate_row_(stream, obj, area, node, friendly_name);
#endif

  req->send(stream);
}

std::string PrometheusHandler::relabel_id_(EntityBase *obj) {
  auto item = relabel_map_id_.find(obj);
  return item == relabel_map_id_.end() ? obj->get_object_id() : item->second;
}

std::string PrometheusHandler::relabel_name_(EntityBase *obj) {
  auto item = relabel_map_name_.find(obj);
  return item == relabel_map_name_.end() ? obj->get_name() : item->second;
}

void PrometheusHandler::add_area_label_(AsyncResponseStream *stream, std::string &area) {
  if (!area.empty()) {
    stream->print(F("\",area=\""));
    stream->print(area.c_str());
  }
}

void PrometheusHandler::add_node_label_(AsyncResponseStream *stream, std::string &node) {
  if (!node.empty()) {
    stream->print(F("\",node=\""));
    stream->print(node.c_str());
  }
}

void PrometheusHandler::add_friendly_name_label_(AsyncResponseStream *stream, std::string &friendly_name) {
  if (!friendly_name.empty()) {
    stream->print(F("\",friendly_name=\""));
    stream->print(friendly_name.c_str());
  }
}

// Type-specific implementation
#ifdef USE_SENSOR
void PrometheusHandler::sensor_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_sensor_value gauge\n"));
  stream->print(F("#TYPE esphome_sensor_failed gauge\n"));
}
void PrometheusHandler::sensor_row_(AsyncResponseStream *stream, sensor::Sensor *obj, std::string &area,
                                    std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (!std::isnan(obj->state)) {
    // We have a valid value, output this value
    stream->print(F("esphome_sensor_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // Data itself
    stream->print(F("esphome_sensor_value{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",unit=\""));
    stream->print(obj->get_unit_of_measurement().c_str());
    stream->print(F("\"} "));
    stream->print(value_accuracy_to_string(obj->state, obj->get_accuracy_decimals()).c_str());
    stream->print(F("\n"));
  } else {
    // Invalid state
    stream->print(F("esphome_sensor_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

// Type-specific implementation
#ifdef USE_BINARY_SENSOR
void PrometheusHandler::binary_sensor_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_binary_sensor_value gauge\n"));
  stream->print(F("#TYPE esphome_binary_sensor_failed gauge\n"));
}
void PrometheusHandler::binary_sensor_row_(AsyncResponseStream *stream, binary_sensor::BinarySensor *obj,
                                           std::string &area, std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (obj->has_state()) {
    // We have a valid value, output this value
    stream->print(F("esphome_binary_sensor_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // Data itself
    stream->print(F("esphome_binary_sensor_value{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} "));
    stream->print(obj->state);
    stream->print(F("\n"));
  } else {
    // Invalid state
    stream->print(F("esphome_binary_sensor_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

#ifdef USE_FAN
void PrometheusHandler::fan_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_fan_value gauge\n"));
  stream->print(F("#TYPE esphome_fan_failed gauge\n"));
  stream->print(F("#TYPE esphome_fan_speed gauge\n"));
  stream->print(F("#TYPE esphome_fan_oscillation gauge\n"));
}
void PrometheusHandler::fan_row_(AsyncResponseStream *stream, fan::Fan *obj, std::string &area, std::string &node,
                                 std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  stream->print(F("esphome_fan_failed{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} 0\n"));
  // Data itself
  stream->print(F("esphome_fan_value{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} "));
  stream->print(obj->state);
  stream->print(F("\n"));
  // Speed if available
  if (obj->get_traits().supports_speed()) {
    stream->print(F("esphome_fan_speed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} "));
    stream->print(obj->speed);
    stream->print(F("\n"));
  }
  // Oscillation if available
  if (obj->get_traits().supports_oscillation()) {
    stream->print(F("esphome_fan_oscillation{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} "));
    stream->print(obj->oscillating);
    stream->print(F("\n"));
  }
}
#endif

#ifdef USE_LIGHT
void PrometheusHandler::light_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_light_state gauge\n"));
  stream->print(F("#TYPE esphome_light_color gauge\n"));
  stream->print(F("#TYPE esphome_light_effect_active gauge\n"));
}
void PrometheusHandler::light_row_(AsyncResponseStream *stream, light::LightState *obj, std::string &area,
                                   std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  // State
  stream->print(F("esphome_light_state{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} "));
  stream->print(obj->remote_values.is_on());
  stream->print(F("\n"));
  // Brightness and RGBW
  light::LightColorValues color = obj->current_values;
  float brightness, r, g, b, w;
  color.as_brightness(&brightness);
  color.as_rgbw(&r, &g, &b, &w);
  stream->print(F("esphome_light_color{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",channel=\"brightness\"} "));
  stream->print(brightness);
  stream->print(F("\n"));
  stream->print(F("esphome_light_color{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",channel=\"r\"} "));
  stream->print(r);
  stream->print(F("\n"));
  stream->print(F("esphome_light_color{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",channel=\"g\"} "));
  stream->print(g);
  stream->print(F("\n"));
  stream->print(F("esphome_light_color{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",channel=\"b\"} "));
  stream->print(b);
  stream->print(F("\n"));
  stream->print(F("esphome_light_color{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",channel=\"w\"} "));
  stream->print(w);
  stream->print(F("\n"));
  // Effect
  std::string effect = obj->get_effect_name();
  if (effect == "None") {
    stream->print(F("esphome_light_effect_active{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",effect=\"None\"} 0\n"));
  } else {
    stream->print(F("esphome_light_effect_active{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",effect=\""));
    stream->print(effect.c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

#ifdef USE_COVER
void PrometheusHandler::cover_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_cover_value gauge\n"));
  stream->print(F("#TYPE esphome_cover_failed gauge\n"));
}
void PrometheusHandler::cover_row_(AsyncResponseStream *stream, cover::Cover *obj, std::string &area, std::string &node,
                                   std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (!std::isnan(obj->position)) {
    // We have a valid value, output this value
    stream->print(F("esphome_cover_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // Data itself
    stream->print(F("esphome_cover_value{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} "));
    stream->print(obj->position);
    stream->print(F("\n"));
    if (obj->get_traits().get_supports_tilt()) {
      stream->print(F("esphome_cover_tilt{id=\""));
      stream->print(relabel_id_(obj).c_str());
      add_area_label_(stream, area);
      add_node_label_(stream, node);
      add_friendly_name_label_(stream, friendly_name);
      stream->print(F("\",name=\""));
      stream->print(relabel_name_(obj).c_str());
      stream->print(F("\"} "));
      stream->print(obj->tilt);
      stream->print(F("\n"));
    }
  } else {
    // Invalid state
    stream->print(F("esphome_cover_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

#ifdef USE_SWITCH
void PrometheusHandler::switch_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_switch_value gauge\n"));
  stream->print(F("#TYPE esphome_switch_failed gauge\n"));
}
void PrometheusHandler::switch_row_(AsyncResponseStream *stream, switch_::Switch *obj, std::string &area,
                                    std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  stream->print(F("esphome_switch_failed{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} 0\n"));
  // Data itself
  stream->print(F("esphome_switch_value{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} "));
  stream->print(obj->state);
  stream->print(F("\n"));
}
#endif

#ifdef USE_LOCK
void PrometheusHandler::lock_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_lock_value gauge\n"));
  stream->print(F("#TYPE esphome_lock_failed gauge\n"));
}
void PrometheusHandler::lock_row_(AsyncResponseStream *stream, lock::Lock *obj, std::string &area, std::string &node,
                                  std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  stream->print(F("esphome_lock_failed{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} 0\n"));
  // Data itself
  stream->print(F("esphome_lock_value{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} "));
  stream->print(obj->state);
  stream->print(F("\n"));
}
#endif

// Type-specific implementation
#ifdef USE_TEXT_SENSOR
void PrometheusHandler::text_sensor_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_text_sensor_value gauge\n"));
  stream->print(F("#TYPE esphome_text_sensor_failed gauge\n"));
}
void PrometheusHandler::text_sensor_row_(AsyncResponseStream *stream, text_sensor::TextSensor *obj, std::string &area,
                                         std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (obj->has_state()) {
    // We have a valid value, output this value
    stream->print(F("esphome_text_sensor_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // Data itself
    stream->print(F("esphome_text_sensor_value{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",value=\""));
    stream->print(obj->state.c_str());
    stream->print(F("\"} "));
    stream->print(F("1.0"));
    stream->print(F("\n"));
  } else {
    // Invalid state
    stream->print(F("esphome_text_sensor_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

// Type-specific implementation
#ifdef USE_NUMBER
void PrometheusHandler::number_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_number_value gauge\n"));
  stream->print(F("#TYPE esphome_number_failed gauge\n"));
}
void PrometheusHandler::number_row_(AsyncResponseStream *stream, number::Number *obj, std::string &area,
                                    std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (!std::isnan(obj->state)) {
    // We have a valid value, output this value
    stream->print(F("esphome_number_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // Data itself
    stream->print(F("esphome_number_value{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} "));
    stream->print(obj->state);
    stream->print(F("\n"));
  } else {
    // Invalid state
    stream->print(F("esphome_number_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

#ifdef USE_SELECT
void PrometheusHandler::select_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_select_value gauge\n"));
  stream->print(F("#TYPE esphome_select_failed gauge\n"));
}
void PrometheusHandler::select_row_(AsyncResponseStream *stream, select::Select *obj, std::string &area,
                                    std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (obj->has_state()) {
    // We have a valid value, output this value
    stream->print(F("esphome_select_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // Data itself
    stream->print(F("esphome_select_value{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",value=\""));
    stream->print(obj->state.c_str());
    stream->print(F("\"} "));
    stream->print(F("1.0"));
    stream->print(F("\n"));
  } else {
    // Invalid state
    stream->print(F("esphome_select_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

#ifdef USE_MEDIA_PLAYER
void PrometheusHandler::media_player_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_media_player_state_value gauge\n"));
  stream->print(F("#TYPE esphome_media_player_volume gauge\n"));
  stream->print(F("#TYPE esphome_media_player_is_muted gauge\n"));
  stream->print(F("#TYPE esphome_media_player_failed gauge\n"));
}
void PrometheusHandler::media_player_row_(AsyncResponseStream *stream, media_player::MediaPlayer *obj,
                                          std::string &area, std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  stream->print(F("esphome_media_player_failed{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} 0\n"));
  // Data itself
  stream->print(F("esphome_media_player_state_value{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",value=\""));
  stream->print(media_player::media_player_state_to_string(obj->state));
  stream->print(F("\"} "));
  stream->print(F("1.0"));
  stream->print(F("\n"));
  stream->print(F("esphome_media_player_volume{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} "));
  stream->print(obj->volume);
  stream->print(F("\n"));
  stream->print(F("esphome_media_player_is_muted{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} "));
  if (obj->is_muted()) {
    stream->print(F("1.0"));
  } else {
    stream->print(F("0.0"));
  }
  stream->print(F("\n"));
}
#endif

#ifdef USE_UPDATE
void PrometheusHandler::update_entity_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_update_entity_state gauge\n"));
  stream->print(F("#TYPE esphome_update_entity_info gauge\n"));
  stream->print(F("#TYPE esphome_update_entity_failed gauge\n"));
}

void PrometheusHandler::handle_update_state_(AsyncResponseStream *stream, update::UpdateState state) {
  switch (state) {
    case update::UpdateState::UPDATE_STATE_UNKNOWN:
      stream->print("unknown");
      break;
    case update::UpdateState::UPDATE_STATE_NO_UPDATE:
      stream->print("none");
      break;
    case update::UpdateState::UPDATE_STATE_AVAILABLE:
      stream->print("available");
      break;
    case update::UpdateState::UPDATE_STATE_INSTALLING:
      stream->print("installing");
      break;
    default:
      stream->print("invalid");
      break;
  }
}

void PrometheusHandler::update_entity_row_(AsyncResponseStream *stream, update::UpdateEntity *obj, std::string &area,
                                           std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  if (obj->has_state()) {
    // We have a valid value, output this value
    stream->print(F("esphome_update_entity_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 0\n"));
    // First update state
    stream->print(F("esphome_update_entity_state{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",value=\""));
    handle_update_state_(stream, obj->state);
    stream->print(F("\"} "));
    stream->print(F("1.0"));
    stream->print(F("\n"));
    // Next update info
    stream->print(F("esphome_update_entity_info{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\",current_version=\""));
    stream->print(obj->update_info.current_version.c_str());
    stream->print(F("\",latest_version=\""));
    stream->print(obj->update_info.latest_version.c_str());
    stream->print(F("\",title=\""));
    stream->print(obj->update_info.title.c_str());
    stream->print(F("\"} "));
    stream->print(F("1.0"));
    stream->print(F("\n"));
  } else {
    // Invalid state
    stream->print(F("esphome_update_entity_failed{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} 1\n"));
  }
}
#endif

#ifdef USE_VALVE
void PrometheusHandler::valve_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_valve_operation gauge\n"));
  stream->print(F("#TYPE esphome_valve_failed gauge\n"));
  stream->print(F("#TYPE esphome_valve_position gauge\n"));
}

void PrometheusHandler::valve_row_(AsyncResponseStream *stream, valve::Valve *obj, std::string &area, std::string &node,
                                   std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  stream->print(F("esphome_valve_failed{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\"} 0\n"));
  // Data itself
  stream->print(F("esphome_valve_operation{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",operation=\""));
  stream->print(valve::valve_operation_to_str(obj->current_operation));
  stream->print(F("\"} "));
  stream->print(F("1.0"));
  stream->print(F("\n"));
  // Now see if position is supported
  if (obj->get_traits().get_supports_position()) {
    stream->print(F("esphome_valve_position{id=\""));
    stream->print(relabel_id_(obj).c_str());
    add_area_label_(stream, area);
    add_node_label_(stream, node);
    add_friendly_name_label_(stream, friendly_name);
    stream->print(F("\",name=\""));
    stream->print(relabel_name_(obj).c_str());
    stream->print(F("\"} "));
    stream->print(obj->position);
    stream->print(F("\n"));
  }
}
#endif

#ifdef USE_CLIMATE
void PrometheusHandler::climate_type_(AsyncResponseStream *stream) {
  stream->print(F("#TYPE esphome_climate_setting gauge\n"));
  stream->print(F("#TYPE esphome_climate_value gauge\n"));
  stream->print(F("#TYPE esphome_climate_failed gauge\n"));
}

void PrometheusHandler::climate_setting_row_(AsyncResponseStream *stream, climate::Climate *obj, std::string &area,
                                             std::string &node, std::string &friendly_name, std::string &setting,
                                             const LogString *setting_value) {
  stream->print(F("esphome_climate_setting{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",category=\""));
  stream->print(setting.c_str());
  stream->print(F("\",setting_value=\""));
  stream->print(LOG_STR_ARG(setting_value));
  stream->print(F("\"} "));
  stream->print(F("1.0"));
  stream->print(F("\n"));
}

void PrometheusHandler::climate_value_row_(AsyncResponseStream *stream, climate::Climate *obj, std::string &area,
                                           std::string &node, std::string &friendly_name, std::string &category,
                                           std::string &climate_value) {
  stream->print(F("esphome_climate_value{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",category=\""));
  stream->print(category.c_str());
  stream->print(F("\"} "));
  stream->print(climate_value.c_str());
  stream->print(F("\n"));
}

void PrometheusHandler::climate_failed_row_(AsyncResponseStream *stream, climate::Climate *obj, std::string &area,
                                            std::string &node, std::string &friendly_name, std::string &category,
                                            bool is_failed_value) {
  stream->print(F("esphome_climate_failed{id=\""));
  stream->print(relabel_id_(obj).c_str());
  add_area_label_(stream, area);
  add_node_label_(stream, node);
  add_friendly_name_label_(stream, friendly_name);
  stream->print(F("\",name=\""));
  stream->print(relabel_name_(obj).c_str());
  stream->print(F("\",category=\""));
  stream->print(category.c_str());
  stream->print(F("\"} "));
  if (is_failed_value) {
    stream->print(F("1.0"));
  } else {
    stream->print(F("0.0"));
  }
  stream->print(F("\n"));
}

void PrometheusHandler::climate_row_(AsyncResponseStream *stream, climate::Climate *obj, std::string &area,
                                     std::string &node, std::string &friendly_name) {
  if (obj->is_internal() && !this->include_internal_)
    return;
  // Data itself
  bool any_failures = false;
  std::string climate_mode_category = "mode";
  const auto *climate_mode_value = climate::climate_mode_to_string(obj->mode);
  climate_setting_row_(stream, obj, area, node, friendly_name, climate_mode_category, climate_mode_value);
  const auto traits = obj->get_traits();
  // Now see if traits is supported
  int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
  int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
  // max temp
  std::string max_temp = "maximum_temperature";
  auto max_temp_value = value_accuracy_to_string(traits.get_visual_max_temperature(), target_accuracy);
  climate_value_row_(stream, obj, area, node, friendly_name, max_temp, max_temp_value);
  // max temp
  std::string min_temp = "mininum_temperature";
  auto min_temp_value = value_accuracy_to_string(traits.get_visual_min_temperature(), target_accuracy);
  climate_value_row_(stream, obj, area, node, friendly_name, min_temp, min_temp_value);
  // now check optional traits
  if (traits.get_supports_current_temperature()) {
    std::string current_temp = "current_temperature";
    if (std::isnan(obj->current_temperature)) {
      climate_failed_row_(stream, obj, area, node, friendly_name, current_temp, true);
      any_failures = true;
    } else {
      auto current_temp_value = value_accuracy_to_string(obj->current_temperature, current_accuracy);
      climate_value_row_(stream, obj, area, node, friendly_name, current_temp, current_temp_value);
      climate_failed_row_(stream, obj, area, node, friendly_name, current_temp, false);
    }
  }
  if (traits.get_supports_current_humidity()) {
    std::string current_humidity = "current_humidity";
    if (std::isnan(obj->current_humidity)) {
      climate_failed_row_(stream, obj, area, node, friendly_name, current_humidity, true);
      any_failures = true;
    } else {
      auto current_humidity_value = value_accuracy_to_string(obj->current_humidity, 0);
      climate_value_row_(stream, obj, area, node, friendly_name, current_humidity, current_humidity_value);
      climate_failed_row_(stream, obj, area, node, friendly_name, current_humidity, false);
    }
  }
  if (traits.get_supports_target_humidity()) {
    std::string target_humidity = "target_humidity";
    if (std::isnan(obj->target_humidity)) {
      climate_failed_row_(stream, obj, area, node, friendly_name, target_humidity, true);
      any_failures = true;
    } else {
      auto target_humidity_value = value_accuracy_to_string(obj->target_humidity, 0);
      climate_value_row_(stream, obj, area, node, friendly_name, target_humidity, target_humidity_value);
      climate_failed_row_(stream, obj, area, node, friendly_name, target_humidity, false);
    }
  }
  if (traits.get_supports_two_point_target_temperature()) {
    std::string target_temp_low = "target_temperature_low";
    auto target_temp_low_value = value_accuracy_to_string(obj->target_temperature_low, target_accuracy);
    climate_value_row_(stream, obj, area, node, friendly_name, target_temp_low, target_temp_low_value);
    std::string target_temp_high = "target_temperature_high";
    auto target_temp_high_value = value_accuracy_to_string(obj->target_temperature_high, target_accuracy);
    climate_value_row_(stream, obj, area, node, friendly_name, target_temp_high, target_temp_high_value);
  } else {
    std::string target_temp = "target_temperature";
    auto target_temp_value = value_accuracy_to_string(obj->target_temperature, target_accuracy);
    climate_value_row_(stream, obj, area, node, friendly_name, target_temp, target_temp_value);
  }
  if (traits.get_supports_action()) {
    std::string climate_trait_category = "action";
    const auto *climate_trait_value = climate::climate_action_to_string(obj->action);
    climate_setting_row_(stream, obj, area, node, friendly_name, climate_trait_category, climate_trait_value);
  }
  if (traits.get_supports_fan_modes()) {
    std::string climate_trait_category = "fan_mode";
    if (obj->fan_mode.has_value()) {
      const auto *climate_trait_value = climate::climate_fan_mode_to_string(obj->fan_mode.value());
      climate_setting_row_(stream, obj, area, node, friendly_name, climate_trait_category, climate_trait_value);
      climate_failed_row_(stream, obj, area, node, friendly_name, climate_trait_category, false);
    } else {
      climate_failed_row_(stream, obj, area, node, friendly_name, climate_trait_category, true);
      any_failures = true;
    }
  }
  if (traits.get_supports_presets()) {
    std::string climate_trait_category = "preset";
    if (obj->preset.has_value()) {
      const auto *climate_trait_value = climate::climate_preset_to_string(obj->preset.value());
      climate_setting_row_(stream, obj, area, node, friendly_name, climate_trait_category, climate_trait_value);
      climate_failed_row_(stream, obj, area, node, friendly_name, climate_trait_category, false);
    } else {
      climate_failed_row_(stream, obj, area, node, friendly_name, climate_trait_category, true);
      any_failures = true;
    }
  }
  if (traits.get_supports_swing_modes()) {
    std::string climate_trait_category = "swing_mode";
    const auto *climate_trait_value = climate::climate_swing_mode_to_string(obj->swing_mode);
    climate_setting_row_(stream, obj, area, node, friendly_name, climate_trait_category, climate_trait_value);
  }
  std::string all_climate_category = "all";
  climate_failed_row_(stream, obj, area, node, friendly_name, all_climate_category, any_failures);
}
#endif

}  // namespace prometheus
}  // namespace esphome
#endif
