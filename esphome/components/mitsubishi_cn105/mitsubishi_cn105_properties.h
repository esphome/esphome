#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

template<auto Unknown, size_t N> struct LookupMap {
  using value_type = decltype(Unknown);
  const std::array<value_type, N> table;

  constexpr value_type lookup(uint8_t raw) const { return (raw < N) ? this->table[raw] : Unknown; }

  constexpr bool reverse_lookup(value_type value, uint8_t &out) const {
    static_assert(N <= std::numeric_limits<uint8_t>::max());
    if (value == Unknown) {
      return false;
    }
    for (uint8_t i = 0; i < static_cast<uint8_t>(N); ++i) {
      if (this->table[i] == value) {
        out = i;
        return true;
      }
    }
    return false;
  }
};

template<auto Unknown, class T, std::size_t N> static constexpr auto make_map(const T (&values)[N]) {
  return LookupMap<Unknown, N>{std::to_array(values)};
}

struct Property {
  using PropertyId = MitsubishiCN105::PropertyId;
  using Status = MitsubishiCN105::Status;
  using PropertyContext = MitsubishiCN105::PropertyContext;

  struct Power {
    static constexpr auto ID = PropertyId::POWER;

    static void decode_context(PropertyContext &ctx, const uint8_t *payload) {}

    static void decode(Status &status, const uint8_t *payload, const PropertyContext &ctx) {
      status.power_on = payload[2] != 0;
    }

    static void encode(uint8_t *payload, const Status &status, const PropertyContext &ctx) {
      payload[1] |= 0x01;
      payload[3] = status.power_on ? 0x01 : 0x00;
    }
  };

  struct Temperature {
    struct Target {
      static constexpr auto ID = PropertyId::TEMPERATURE;
      static constexpr uint8_t TARGET_TEMPERATURE_ENC_A_OFFSET = 31;

      static void decode_context(PropertyContext &ctx, const uint8_t *payload) {
        ctx.use_temperature_encoding_b = payload[10] != 0;
      }

      static void decode(Status &status, const uint8_t *payload, const PropertyContext &ctx) {
        status.target_temperature = Temperature::decode(-payload[4], payload[10], TARGET_TEMPERATURE_ENC_A_OFFSET);
      }

      static void encode(uint8_t *payload, const Status &status, const PropertyContext &ctx) {
        payload[1] |= 0x04;
        if (ctx.use_temperature_encoding_b) {
          payload[14] = static_cast<uint8_t>(std::round(status.target_temperature * 2.0f) + 128);
        } else {
          payload[5] = static_cast<uint8_t>(TARGET_TEMPERATURE_ENC_A_OFFSET - std::round(status.target_temperature));
        }
      }
    };

    struct Room {
      static void decode_context(PropertyContext &ctx, const uint8_t *payload) {}

      static void decode(Status &status, const uint8_t *payload, const PropertyContext &ctx) {
        status.room_temperature = Temperature::decode(payload[2], payload[5], 10);
      }
    };

    struct Remote {
      static constexpr auto ID = PropertyId::REMOTE_TEMPERATURE;

      static void encode(uint8_t *payload, uint8_t remote_temperature_half_deg, const PropertyContext &) {
        if (remote_temperature_half_deg == MitsubishiCN105::REMOTE_TEMPERATURE_DISABLED) {
          payload[3] = 0x80;
        } else {
          payload[1] = 0x01;
          payload[2] = static_cast<uint8_t>(remote_temperature_half_deg - 16);
          payload[3] = static_cast<uint8_t>(remote_temperature_half_deg + 128);
        }
      }
    };

   protected:
    static constexpr float decode(int temp_a, int temp_b, int delta) {
      return temp_b != 0 ? (temp_b - 128) / 2.0f : delta + temp_a;
    }
  };

