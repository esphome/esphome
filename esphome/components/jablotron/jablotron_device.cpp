#include "jablotron_device.h"
#include "jablotron_component.h"

namespace esphome {
namespace jablotron {

static const char TAG[] = "jablotron";

const std::string &JablotronDevice::get_access_code() const { return this->access_code_; }
void JablotronDevice::set_access_code(std::string access_code) { this->access_code_ = std::move(access_code); }

void JablotronDevice::set_parent_jablotron(JablotronComponent *parent) {
  if (this->parent_ != parent) {
    this->parent_ = parent;
    if (parent != nullptr) {
      this->register_parent(*parent);
    }
  }
}

JablotronComponent *JablotronDevice::get_parent_jablotron() const { return this->parent_; }

const std::string &IndexedDevice::get_index_string() const {
  if (this->index_str_.empty()) {
    ESP_LOGE(TAG, "Index not set");
  }
  return this->index_str_;
}

void IndexedDevice::set_index(int value) {
  this->index_ = value;
  this->index_str_ = std::to_string(value);
}

SectionFlag SectionFlagDevice::get_flag() const { return this->flag_; }

void SectionFlagDevice::set_flag(SectionFlag value) { this->flag_ = value; }

void SectionFlagDevice::set_flag(int value) { this->set_flag(static_cast<SectionFlag>(value)); }

}  // namespace jablotron
}  // namespace esphome
