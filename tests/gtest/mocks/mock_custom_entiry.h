#pragma once
#include "gmock/gmock.h"
#include "esphome/core/entity_base.h"

namespace esphome {
namespace test {

struct CustomEntity : public EntityBase {
  static constexpr EntityType ENTITY_TYPE{static_cast<EntityType>(EntityType::MAX + 1)};
  virtual void add_on_state_callback(std::function<void(bool)> &&callback) = 0;
};

struct MockCustomEntity : public CustomEntity {
  MOCK_METHOD(void, add_on_state_callback, (std::function<void(bool)> && callback), (override));
};

}  // namespace test
}  // namespace esphome