  template<typename Derived, auto Field> struct Lookup {
    using Value = std::remove_cvref_t<decltype(std::declval<Status>().*Field)>;

    static void decode_context(PropertyContext &ctx, const uint8_t *payload) {}

    static void decode(Status &status, const uint8_t *payload, const PropertyContext &ctx) {
      status.*Field = Derived::MAP.lookup(Derived::decode_raw(payload, ctx));
    }

    static void encode(uint8_t *payload, const Status &status, const PropertyContext &ctx) {
      uint8_t raw;
      if (Derived::MAP.reverse_lookup(status.*Field, raw)) {
        Derived::encode_raw(payload, raw, ctx);
      }
    }

    template<typename Mask> static bool validate_and_set(Value value, Status &status, Mask &mask) {
      uint8_t raw;
      if (!Derived::MAP.reverse_lookup(value, raw)) {
        return false;
      }
      status.*Field = value;
      mask.set(Derived::ID);
      return true;
    }

   private:
    friend Derived;
    constexpr Lookup() = default;
  };

  struct Mode : Lookup<Mode, &Status::mode> {
    static constexpr auto ID = PropertyId::MODE;
    static constexpr auto MAP = make_map<MitsubishiCN105::Mode::UNKNOWN>({
        MitsubishiCN105::Mode::UNKNOWN,   // 0x00
        MitsubishiCN105::Mode::HEAT,      // 0x01
        MitsubishiCN105::Mode::DRY,       // 0x02
        MitsubishiCN105::Mode::COOL,      // 0x03
        MitsubishiCN105::Mode::UNKNOWN,   // 0x04
        MitsubishiCN105::Mode::UNKNOWN,   // 0x05
        MitsubishiCN105::Mode::UNKNOWN,   // 0x06
        MitsubishiCN105::Mode::FAN_ONLY,  // 0x07
        MitsubishiCN105::Mode::AUTO       // 0x08
    });

    static uint8_t decode_raw(const uint8_t *payload, const PropertyContext &ctx) {
      const bool i_see = payload[3] > 0x08;
      return payload[3] - (i_see ? 0x08 : 0);
    }

    static void encode_raw(uint8_t *payload, uint8_t raw, const PropertyContext &) {
      payload[1] |= 0x02;
      payload[4] = raw;
    }
  };

  struct FanMode : Lookup<FanMode, &Status::fan_mode> {
    static constexpr auto ID = PropertyId::FAN;
    static constexpr auto MAP = make_map<MitsubishiCN105::FanMode::UNKNOWN>({
        MitsubishiCN105::FanMode::AUTO,     // 0x00
        MitsubishiCN105::FanMode::QUIET,    // 0x01
        MitsubishiCN105::FanMode::SPEED_1,  // 0x02
        MitsubishiCN105::FanMode::SPEED_2,  // 0x03
        MitsubishiCN105::FanMode::UNKNOWN,  // 0x04
        MitsubishiCN105::FanMode::SPEED_3,  // 0x05
        MitsubishiCN105::FanMode::SPEED_4   // 0x06
    });

    static uint8_t decode_raw(const uint8_t *payload, const PropertyContext &ctx) { return payload[5]; }

    static void encode_raw(uint8_t *payload, uint8_t raw, const PropertyContext &) {
      payload[1] |= 0x08;
      payload[6] = raw;
    }
  };

  struct VaneMode : Lookup<VaneMode, &Status::vane_mode> {
    static constexpr auto ID = PropertyId::VANE;
    static constexpr auto MAP = make_map<MitsubishiCN105::VaneMode::UNKNOWN>({
        MitsubishiCN105::VaneMode::AUTO,        // 0x00
        MitsubishiCN105::VaneMode::POSITION_1,  // 0x01
        MitsubishiCN105::VaneMode::POSITION_2,  // 0x02
        MitsubishiCN105::VaneMode::POSITION_3,  // 0x03
        MitsubishiCN105::VaneMode::POSITION_4,  // 0x04
        MitsubishiCN105::VaneMode::POSITION_5,  // 0x05
        MitsubishiCN105::VaneMode::UNKNOWN,     // 0x06
        MitsubishiCN105::VaneMode::SWING        // 0x07
    });

    static uint8_t decode_raw(const uint8_t *payload, const PropertyContext &ctx) { return payload[6]; }

    static void encode_raw(uint8_t *payload, uint8_t raw, const PropertyContext &) {
      payload[1] |= 0x10;
      payload[7] = raw;
    }
  };

