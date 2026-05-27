#include "dfrobot_c4004.h"

namespace esphome {
namespace dfrobot_c4004 {

static const char *const TAG = "dfrobot_c4004";

void C4004Component::setup() {
  this->flush_input();
  this->connected_ = this->begin();
  this->publish_online(this->connected_);

  if (!this->connected_) {
    ESP_LOGW(TAG, "C4004 did not respond during setup");
    this->publish_status("C4004 setup failed: no response");
    this->publish_all_states();
    return;
  }

  ESP_LOGI(TAG, "C4004 setup successful");
  this->sync_device_state();
  this->publish_status("C4004 setup successful");
}

void C4004Component::loop() {
  const uint32_t now = millis();
  const ReportedEvent event = this->get_reported_info(5);

  if (event != EVENT_NONE) {
    if (event == EVENT_HEARTBEAT) {
      this->connected_ = true;
      this->publish_online(true);
    } else if (event == EVENT_PRESENCE) {
      this->publish_presence_state();
    } else if (event == EVENT_MOTION) {
      this->publish_motion_state();
    } else if (event == EVENT_TRAJECTORY) {
      this->publish_target_count_number();
    } else if (event == EVENT_PEOPLE_COUNT) {
      this->publish_people_count();
    }
  }

  if (now - this->last_active_query_ms_ >= 2000UL) {
    this->last_active_query_ms_ = now;
    this->get_presence_state();
    this->get_motion_state();
    this->get_people_count_info(GET_DATA_REPORT);
    this->get_target_count_active();
    this->publish_presence_state();
    this->publish_motion_state();
    this->publish_people_count();
    this->publish_target_count_number();
  }

  if (now - this->last_heartbeat_query_ms_ >= 10000UL) {
    this->last_heartbeat_query_ms_ = now;
    this->connected_ = this->is_connected();
    this->publish_online(this->connected_);
  }

  if (this->last_heartbeat_ms_ != 0 && now - this->last_heartbeat_ms_ > HEARTBEAT_TIMEOUT_MS) {
    this->connected_ = false;
    this->heartbeat_ = false;
    this->publish_online(false);
  }
}

void C4004Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DFRobot C4004");
  ESP_LOGCONFIG(TAG, "  Connected: %s", TRUEFALSE(this->connected_));
  ESP_LOGCONFIG(TAG, "  Install mode: %s", this->install_mode_to_string(this->install_info_.mode));
  ESP_LOGCONFIG(TAG, "  Install height: %u cm", this->install_info_.height_cm);
  ESP_LOGCONFIG(TAG, "  Install Z angle: %d deg", this->install_info_.z_angle);
  ESP_LOGCONFIG(TAG, "  Detection range mode: %s", this->range_mode_to_string(this->range_info_.mode));
}

bool C4004Component::begin() {
  const uint32_t start = millis();
  while (millis() - start < 1200UL) {
    if (this->is_init_finished()) {
      return true;
    }
    delay(20);
  }
  return this->is_connected();
}

bool C4004Component::is_init_finished() {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  if (this->request_frame(CTRL_WORK_STATUS, CMD_WORK_STATUS_INIT_FINISHED_QUERY, &data, 1, &packet)) {
    if (packet.len > 0) {
      this->init_finished_ = packet.data[0] == 0x01;
    }
  }
  return this->init_finished_;
}

bool C4004Component::is_connected() {
  if (this->get_heartbeat(GET_DATA_ACTIVE)) {
    return true;
  }
  return this->last_heartbeat_ms_ != 0 && millis() - this->last_heartbeat_ms_ < HEARTBEAT_TIMEOUT_MS;
}

bool C4004Component::get_heartbeat(GetDataMode mode) {
  if (mode == GET_DATA_REPORT) {
    return this->heartbeat_;
  }

  const uint8_t data = QUERY_DATA;
  Packet packet;
  if (!this->request_frame(CTRL_SYSTEM, CMD_SYSTEM_HEARTBEAT_QUERY, &data, 1, &packet)) {
    return false;
  }
  if (packet.len > 0 && packet.data[0] != QUERY_DATA) {
    return false;
  }
  this->last_heartbeat_ms_ = millis();
  this->heartbeat_ = true;
  return true;
}

ReportedEvent C4004Component::get_reported_info(uint16_t timeout_ms) {
  Packet packet;
  if (!this->read_frame(&packet, timeout_ms)) {
    return EVENT_NONE;
  }
  return this->handle_packet(&packet);
}

bool C4004Component::reset_device() {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  const bool ok = this->request_frame(CTRL_SYSTEM, CMD_SYSTEM_RESET, &data, 1, &packet);
  if (ok) {
    this->publish_status("C4004 reset command sent");
  }
  return ok;
}

bool C4004Component::factory_reset() {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  const bool ok = this->request_frame(CTRL_SYSTEM, CMD_SYSTEM_FACTORY_RESET, &data, 1, &packet);
  if (ok) {
    this->publish_status("C4004 factory reset command sent");
  }
  return ok;
}

bool C4004Component::save_install_settings() {
  const bool ok = this->set_install_info(this->install_info_);
  if (ok) {
    this->publish_install_info();
    this->publish_status("Install settings saved");
  }
  return ok;
}

bool C4004Component::apply_boundary_range() {
  this->range_info_.mode = RANGE_FOUR_SIDE_BOUNDARY;
  const bool ok = [&]() {
    uint8_t data[9];
    Packet packet;
    data[0] = RANGE_FOUR_SIDE_BOUNDARY;
    this->write_sign_bit_int16(&data[1], this->range_info_.x_positive_cm);
    this->write_sign_bit_int16(&data[3], this->range_info_.x_negative_cm);
    this->write_sign_bit_int16(&data[5], this->range_info_.y_positive_cm);
    this->write_sign_bit_int16(&data[7], this->range_info_.y_negative_cm);
    return this->request_frame(CTRL_DETECTION_RANGE, CMD_DETECTION_RANGE_SET_RANGE, data, sizeof(data), &packet);
  }();

  if (ok) {
    this->range_info_.mode = RANGE_FOUR_SIDE_BOUNDARY;
    this->publish_boundary_range();
    this->publish_detection_range_mode();
    this->publish_status("Boundary detection range applied");
  }
  return ok;
}

bool C4004Component::set_trajectory_range_mode() {
  uint8_t data[2];
  Packet packet;
  data[0] = RANGE_TRAJECTORY;
  data[1] = 0x00;
  const bool ok = this->request_frame(CTRL_DETECTION_RANGE, CMD_DETECTION_RANGE_SET_RANGE, data, sizeof(data), &packet);
  if (ok) {
    this->range_info_.mode = RANGE_TRAJECTORY;
    this->publish_detection_range_mode();
    this->publish_status("Trajectory detection range mode applied");
  }
  return ok;
}

bool C4004Component::clear_all_tags() {
  const uint8_t data = 0xFF;
  Packet packet;
  if (!this->request_frame(CTRL_DETECTION_RANGE, CMD_DETECTION_RANGE_CLEAR_TAG, &data, 1, &packet)) {
    return false;
  }
  if (packet.len > 0 && packet.data[0] != 0xFF) {
    return false;
  }
  this->publish_status("All tags cleared");
  return true;
}

bool C4004Component::clear_people_count_command() {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  const bool ok = this->request_frame(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_CLEAR_COUNT, &data, 1, &packet);
  if (ok) {
    this->people_count_ = 0;
    this->publish_people_count();
    this->publish_status("People count cleared");
  }
  return ok;
}

bool C4004Component::write_presence_enable(bool enable) {
  if (!this->set_byte(CTRL_PRESENCE, CMD_PRESENCE_SET_ENABLE, enable ? 1 : 0)) {
    return false;
  }
  this->presence_enable_ = enable;
  this->publish_switch_states();
  return true;
}

bool C4004Component::write_trajectory_tracking(bool enable) {
  if (!this->set_byte(CTRL_TRAJECTORY, CMD_TRAJECTORY_SET_ENABLE, enable ? 1 : 0)) {
    return false;
  }
  this->trajectory_tracking_ = enable;
  this->publish_switch_states();
  return true;
}

bool C4004Component::write_trajectory_led(bool enable) {
  if (!this->set_byte(CTRL_TRAJECTORY, CMD_TRAJECTORY_SET_TRAJECTORY_LED, enable ? 1 : 0)) {
    return false;
  }
  this->trajectory_led_ = enable;
  this->publish_switch_states();
  return true;
}

bool C4004Component::write_motion_led(bool enable) {
  if (!this->set_byte(CTRL_TRAJECTORY, CMD_TRAJECTORY_SET_MOTION_LED, enable ? 1 : 0)) {
    return false;
  }
  this->motion_led_ = enable;
  this->publish_switch_states();
  return true;
}

bool C4004Component::write_people_report_interval(float value) {
  const uint32_t new_value = static_cast<uint32_t>(value);
  if (!this->set_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_SET_REPORT_INTERVAL, new_value)) {
    return false;
  }
  this->people_report_interval_ = new_value;
  this->publish_people_setting_numbers();
  return true;
}

