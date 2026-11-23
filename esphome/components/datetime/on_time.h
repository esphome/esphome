#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TIME

#include "esphome/core/base_automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "datetime_entity.h"
#include "time_entity.h"

// Kept out of the entity headers, which application.h includes before base_automation.h can be.
namespace esphome::datetime {

#ifdef USE_DATETIME_TIME
/// Runs fire() when the clock reaches the entity's time, for on_time.
class OnTimeBase : public FiringComponent<>, public Parented<TimeEntity> {
 public:
  explicit OnTimeBase(TimeEntity *parent) : Parented(parent) {}
  void loop() override;

 protected:
  bool matches_(const ESPTime &time) const;

  optional<ESPTime> last_check_;
};
#endif

#ifdef USE_DATETIME_DATETIME
/// Runs fire() when the clock reaches the entity's date and time, for on_time.
class OnDateTimeBase : public FiringComponent<>, public Parented<DateTimeEntity> {
 public:
  explicit OnDateTimeBase(DateTimeEntity *parent) : Parented(parent) {}
  void loop() override;

 protected:
  bool matches_(const ESPTime &time) const;

  optional<ESPTime> last_check_;
};
#endif

}  // namespace esphome::datetime

#endif  // USE_TIME