  struct WideVaneMode : Lookup<WideVaneMode, &Status::wide_vane_mode> {
    static constexpr auto ID = PropertyId::WIDE_VANE;
    static constexpr auto MAP = make_map<MitsubishiCN105::WideVaneMode::UNKNOWN>({
        MitsubishiCN105::WideVaneMode::UNKNOWN,     // 0x00
        MitsubishiCN105::WideVaneMode::FAR_LEFT,    // 0x01
        MitsubishiCN105::WideVaneMode::LEFT,        // 0x02
        MitsubishiCN105::WideVaneMode::CENTER,      // 0x03
        MitsubishiCN105::WideVaneMode::RIGHT,       // 0x04
        MitsubishiCN105::WideVaneMode::FAR_RIGHT,   // 0x05
        MitsubishiCN105::WideVaneMode::UNKNOWN,     // 0x06
        MitsubishiCN105::WideVaneMode::UNKNOWN,     // 0x07
        MitsubishiCN105::WideVaneMode::LEFT_RIGHT,  // 0x08
        MitsubishiCN105::WideVaneMode::UNKNOWN,     // 0x09
        MitsubishiCN105::WideVaneMode::UNKNOWN,     // 0x0A
        MitsubishiCN105::WideVaneMode::UNKNOWN,     // 0x0B
        MitsubishiCN105::WideVaneMode::SWING        // 0x0C
    });

    static void decode_context(PropertyContext &ctx, const uint8_t *payload) {
      ctx.set_wide_vane_high_bit = (payload[9] & 0xF0) == 0x80;
    }

    static uint8_t decode_raw(const uint8_t *payload, const PropertyContext &ctx) { return payload[9] & 0x0F; }

    static void encode_raw(uint8_t *payload, uint8_t raw, const PropertyContext &ctx) {
      payload[2] |= 0x01;
      payload[13] = ctx.set_wide_vane_high_bit ? raw | 0x80 : raw;
    }
  };

  template<typename Mask> struct Decoder {
    const std::span<const uint8_t> payload;
    PropertyContext &context;
    const Mask &pending_writes;

    bool ESPHOME_ALWAYS_INLINE decode_settings(Status &status) {
      if (this->payload.size() <= 10) {
        return false;
      }
      this->decode_<Power, Temperature::Target, Mode, FanMode, VaneMode, WideVaneMode>(status);
      return true;
    }

    bool ESPHOME_ALWAYS_INLINE decode_room_temperature(Status &status) {
      if (this->payload.size() <= 5) {
        return false;
      }
      this->decode_<Temperature::Room>(status);
      return true;
    }

   protected:
    template<typename T, typename Out> ESPHOME_ALWAYS_INLINE void decode_one_(Out &out) {
      T::decode_context(this->context, this->payload.data());
      if constexpr (requires { T::ID; }) {
        if (this->pending_writes.contains(T::ID)) {
          return;
        }
      }
      T::decode(out, this->payload.data(), this->context);
    }

    template<typename... T, typename Out> void ESPHOME_ALWAYS_INLINE decode_(Out &out) {
      (this->decode_one_<T>(out), ...);
    }
  };

  template<typename Mask> struct Encoder {
    uint8_t *payload;
    const PropertyContext &context;
    Mask &pending_writes;

    void ESPHOME_ALWAYS_INLINE encode_settings(const Status &status) {
      this->payload[0] = 0x01;
      this->encode_and_clear_<Power, Temperature::Target, WideVaneMode, VaneMode, Mode, FanMode>(status);
    }

    void ESPHOME_ALWAYS_INLINE encode_remote_temperature(uint8_t remote_temperature_half_deg) {
      this->payload[0] = 0x07;
      this->encode_and_clear_<Temperature::Remote>(remote_temperature_half_deg);
    }

   protected:
    template<typename... T, typename In> void ESPHOME_ALWAYS_INLINE encode_and_clear_(const In &in) {
      (this->encode_one_<T>(in), ...);
      (this->pending_writes.clear(T::ID), ...);
    }

    template<typename T, typename In> void encode_one_(const In &in) {
      if (this->pending_writes.contains(T::ID)) {
        T::encode(this->payload, in, this->context);
      }
    }
  };
};

}  // namespace esphome::mitsubishi_cn105
