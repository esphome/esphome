#include "storage.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace storage {

/***********************************************************************************
 *
 *   Action and condition processint
 */
template<typename... Ts> class StorageIsPresent : public Condition<Ts...> {
 public:
  StorageIsPresent(Storage *parent) : parent_(parent) {}
  bool check(Ts... x) override { return this->parent_->state_media(); }

 protected:
  Storage *parent_;
};

}  // namespace storage
}  // namespace esphome
