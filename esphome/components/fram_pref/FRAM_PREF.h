#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/fram/FRAM.h"
#include <map>

namespace esphome {
namespace fram_pref {

enum Flags : uint8_t {
  FLAG_STATIC        = 0b00000001,
  FLAG_PERSIST_KEY   = 0b00000010,
  FLAG_ERR           = 0b10000000,
  FLAG_ERR_SIZE_REQ  = 0b00010000,
  FLAG_ERR_SIZE_FRAM = 0b00100000,
  FLAG_ERR_SIZE_POOL = 0b01000000
};

struct PREF_STRUCT {
  std::string key;
  uint16_t addr;
  uint16_t size;
  uint16_t size_req;
  uint8_t flags;
};

class FRAM_PREF : public Component, public ESPPreferences {
  public:
    FRAM_PREF(fram::FRAM * fram);
    
    void set_pool(uint16_t pool_size, uint16_t pool_start);
    void set_static_pref(std::string key, uint16_t addr, uint16_t size, std::function<uint32_t()> && fn, bool persist_key);
    
    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override { return setup_priority::BUS; }
    
    ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override;
    ESPPreferenceObject make_preference(size_t length, uint32_t type) override;
    bool sync() override;
    bool reset() override;
  
  protected:
    friend class FRAMPreferenceBackend;
    
    bool _check();
    void _clear();
    
    fram::FRAM * fram_;
    uint16_t pool_size_{0};
    uint16_t pool_start_{0};
    uint16_t pool_next_{0};
    bool pool_cleared_{false};
    
    std::vector<PREF_STRUCT> prefs_;
    std::vector<std::function<uint32_t()>> prefs_static_cb_;
    std::map<uint32_t,uint8_t> prefs_static_map_;
    
    ESPPreferences * pref_prev_;
};

}  // namespace fram_pref
}  // namespace esphome
