#include <gtest/gtest.h>
#include <type_traits>
#include "esphome/components/sx126x/radio_operation.h"

namespace esphome::sx126x::testing {

static_assert(!std::is_copy_constructible_v<RadioOperation>);
static_assert(!std::is_move_constructible_v<RadioOperation>);

template<typename T>
concept TransmitWithoutRestoration =
    requires(T &operation) { operation.transmit([]() { return true; }, []() { return SX126xError::NONE; }); };
static_assert(!TransmitWithoutRestoration<RadioOperation>);

TEST(RadioOperation, RejectedAdmissionDoesNotRunCallbacks) {
  RadioOperation operation;
  bool called = false;
  EXPECT_EQ(operation.transmit(
                [&]() {
                  called = true;
                  return true;
                },
                [&]() {
                  called = true;
                  return SX126xError::NONE;
                },
                [&]() {
                  called = true;
                  return true;
                }),
            SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_FALSE(called);
  EXPECT_FALSE(operation.is_ready());
}

TEST(RadioOperation, ConfigurationOwnsReadinessAndRejectsReentry) {
  RadioOperation operation;
  bool nested = false;
  EXPECT_FALSE(operation.configure([&]() {
    EXPECT_FALSE(operation.is_ready());
    EXPECT_EQ(operation.transmit_admission(), SX126xError::SX126X_ERROR_BUSY);
    EXPECT_FALSE(operation.configure([&]() {
      nested = true;
      return true;
    }));
    EXPECT_FALSE(operation.change_mode([&]() {
      nested = true;
      return true;
    }));
    return false;
  }));
  EXPECT_FALSE(nested);
  EXPECT_EQ(operation.transmit_admission(), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(operation.configure([]() { return true; }));
  EXPECT_TRUE(operation.is_ready());
}

TEST(RadioOperation, EarlyPreparationFailureStillRestoresExactlyOnce) {
  RadioOperation operation;
  ASSERT_TRUE(operation.configure([]() { return true; }));
  unsigned restored = 0;
  bool waited = false;
  EXPECT_EQ(operation.transmit([]() { return false; },
                               [&]() {
                                 waited = true;
                                 return SX126xError::NONE;
                               },
                               [&]() {
                                 restored++;
                                 return true;
                               }),
            SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  EXPECT_FALSE(waited);
  EXPECT_EQ(restored, 1u);
  EXPECT_TRUE(operation.is_ready());
}

TEST(RadioOperation, EveryWaitResultRestoresAndCleanupFailureBlocksReuse) {
  for (auto result : {SX126xError::NONE, SX126xError::TIMEOUT, SX126xError::SX126X_ERROR_BUSY_TIMEOUT}) {
    for (bool restored : {false, true}) {
      RadioOperation operation;
      ASSERT_TRUE(operation.configure([]() { return true; }));
      unsigned cleanups = 0;
      const auto actual = operation.transmit([]() { return true; }, [&]() { return result; },
                                             [&]() {
                                               cleanups++;
                                               EXPECT_FALSE(operation.is_ready());
                                               EXPECT_TRUE(operation.is_transmitting());
                                               return restored;
                                             });
      EXPECT_EQ(actual, restored ? result : SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
      EXPECT_EQ(cleanups, 1u);
      EXPECT_EQ(operation.is_ready(), restored);
      if (!restored) {
        EXPECT_EQ(operation.transmit_admission(), SX126xError::SX126X_ERROR_NOT_READY);
        bool called = false;
        EXPECT_FALSE(operation.change_mode([&]() {
          called = true;
          return true;
        }));
        EXPECT_FALSE(called);
        EXPECT_TRUE(operation.configure([]() { return true; }));
        EXPECT_TRUE(operation.is_ready());
      }
    }
  }
}

TEST(RadioOperation, CannotReenterDuringPreparationWaitingOrRestoration) {
  RadioOperation operation;
  ASSERT_TRUE(operation.configure([]() { return true; }));
  unsigned phases = 0;
  auto check = [&]() {
    phases++;
    bool nested = false;
    EXPECT_FALSE(operation.is_ready());
    EXPECT_TRUE(operation.is_transmitting());
    EXPECT_FALSE(operation.configure([&]() {
      nested = true;
      return true;
    }));
    EXPECT_FALSE(operation.change_mode([&]() {
      nested = true;
      return true;
    }));
    EXPECT_EQ(operation.transmit(
                  [&]() {
                    nested = true;
                    return true;
                  },
                  []() { return SX126xError::NONE; }, []() { return true; }),
              SX126xError::SX126X_ERROR_BUSY);
    EXPECT_FALSE(nested);
  };
  EXPECT_EQ(operation.transmit(
                [&]() {
                  check();
                  return true;
                },
                [&]() {
                  check();
                  return SX126xError::NONE;
                },
                [&]() {
                  check();
                  return true;
                }),
            SX126xError::NONE);
  EXPECT_EQ(phases, 3u);
  EXPECT_TRUE(operation.is_ready());
}

TEST(RadioOperation, FailedModeChangeAndColdSleepRequireConfiguration) {
  RadioOperation operation;
  ASSERT_TRUE(operation.configure([]() { return true; }));
  EXPECT_FALSE(operation.change_mode([]() { return false; }));
  EXPECT_EQ(operation.transmit_admission(), SX126xError::SX126X_ERROR_NOT_READY);
  ASSERT_TRUE(operation.configure([]() { return true; }));
  EXPECT_TRUE(operation.change_mode([]() { return true; }, false));
  EXPECT_FALSE(operation.is_ready());
  ASSERT_TRUE(operation.configure([]() { return true; }));
  EXPECT_TRUE(operation.change_mode([]() { return true; }));
  EXPECT_TRUE(operation.is_ready());
}

}  // namespace esphome::sx126x::testing