bool C4004Component::write_trajectory_generate_distance(float value) {
  const uint32_t new_value = static_cast<uint32_t>(value);
  if (!this->set_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_SET_TRAJECTORY_DISTANCE, new_value)) {
    return false;
  }
  this->trajectory_generate_distance_ = new_value;
  this->publish_people_setting_numbers();
  return true;
}

bool C4004Component::write_trajectory_hold_time(float value) {
  const uint32_t new_value = static_cast<uint32_t>(value);
  if (!this->set_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_SET_TRAJECTORY_HOLD_TIME, new_value)) {
    return false;
  }
  this->trajectory_hold_time_ = new_value;
  this->publish_people_setting_numbers();
  return true;
}

bool C4004Component::write_no_person_delay(float value) {
  const uint32_t new_value = static_cast<uint32_t>(value);
  if (!this->set_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_SET_NO_PERSON_DELAY, new_value)) {
    return false;
  }
  this->no_person_delay_ = new_value;
  this->publish_people_setting_numbers();
  return true;
}

void C4004Component::set_pending_install_mode(const std::string &value) {
  if (value == "Top") {
    this->install_info_.mode = INSTALL_MODE_TOP;
  } else {
    this->install_info_.mode = INSTALL_MODE_SIDE;
  }
  this->publish_install_info();
}

