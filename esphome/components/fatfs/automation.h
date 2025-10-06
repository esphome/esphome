#include "fatfs.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esp_log.h"

namespace esphome {
namespace fatfs {

const char *TAG = "fatfs_automation";

template<typename... Ts> class FatIsExistCondition : public Condition<Ts...> {
 public:
  FatIsExistCondition(FatFs *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  bool check(Ts... x) override {
    auto path = this->path_.value(x...);
    if (this->parent_->is_mount()) {
      return false;
    } else {
      return this->parent_->is_exist(path);
    }
  }

 protected:
  FatFs *parent_;
};

}  // namespace fatfs
}  // namespace esphome
