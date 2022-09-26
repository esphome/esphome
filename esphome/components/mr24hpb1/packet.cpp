#include "mr24hpb1.h"

namespace esphome {
namespace mr24hpb1 {

Packet::Packet(uint16_t size) {
  this->packet_data_.reserve(size);
}

void Packet::clear() {
  this->packet_data_.clear();
}

void Packet::append_data(std::vector<uint8_t> &data) {
  this->packet_data_.insert(std::end(this->packet_data_), std::begin(data), std::end(data));
}

void Packet::push_data(uint8_t data) {
  this->packet_data_.push_back(data);
}

uint8_t* Packet::raw_data() {
  return this->packet_data_.data();
}

uint16_t Packet::raw_size() {
  return this->packet_data_.size();
}

std::vector<uint8_t>& Packet::raw_vect() {
  return this->packet_data_;
}

FunctionCode Packet::function_code() { return this->packet_data_.at(3); }

AddressCode1 Packet::address_code_1() { return this->packet_data_.at(4); }

AddressCode2 Packet::address_code_2() { return this->packet_data_.at(5); }

uint16_t Packet::length() {
  uint16_t length_low = this->packet_data_.at(1);
  uint16_t length_high = this->packet_data_.at(2);

  return (length_high << 8 | length_low) + 1;  // include PACKET_START in length
}

uint16_t Packet::crc() {
  uint16_t packet_size = this->packet_data_.size();
  uint16_t crc16_high = this->packet_data_.at(packet_size - 1);
  uint16_t crc16_low = this->packet_data_.at(packet_size - 2);
  return crc16_low << 8 | crc16_high;
}

uint32_t Packet::data_as_int() {
  uint32_t converted_data = 0x00;
  std::vector<uint8_t> data = this->data();
  for (uint8_t &byte : data) {
    converted_data <<= 8;
    converted_data |= byte;
  }
  return converted_data;
}

std::vector<uint8_t> Packet::data() {
  return std::vector<uint8_t>(this->packet_data_.begin() + 6, this->packet_data_.end() - 2);
}

std::string Packet::data_as_string() {
  std::string str = "";
  std::vector<uint8_t> data = this->data();
  for (uint8_t &byte : data) {
    str += byte;
  }
  return str;
}

using Float_Byte = union {
  uint32_t Byte;
  float Float;
};

float Packet::data_as_float() {
  Float_Byte converted_data;
  converted_data.Byte = 0x00;
  std::vector<uint8_t> data = this->data();
  for (auto i = data.rbegin(); i != data.rend(); ++i) {
    converted_data.Byte <<= 8;
    converted_data.Byte |= *i;
  }
  return converted_data.Float;
}

void Packet::log() {
  std::string output = "Packet:";
  char buf[6];
  for (uint8_t &byte : this->packet_data_) {
    sprintf(buf, " 0x%02x", byte);
    output += buf;
  }

  ESP_LOGD(TAG, "%s", output.c_str());
}
}
}