void C4004Component::set_pending_install_height(float value) {
  this->install_info_.height_cm = static_cast<uint16_t>(value);
  this->publish_install_info();
}

void C4004Component::set_pending_install_z_angle(float value) {
  this->install_info_.z_angle = static_cast<int16_t>(value);
  this->publish_install_info();
}

void C4004Component::set_pending_range_x_max(float value) {
  this->range_info_.x_positive_cm = static_cast<int16_t>(value);
  this->publish_boundary_range();
}

void C4004Component::set_pending_range_x_min(float value) {
  this->range_info_.x_negative_cm = static_cast<int16_t>(value);
  this->publish_boundary_range();
}

void C4004Component::set_pending_range_y_max(float value) {
  this->range_info_.y_positive_cm = static_cast<int16_t>(value);
  this->publish_boundary_range();
}

void C4004Component::set_pending_range_y_min(float value) {
  this->range_info_.y_negative_cm = static_cast<int16_t>(value);
  this->publish_boundary_range();
}

bool C4004Component::get_install_info(InstallInfo *info) {
  if (info == nullptr) {
    return false;
  }

  const uint8_t data = QUERY_DATA;
  Packet packet;
  InstallInfo next;

  if (!this->request_frame(CTRL_INSTALL_INFO, CMD_INSTALL_QUERY_ANGLE, &data, 1, &packet) || packet.len < 6) {
    return false;
  }
  next.x_angle = this->read_int16(&packet.data[0]) / 100;
  next.y_angle = this->read_int16(&packet.data[2]) / 100;
  next.z_angle = this->read_int16(&packet.data[4]) / 100;

  if (!this->request_frame(CTRL_INSTALL_INFO, CMD_INSTALL_QUERY_HEIGHT, &data, 1, &packet) || packet.len < 2) {
    return false;
  }
  next.height_cm = this->read_uint16(packet.data);

  if (!this->request_frame(CTRL_INSTALL_INFO, CMD_INSTALL_QUERY_MODE, &data, 1, &packet) || packet.len < 1) {
    return false;
  }
  next.mode = static_cast<InstallMode>(packet.data[0]);
  *info = next;
  this->install_info_ = next;
  return true;
}

bool C4004Component::set_install_info(const InstallInfo &info) {
  uint8_t angle_data[6];
  uint8_t height_data[2];
  uint8_t mode_data = static_cast<uint8_t>(info.mode);
  Packet packet;

  int32_t x_angle = static_cast<int32_t>(info.x_angle) * 100;
  int32_t y_angle = static_cast<int32_t>(info.y_angle) * 100;
  int32_t z_angle = static_cast<int32_t>(info.z_angle) * 100;
  x_angle = x_angle > 18000 ? 18000 : (x_angle < -18000 ? -18000 : x_angle);
  y_angle = y_angle > 18000 ? 18000 : (y_angle < -18000 ? -18000 : y_angle);
  z_angle = z_angle > 18000 ? 18000 : (z_angle < -18000 ? -18000 : z_angle);

  this->write_int16(&angle_data[0], static_cast<int16_t>(x_angle));
  this->write_int16(&angle_data[2], static_cast<int16_t>(y_angle));
  this->write_int16(&angle_data[4], static_cast<int16_t>(z_angle));
  this->write_uint16(height_data, info.height_cm);

  if (!this->request_frame(CTRL_INSTALL_INFO, CMD_INSTALL_SET_MODE, &mode_data, 1, &packet)) {
    return false;
  }
  if (!this->request_frame(CTRL_INSTALL_INFO, CMD_INSTALL_SET_ANGLE, angle_data, sizeof(angle_data), &packet)) {
    return false;
  }
  return this->request_frame(CTRL_INSTALL_INFO, CMD_INSTALL_SET_HEIGHT, height_data, sizeof(height_data), &packet);
}

bool C4004Component::get_presence_enable(bool *enable) {
  uint8_t value = 0;
  if (enable == nullptr || !this->query_byte(CTRL_PRESENCE, CMD_PRESENCE_QUERY_ENABLE, &value)) {
    return false;
  }
  this->presence_enable_ = value != 0;
  *enable = this->presence_enable_;
  return true;
}

PresenceState C4004Component::get_presence_state() {
  uint8_t value = PRESENCE_UNKNOWN;
  if (this->query_byte(CTRL_PRESENCE, CMD_PRESENCE_QUERY_STATE, &value)) {
    this->presence_state_ = static_cast<PresenceState>(value);
  }
  return this->presence_state_;
}

