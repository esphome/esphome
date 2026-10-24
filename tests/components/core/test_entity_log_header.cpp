#include <gtest/gtest.h>

#include "esphome/core/entity_base.h"

namespace esphome::core::testing {

class LogHeaderEntity : public EntityBase {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0, 0); }
};

// Callers skip the rest of an entity's dump on false, so a null entity must return false and any
// other entity true, whatever the log level
TEST(EntityLogHeader, NullEntityReturnsFalse) { EXPECT_FALSE(log_entity_header("test", "", "Sensor", nullptr)); }

TEST(EntityLogHeader, EntityReturnsTrue) {
  LogHeaderEntity entity;
  entity.configure("Living Room Temp");
  EXPECT_TRUE(log_entity_header("test", "  ", "Sensor", &entity));
}

}  // namespace esphome::core::testing
