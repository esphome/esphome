#include "mitp_select.h"

namespace esphome {
namespace mitsubishi_itp {

void VanePositionSelect::process_packet(const SettingsGetResponsePacket &packet) {
  switch (packet.get_vane()) {
    case SettingsSetRequestPacket::VANE_AUTO:
      mitp_select_value_ = std::string("Auto");
      break;
    case SettingsSetRequestPacket::VANE_1:
      mitp_select_value_ = std::string("1");
      break;
    case SettingsSetRequestPacket::VANE_2:
      mitp_select_value_ = std::string("2");
      break;
    case SettingsSetRequestPacket::VANE_3:
      mitp_select_value_ = std::string("3");
      break;
    case SettingsSetRequestPacket::VANE_4:
      mitp_select_value_ = std::string("4");
      break;
    case SettingsSetRequestPacket::VANE_5:
      mitp_select_value_ = std::string("5");
      break;
    case SettingsSetRequestPacket::VANE_SWING:
      mitp_select_value_ = std::string("Swing");
      break;
    default:
      ESP_LOGW(TAG, "Vane in unknown position %x", packet.get_vane());
  }
}

void HorizontalVanePositionSelect::process_packet(const SettingsGetResponsePacket &packet) {
  switch (packet.get_horizontal_vane()) {
    case SettingsSetRequestPacket::HV_AUTO:
      mitp_select_value_ = std::string("Auto");
      break;
    case SettingsSetRequestPacket::HV_LEFT_FULL:
      mitp_select_value_ = std::string("<<");
      break;
    case SettingsSetRequestPacket::HV_LEFT:
      mitp_select_value_ = std::string("<");
      break;
    case SettingsSetRequestPacket::HV_CENTER:
      mitp_select_value_ = std::string("");
      break;
    case SettingsSetRequestPacket::HV_RIGHT:
      mitp_select_value_ = std::string(">");
      break;
    case SettingsSetRequestPacket::HV_RIGHT_FULL:
      mitp_select_value_ = std::string(">>");
      break;
    case SettingsSetRequestPacket::HV_SPLIT:
      mitp_select_value_ = std::string("<>");
      break;
    case SettingsSetRequestPacket::HV_SWING:
      mitp_select_value_ = std::string("Swing");
      break;
    default:
      ESP_LOGW(TAG, "Vane in unknown horizontal position %x", packet.get_horizontal_vane());
  }
}
}  // namespace mitsubishi_itp
}  // namespace esphome