MotionState C4004Component::get_motion_state() {
  uint8_t value = MOTION_UNKNOWN;
  if (this->query_byte(CTRL_PRESENCE, CMD_PRESENCE_QUERY_MOTION, &value)) {
    this->motion_state_ = static_cast<MotionState>(value);
  }
  return this->motion_state_;
}

bool C4004Component::get_trajectory_tracking(bool *enable) {
  uint8_t value = 0;
  if (enable == nullptr || !this->query_byte(CTRL_TRAJECTORY, CMD_TRAJECTORY_QUERY_ENABLE, &value)) {
    return false;
  }
  this->trajectory_tracking_ = value != 0;
  *enable = this->trajectory_tracking_;
  return true;
}

bool C4004Component::get_trajectory_led(bool *enable) {
  uint8_t value = 0;
  if (enable == nullptr || !this->query_byte(CTRL_TRAJECTORY, CMD_TRAJECTORY_QUERY_TRAJECTORY_LED, &value)) {
    return false;
  }
  this->trajectory_led_ = value != 0;
  *enable = this->trajectory_led_;
  return true;
}

bool C4004Component::get_motion_led(bool *enable) {
  uint8_t value = 0;
  if (enable == nullptr || !this->query_byte(CTRL_TRAJECTORY, CMD_TRAJECTORY_QUERY_MOTION_LED, &value)) {
    return false;
  }
  this->motion_led_ = value != 0;
  *enable = this->motion_led_;
  return true;
}

uint8_t C4004Component::get_target_count_active() {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  this->request_frame(CTRL_TRAJECTORY, CMD_TRAJECTORY_QUERY_TARGET, &data, 1, &packet);
  return this->target_count_;
}

DetectionRangeMode C4004Component::get_detection_range_mode() {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  if (this->request_frame(CTRL_DETECTION_RANGE, CMD_DETECTION_RANGE_QUERY_RANGE, &data, 1, &packet)) {
    return this->range_info_.mode;
  }
  return this->range_info_.mode;
}

bool C4004Component::get_boundary_detection_range(BoundaryDetectionRange *range) {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  if (range == nullptr) {
    return false;
  }
  if (!this->request_frame(CTRL_DETECTION_RANGE, CMD_DETECTION_RANGE_QUERY_RANGE, &data, 1, &packet)) {
    return false;
  }
  *range = this->range_info_;
  return true;
}

uint8_t C4004Component::get_people_count_info(GetDataMode mode) {
  if (mode == GET_DATA_ACTIVE) {
    const uint8_t data = QUERY_DATA;
    Packet packet;
    this->request_frame(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_QUERY_COUNT, &data, 1, &packet);
  }
  return this->people_count_;
}

bool C4004Component::get_people_report_interval(uint32_t *value) {
  return this->query_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_QUERY_REPORT_INTERVAL, value);
}

bool C4004Component::get_trajectory_generate_distance(uint32_t *value) {
  return this->query_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_QUERY_TRAJECTORY_DISTANCE, value);
}

bool C4004Component::get_trajectory_hold_time(uint32_t *value) {
  return this->query_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_QUERY_TRAJECTORY_HOLD_TIME, value);
}

bool C4004Component::get_no_person_delay(uint32_t *value) {
  return this->query_uint32(CTRL_PEOPLE_COUNT, CMD_PEOPLE_COUNT_QUERY_NO_PERSON_DELAY, value);
}

bool C4004Component::set_byte(uint8_t control, uint8_t cmd, uint8_t value) {
  Packet packet;
  return this->request_frame(control, cmd, &value, 1, &packet);
}

bool C4004Component::query_byte(uint8_t control, uint8_t cmd, uint8_t *value) {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  if (value == nullptr || !this->request_frame(control, cmd, &data, 1, &packet) || packet.len < 1) {
    return false;
  }
  *value = packet.data[0];
  return true;
}

bool C4004Component::set_uint32(uint8_t control, uint8_t cmd, uint32_t value) {
  uint8_t data[4];
  Packet packet;
  this->write_uint32(data, value);
  return this->request_frame(control, cmd, data, sizeof(data), &packet);
}

bool C4004Component::query_uint32(uint8_t control, uint8_t cmd, uint32_t *value) {
  const uint8_t data = QUERY_DATA;
  Packet packet;
  if (value == nullptr || !this->request_frame(control, cmd, &data, 1, &packet) || packet.len < 4) {
    return false;
  }
  *value = this->read_uint32(packet.data);
  return true;
}

bool C4004Component::send_command(uint8_t control, uint8_t cmd, const uint8_t *data, uint16_t len) {
  if (len > MAX_PAYLOAD) {
    return false;
  }

  uint8_t frame[MAX_PAYLOAD + 9];
  uint16_t offset = 0;
  uint8_t checksum = 0;

  frame[offset++] = FRAME_HEAD1;
  frame[offset++] = FRAME_HEAD2;
  frame[offset++] = control;
  frame[offset++] = cmd;
  frame[offset++] = static_cast<uint8_t>(len >> 8);
  frame[offset++] = static_cast<uint8_t>(len & 0xFF);
  for (uint16_t i = 0; i < len; i++) {
    frame[offset++] = data == nullptr ? 0 : data[i];
  }
  for (uint16_t i = 0; i < offset; i++) {
    checksum += frame[i];
  }
  frame[offset++] = checksum;
  frame[offset++] = FRAME_TAIL1;
  frame[offset++] = FRAME_TAIL2;
  this->write_array(frame, offset);
  return true;
}

bool C4004Component::request_frame(uint8_t control, uint8_t cmd, const uint8_t *data, uint16_t len, Packet *response,
                                   uint16_t timeout_ms) {
  if (response == nullptr) {
    return false;
  }

  this->flush_input();
  if (!this->send_command(control, cmd, data, len)) {
    return false;
  }

  const uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    const uint16_t elapsed = static_cast<uint16_t>(millis() - start);
    const uint16_t left = elapsed >= timeout_ms ? 1 : timeout_ms - elapsed;
    if (!this->read_frame(response, left)) {
      continue;
    }
    this->handle_packet(response);
    if (response->control == control && response->cmd == cmd) {
      return true;
    }
  }
  return false;
}

bool C4004Component::read_frame(Packet *packet, uint16_t timeout_ms) {
  if (packet == nullptr) {
    return false;
  }

  const uint32_t start = millis();
  uint8_t value = 0;
  uint8_t checksum = 0;
  uint8_t recv_checksum = 0;
  uint8_t tail1 = 0;
  uint8_t tail2 = 0;

  while (millis() - start < timeout_ms) {
    if (!this->read_byte(&value, 1)) {
      continue;
    }
    if (value != FRAME_HEAD1) {
      continue;
    }
    checksum = value;

    if (!this->read_byte(&value, timeout_ms) || value != FRAME_HEAD2) {
      continue;
    }
    checksum += value;

    if (!this->read_byte(&packet->control, timeout_ms)) {
      return false;
    }
    checksum += packet->control;
    if (!this->read_byte(&packet->cmd, timeout_ms)) {
      return false;
    }
    checksum += packet->cmd;
    if (!this->read_byte(&value, timeout_ms)) {
      return false;
    }
    checksum += value;
    packet->len = static_cast<uint16_t>(value) << 8;
    if (!this->read_byte(&value, timeout_ms)) {
      return false;
    }
    checksum += value;
    packet->len |= value;

    if (packet->len > MAX_PAYLOAD) {
      this->flush_input();
      return false;
    }

    for (uint16_t i = 0; i < packet->len; i++) {
      if (!this->read_byte(&packet->data[i], timeout_ms)) {
        return false;
      }
      checksum += packet->data[i];
    }

    if (!this->read_byte(&recv_checksum, timeout_ms)) {
      return false;
    }
    if (!this->read_byte(&tail1, timeout_ms) || !this->read_byte(&tail2, timeout_ms)) {
      return false;
    }
    if (tail1 != FRAME_TAIL1 || tail2 != FRAME_TAIL2 || checksum != recv_checksum) {
      return false;
    }
    return true;
  }
  return false;
}

bool C4004Component::read_byte(uint8_t *value, uint16_t timeout_ms) {
  if (value == nullptr) {
    return false;
  }

  const uint32_t start = millis();
  do {
    if (this->available() > 0) {
      this->read_array(value, 1);
      return true;
    }
    delay(1);
  } while (millis() - start < timeout_ms);
  return false;
}

void C4004Component::flush_input() {
  uint8_t tmp[32];
  while (this->available() > 0) {
    const size_t to_read = this->available() > sizeof(tmp) ? sizeof(tmp) : this->available();
    this->read_array(tmp, to_read);
  }
}

ReportedEvent C4004Component::handle_packet(const Packet *packet) {
  if (packet == nullptr) {
    return EVENT_ERROR;
  }

  if (packet->control == CTRL_SYSTEM &&
      (packet->cmd == CMD_SYSTEM_HEARTBEAT_REPORT || packet->cmd == CMD_SYSTEM_HEARTBEAT_QUERY)) {
    this->last_heartbeat_ms_ = millis();
    this->heartbeat_ = true;
  } else if (packet->control == CTRL_WORK_STATUS &&
             (packet->cmd == CMD_WORK_STATUS_INIT_FINISHED_REPORT ||
              packet->cmd == CMD_WORK_STATUS_INIT_FINISHED_QUERY)) {
    if (packet->len > 0) {
      this->init_finished_ = packet->data[0] == 0x01 || packet->cmd == CMD_WORK_STATUS_INIT_FINISHED_REPORT;
    }
  } else if (packet->control == CTRL_PRESENCE && packet->cmd == CMD_PRESENCE_QUERY_ENABLE && packet->len > 0) {
    this->presence_enable_ = packet->data[0] != 0;
  } else if (packet->control == CTRL_PRESENCE &&
             (packet->cmd == CMD_PRESENCE_REPORT || packet->cmd == CMD_PRESENCE_QUERY_STATE) && packet->len > 0) {
    this->presence_state_ = static_cast<PresenceState>(packet->data[0]);
  } else if (packet->control == CTRL_PRESENCE &&
             (packet->cmd == CMD_PRESENCE_MOTION_REPORT || packet->cmd == CMD_PRESENCE_QUERY_MOTION) &&
             packet->len > 0) {
    this->motion_state_ = static_cast<MotionState>(packet->data[0]);
  } else if (packet->control == CTRL_TRAJECTORY &&
             (packet->cmd == CMD_TRAJECTORY_TARGET_REPORT || packet->cmd == CMD_TRAJECTORY_QUERY_TARGET)) {
    this->parse_targets(packet->data, packet->len);
  } else if (packet->control == CTRL_TRAJECTORY && packet->cmd == CMD_TRAJECTORY_QUERY_TRAJECTORY_LED &&
             packet->len > 0) {
    this->trajectory_led_ = packet->data[0] != 0;
  } else if (packet->control == CTRL_TRAJECTORY && packet->cmd == CMD_TRAJECTORY_QUERY_MOTION_LED &&
             packet->len > 0) {
    this->motion_led_ = packet->data[0] != 0;
  } else if (packet->control == CTRL_DETECTION_RANGE && packet->cmd == CMD_DETECTION_RANGE_QUERY_RANGE) {
    this->parse_boundary_range(packet->data, packet->len);
  } else if (packet->control == CTRL_PEOPLE_COUNT &&
             (packet->cmd == CMD_PEOPLE_COUNT_REPORT || packet->cmd == CMD_PEOPLE_COUNT_QUERY_COUNT)) {
    this->parse_people_count(packet->data, packet->len);
  }

  return this->classify_packet(packet);
}

ReportedEvent C4004Component::classify_packet(const Packet *packet) const {
  if (packet == nullptr) {
    return EVENT_ERROR;
  }
  if (packet->control == CTRL_SYSTEM &&
      (packet->cmd == CMD_SYSTEM_HEARTBEAT_REPORT || packet->cmd == CMD_SYSTEM_HEARTBEAT_QUERY)) {
    return EVENT_HEARTBEAT;
  }
  if (packet->control == CTRL_WORK_STATUS && packet->cmd == CMD_WORK_STATUS_INIT_FINISHED_REPORT) {
    return EVENT_INIT_FINISHED;
  }
  if (packet->control == CTRL_PRESENCE &&
      (packet->cmd == CMD_PRESENCE_REPORT || packet->cmd == CMD_PRESENCE_QUERY_STATE)) {
    return EVENT_PRESENCE;
  }
  if (packet->control == CTRL_PRESENCE &&
      (packet->cmd == CMD_PRESENCE_MOTION_REPORT || packet->cmd == CMD_PRESENCE_QUERY_MOTION)) {
    return EVENT_MOTION;
  }
  if (packet->control == CTRL_TRAJECTORY &&
      (packet->cmd == CMD_TRAJECTORY_TARGET_REPORT || packet->cmd == CMD_TRAJECTORY_QUERY_TARGET)) {
    return EVENT_TRAJECTORY;
  }
  if (packet->control == CTRL_DETECTION_RANGE && packet->cmd == CMD_DETECTION_RANGE_TAG_REPORT) {
    return EVENT_TAG;
  }
  if (packet->control == CTRL_PEOPLE_COUNT &&
      (packet->cmd == CMD_PEOPLE_COUNT_REPORT || packet->cmd == CMD_PEOPLE_COUNT_QUERY_COUNT)) {
    return EVENT_PEOPLE_COUNT;
  }
  return EVENT_UNKNOWN;
}

void C4004Component::parse_targets(const uint8_t *data, uint16_t len) {
  const uint8_t target_len = 11;
  if (data == nullptr) {
    this->target_count_ = 0;
    return;
  }
  uint8_t count = len / target_len;
  if (count > MAX_TARGETS) {
    count = MAX_TARGETS;
  }
  this->target_count_ = count;
}

void C4004Component::parse_boundary_range(const uint8_t *data, uint16_t len) {
  if (data == nullptr || len < 1) {
    return;
  }

  this->range_info_.mode = static_cast<DetectionRangeMode>(data[0]);
  if (this->range_info_.mode != RANGE_FOUR_SIDE_BOUNDARY) {
    return;
  }

  uint8_t offset = 1;
  if (len >= 10 && data[1] == 0x00) {
    offset = 2;
  }
  if (len >= static_cast<uint16_t>(offset + 8)) {
    this->range_info_.x_positive_cm = this->read_sign_bit_int16(&data[offset]);
    this->range_info_.x_negative_cm = this->read_sign_bit_int16(&data[offset + 2]);
    this->range_info_.y_positive_cm = this->read_sign_bit_int16(&data[offset + 4]);
    this->range_info_.y_negative_cm = this->read_sign_bit_int16(&data[offset + 6]);
  }
}

void C4004Component::parse_people_count(const uint8_t *data, uint16_t len) {
  if (data == nullptr || len == 0) {
    this->people_count_ = 0;
  } else if (len >= 2) {
    this->people_count_ = data[1];
  } else {
    this->people_count_ = data[0];
  }
}

uint16_t C4004Component::read_uint16(const uint8_t *data) const {
  return static_cast<uint16_t>(data[0]) << 8 | data[1];
}

int16_t C4004Component::read_int16(const uint8_t *data) const {
  return static_cast<int16_t>(this->read_uint16(data));
}

int16_t C4004Component::read_sign_bit_int16(const uint8_t *data) const {
  const uint16_t raw = this->read_uint16(data);
  const int16_t magnitude = static_cast<int16_t>(raw & 0x7FFF);
  return (raw & 0x8000) != 0 ? static_cast<int16_t>(-magnitude) : magnitude;
}

uint32_t C4004Component::read_uint32(const uint8_t *data) const {
  return static_cast<uint32_t>(data[0]) << 24 | static_cast<uint32_t>(data[1]) << 16 |
         static_cast<uint32_t>(data[2]) << 8 | data[3];
}

void C4004Component::write_uint16(uint8_t *data, uint16_t value) const {
  data[0] = static_cast<uint8_t>(value >> 8);
  data[1] = static_cast<uint8_t>(value & 0xFF);
}

void C4004Component::write_int16(uint8_t *data, int16_t value) const {
  this->write_uint16(data, static_cast<uint16_t>(value));
}

void C4004Component::write_sign_bit_int16(uint8_t *data, int16_t value) const {
  int32_t magnitude = value;
  uint16_t raw = 0;
  if (magnitude < 0) {
    magnitude = -magnitude;
    raw = 0x8000;
  }
  if (magnitude > 0x7FFF) {
    magnitude = 0x7FFF;
  }
  raw |= static_cast<uint16_t>(magnitude);
  this->write_uint16(data, raw);
}

void C4004Component::write_uint32(uint8_t *data, uint32_t value) const {
  data[0] = static_cast<uint8_t>(value >> 24);
  data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
  data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
  data[3] = static_cast<uint8_t>(value & 0xFF);
}

void C4004Component::sync_device_state() {
  InstallInfo install_info;
  BoundaryDetectionRange range_info;
  bool bool_value = false;
  uint32_t uint32_value = 0;

  if (this->get_install_info(&install_info)) {
    this->install_info_ = install_info;
  }
  if (this->get_presence_enable(&bool_value)) {
    this->presence_enable_ = bool_value;
  }
  if (this->get_trajectory_tracking(&bool_value)) {
    this->trajectory_tracking_ = bool_value;
  }
  if (this->get_trajectory_led(&bool_value)) {
    this->trajectory_led_ = bool_value;
  } else {
    this->trajectory_led_ = true;
  }
  if (this->get_motion_led(&bool_value)) {
    this->motion_led_ = bool_value;
  } else {
    this->motion_led_ = true;
  }
  if (this->get_boundary_detection_range(&range_info)) {
    this->range_info_ = range_info;
  }
  this->get_detection_range_mode();
  this->get_presence_state();
  this->get_motion_state();
  this->get_target_count_active();
  this->get_people_count_info(GET_DATA_ACTIVE);

  if (this->get_people_report_interval(&uint32_value)) {
    this->people_report_interval_ = uint32_value;
  }
  if (this->get_trajectory_generate_distance(&uint32_value)) {
    this->trajectory_generate_distance_ = uint32_value;
  }
  if (this->get_trajectory_hold_time(&uint32_value)) {
    this->trajectory_hold_time_ = uint32_value;
  }
  if (this->get_no_person_delay(&uint32_value)) {
    this->no_person_delay_ = uint32_value;
  }

  this->publish_all_states();
}

void C4004Component::publish_all_states() {
  this->publish_presence_state();
  this->publish_motion_state();
  this->publish_people_count();
  this->publish_target_count_number();
  this->publish_install_info();
  this->publish_boundary_range();
  this->publish_switch_states();
  this->publish_people_setting_numbers();
  this->publish_detection_range_mode();
}

void C4004Component::publish_online(bool online) {
#ifdef USE_BINARY_SENSOR
  if (this->online_binary_sensor_ != nullptr) {
    this->online_binary_sensor_->publish_state(online);
  }
#else
  (void) online;
#endif
}

void C4004Component::publish_presence_state() {
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(this->presence_state_ == PRESENCE);
  }
#endif
}

void C4004Component::publish_motion_state() {
#ifdef USE_SENSOR
  if (this->motion_state_sensor_ != nullptr) {
    this->motion_state_sensor_->publish_state(static_cast<float>(this->motion_state_));
  }
#endif
}

void C4004Component::publish_people_count() {
#ifdef USE_SENSOR
  if (this->people_count_sensor_ != nullptr) {
    this->people_count_sensor_->publish_state(this->people_count_);
  }
#endif
}

void C4004Component::publish_install_info() {
#ifdef USE_SELECT
  if (this->install_mode_select_ != nullptr) {
    this->install_mode_select_->publish_state(this->install_mode_to_string(this->install_info_.mode));
  }
#endif
#ifdef USE_NUMBER
  if (this->install_height_number_ != nullptr) {
    this->install_height_number_->publish_state(this->install_info_.height_cm);
  }
  if (this->install_z_angle_number_ != nullptr) {
    this->install_z_angle_number_->publish_state(this->install_info_.z_angle);
  }
#endif
}

void C4004Component::publish_boundary_range() {
#ifdef USE_NUMBER
  if (this->range_x_max_number_ != nullptr) {
    this->range_x_max_number_->publish_state(this->range_info_.x_positive_cm);
  }
  if (this->range_x_min_number_ != nullptr) {
    this->range_x_min_number_->publish_state(this->range_info_.x_negative_cm);
  }
  if (this->range_y_max_number_ != nullptr) {
    this->range_y_max_number_->publish_state(this->range_info_.y_positive_cm);
  }
  if (this->range_y_min_number_ != nullptr) {
    this->range_y_min_number_->publish_state(this->range_info_.y_negative_cm);
  }
#endif
}

void C4004Component::publish_switch_states() {
#ifdef USE_SWITCH
  if (this->presence_enable_switch_ != nullptr) {
    this->presence_enable_switch_->publish_state(this->presence_enable_);
  }
  if (this->trajectory_tracking_switch_ != nullptr) {
    this->trajectory_tracking_switch_->publish_state(this->trajectory_tracking_);
  }
  if (this->trajectory_led_switch_ != nullptr) {
    this->trajectory_led_switch_->publish_state(this->trajectory_led_);
  }
  if (this->motion_led_switch_ != nullptr) {
    this->motion_led_switch_->publish_state(this->motion_led_);
  }
#endif
}

void C4004Component::publish_people_setting_numbers() {
#ifdef USE_NUMBER
  if (this->people_report_interval_number_ != nullptr) {
    this->people_report_interval_number_->publish_state(this->people_report_interval_);
  }
  if (this->trajectory_generate_distance_number_ != nullptr) {
    this->trajectory_generate_distance_number_->publish_state(this->trajectory_generate_distance_);
  }
  if (this->trajectory_hold_time_number_ != nullptr) {
    this->trajectory_hold_time_number_->publish_state(this->trajectory_hold_time_);
  }
  if (this->no_person_delay_number_ != nullptr) {
    this->no_person_delay_number_->publish_state(this->no_person_delay_);
  }
#endif
}

void C4004Component::publish_target_count_number() {
#ifdef USE_NUMBER
  if (this->target_count_number_ != nullptr) {
    this->target_count_number_->publish_state(this->target_count_);
  }
#endif
}

void C4004Component::publish_detection_range_mode() {
#ifdef USE_TEXT_SENSOR
  if (this->detection_range_mode_text_sensor_ != nullptr) {
    this->detection_range_mode_text_sensor_->publish_state(this->range_mode_to_string(this->range_info_.mode));
  }
#endif
}

void C4004Component::publish_status(const char *message) {
#ifdef USE_TEXT_SENSOR
  if (this->status_text_sensor_ != nullptr) {
    this->status_text_sensor_->publish_state(message);
  }
#else
  (void) message;
#endif
}

const char *C4004Component::install_mode_to_string(InstallMode mode) const {
  if (mode == INSTALL_MODE_TOP) {
    return "Top";
  }
  return "Side";
}

const char *C4004Component::range_mode_to_string(DetectionRangeMode mode) const {
  switch (mode) {
    case RANGE_SIDE_DEFAULT:
      return "Side Default";
    case RANGE_SIDE_LEFT_EDGE:
      return "Side Left Edge";
    case RANGE_SIDE_RIGHT_EDGE:
      return "Side Right Edge";
    case RANGE_HOTEL_CORRIDOR:
      return "Hotel Corridor";
    case RANGE_FOUR_SIDE_BOUNDARY:
      return "Four-side Boundary";
    case RANGE_TRAJECTORY:
      return "Trajectory";
    case RANGE_CONFIG_FILE:
      return "Config File";
    case RANGE_NO_BOUNDARY:
      return "No Boundary";
    case RANGE_TOP_DEFAULT:
      return "Top Default";
    case RANGE_TOP_LEFT_EDGE:
      return "Top Left Edge";
    case RANGE_TOP_RIGHT_EDGE:
      return "Top Right Edge";
    default:
      return "Unknown";
  }
}

}  // namespace dfrobot_c4004
}  // namespace esphome
