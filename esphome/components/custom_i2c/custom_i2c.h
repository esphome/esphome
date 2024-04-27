#pragma once

#include <bitset>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

// TODO: move all the output related stuff into its own header that's only included when outputs are used
#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

namespace esphome::custom_i2c {

static const char *const TAG = "custom_i2c";
static const char *const TRANSACTION_FAILURE_TAG = "custom_i2c.transaction_failure";
static const char *const FLOAT_OUTPUT_TAG = "custom_i2c.float_output";

// I'd like to do something more efficient here, like ideally byte sequences would go into flash and not have to stay
// resident in memory when they're not actively in use. This'll do for now though.
using ByteSequence = std::vector<uint8_t>;
using RegisterAddress = ByteSequence;

// Helper that's kind of like esphome::CallbackManager but without the indirection of callbacks.
// Useful for when you want to fire off a bunch of triggers in response to an event without needing to do any
// intervening processing that would make esphome::CallbackManager useful.
// (TODO: consider upstreaming into esphome/core/helpers.h)
template<typename... Ts> class TriggerManager {
 public:
  void add(Trigger<Ts...> *trigger) { this->triggers_.push_back(trigger); }
  void reserve(size_t count) { this->triggers_.reserve(count); }
  void trigger(Ts... args) {
    for (Trigger<Ts...> *trigger : this->triggers_) {
      trigger->trigger(args...);
    }
  }

 protected:
  std::vector<Trigger<Ts...> *> triggers_;
};

class CustomI2COnSetupComponent : public Component {
 public:
  // TODO: Right now, this winds up running after all the pin banks and pins are set up, which isn't helpful if e.g. the
  // user is dealing with a device like an MCP23017 that allows changing the register address layout and they want to
  // change the address layout before anything else writes to the device. Need to find a way to force setup actions to
  // run before anything else uses the custom I2C device.
  // (Amusingly, the same thing happens with the pin banks vs. the pins: the pins are set up just before the pin bank is
  // set up, so allowing setup actions to be attached to pin banks won't help either.)
  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot the above setup order annoyance
    ESP_LOGV(TAG, "running custom_i2c specific setup actions");  // NOLINT
    this->on_setup_triggers.trigger();
  };

  TriggerManager<> on_setup_triggers;
};

// Convert a set of bytes read from I2C to a scalar value of the specified type.
template<typename T> T from_i2c(std::array<uint8_t, sizeof(T)> data) {
  return convert_big_endian(*reinterpret_cast<T *>(data.data()));
}

// Convert a scalar value to a set of bytes ready for writing to an I2C device or register.
template<typename T> std::array<uint8_t, sizeof(T)> to_i2c(T value) {
  value = convert_big_endian(value);

  std::array<uint8_t, sizeof(T)> data;
  memcpy(data.data(), reinterpret_cast<uint8_t *>(&value), data.size());

  return data;
}

class CustomI2CDevice : public Component {
 public:
  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot a setup order annoyance
    ESP_LOGV(TAG, "Setting up custom_i2c device");  // NOLINT
  }
  void loop() override {}
  void dump_config() override {
    ESP_LOGCONFIG(TAG, "CustomI2CDevice: present! (TODO: actually log the config)");
  };  // NOLINT
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_i2c_bus(i2c::I2CBus *bus) {
    this->bus_ = bus;
    this->i2c_device.set_i2c_bus(bus);
  }
  void set_i2c_address(uint8_t device_address) {
    this->device_address_ = device_address;
    this->i2c_device.set_i2c_address(device_address);
  }
  void set_read_delay(int32_t read_delay) { this->read_delay_ = read_delay; }

  bool read_bytes(RegisterAddress register_address, uint8_t *data, size_t count, int32_t read_delay = -1) {
    auto result = this->i2c_device.write(register_address.data(), register_address.size());
    if (result != i2c::ERROR_OK) {
      // ESPHome's i2c component will have already logged this at very verbose level; since the assumption is that most
      // consumers of the custom_i2c component won't do anything to handle errors like this, re-log it at debug level so
      // that users will see it with the default log settings. (We log it with a separate tag so that users can disable
      // failure messages if they're expected - e.g. on a bus where devices are hot-swapped - and too verbose.)
      ESP_LOGD(TRANSACTION_FAILURE_TAG, "read from device 0x%02x failed with error %d", this->device_address_,
               result);  // NOLINT
      return false;
    }

    if (read_delay == -1) {
      read_delay = this->read_delay_;
    }

    if (read_delay_ > 0) {
      delayMicroseconds(read_delay_);
    }

    return this->i2c_device.read(data, count) == i2c::ERROR_OK;
  }

  template<size_t bytes>
  optional<std::array<uint8_t, bytes>> maybe_read_bytes(RegisterAddress register_address, int32_t read_delay = -1) {
    std::array<uint8_t, bytes> data;
    if (!this->read_bytes(register_address, data.data(), data.size(), read_delay)) {
      return {};
    }
    return data;
  }

  optional<std::vector<uint8_t>> maybe_read_bytes(RegisterAddress register_address, size_t count,
                                                  int32_t read_delay = -1) {
    std::vector<uint8_t> data(count);
    if (!this->read_bytes(register_address, data.data(), count, read_delay)) {
      return {};
    }
    return data;
  }

  template<size_t bytes>
  std::array<uint8_t, bytes> read_bytes(RegisterAddress register_address, int32_t read_delay = -1) {
    optional<std::array<uint8_t, bytes>> result = this->maybe_read_bytes<bytes>(register_address, read_delay);
    if (result.has_value()) {
      return result.value();
    } else {
      return std::array<uint8_t, bytes>{};
    }
  }

  std::vector<uint8_t> read_bytes(RegisterAddress register_address, size_t count, int32_t read_delay = -1) {
    optional<std::vector<uint8_t>> result = this->maybe_read_bytes(register_address, count, read_delay);
    if (result.has_value()) {
      return result.value();
    } else {
      // Mirror the std::array-specialized variant's behavior of returning a zero-initialized list of bytes so that
      // the caller still gets the number of bytes they expect. We could conceivably add a parameter that lets the user
      // request that an empty vector be returned in such a case, although the caller can obtain the same result by
      // calling the std::optional<std::vector> variant and then calling `.value_or(std::vector<uint8_t>())` on the
      // result.
      return std::vector<uint8_t>(count);
    }
  }

  bool write_bytes(RegisterAddress register_address, uint8_t *data, size_t count) {
    i2c::WriteBuffer buffers[2];
    buffers[0].data = register_address.data();
    buffers[0].len = register_address.size();
    buffers[1].data = data;
    buffers[1].len = count;
    // TODO: Same note as above - consider logging if the result is anything but ERROR_OK

    auto result = this->bus_->writev(this->device_address_, buffers, 2);
    if (result != i2c::ERROR_OK) {
      // See the comment in read_bytes for why we do this
      ESP_LOGD(TRANSACTION_FAILURE_TAG, "write to device 0x%02x failed with error %d", this->device_address_,
               result);  // NOLINT
    }

    return result;
  }

  template<size_t bytes> bool write_bytes(RegisterAddress register_address, std::array<uint8_t, bytes> data) {
    return this->write_bytes(register_address, data.data(), data.size());
  }

  bool write_bytes(RegisterAddress register_address, std::vector<uint8_t> data) {
    return this->write_bytes(register_address, data.data(), data.size());
  }

  template<typename T> optional<T> maybe_read_as(RegisterAddress register_address, int32_t read_delay = -1) {
    static_assert(!std::is_same<T, std::string>::value,
                  "custom_i2c: read_as and maybe_read_as don't support std::string yet.");

    optional<std::array<uint8_t, sizeof(T)>> result = this->maybe_read_bytes<sizeof(T)>(register_address, read_delay);

    if (result.has_value()) {
      return from_i2c<T>(result.value());
    } else {
      return {};
    }
  }

  template<typename T> T read_as(RegisterAddress register_address, int32_t read_delay = -1) {
    return this->maybe_read_as<T>(register_address, read_delay).value_or(T{});
  }

  template<typename T> bool write_as(RegisterAddress register_address, T value) {
    static_assert(!std::is_same<T, std::string>::value, "custom_i2c: write_as doesn't support std::string yet.");
    return this->write_bytes(register_address, to_i2c<T>(value));
  }

  i2c::I2CDevice i2c_device{};

 protected:
  i2c::I2CBus *bus_{};
  uint8_t device_address_{};
  int32_t read_delay_ = -1;
};

class CustomI2CRegister : public Component {
 public:
  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot a setup order annoyance
    ESP_LOGV(TAG, "Setting up custom_i2c register");  // NOLINT
  }
  void loop() override {}
  void dump_config() override {}
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_custom_i2c_device(CustomI2CDevice *device) { this->device_ = device; }
  void set_read_delay(int32_t read_delay) { this->read_delay_ = read_delay; }
  void set_register_address(RegisterAddress register_address) { this->register_address_ = register_address; }

  bool read_bytes(uint8_t *data, size_t count) {
    return this->device_->read_bytes(this->register_address_, data, count, this->read_delay_);
  }

  template<size_t bytes> std::array<uint8_t, bytes> read_bytes() {
    return this->device_->read_bytes<bytes>(this->register_address_, this->read_delay_);
  }

  std::vector<uint8_t> read_bytes(size_t count) {
    return this->device_->read_bytes(this->register_address_, count, this->read_delay_);
  }

  template<size_t bytes> optional<std::array<uint8_t, bytes>> maybe_read_bytes() {
    return this->device_->maybe_read_bytes<bytes>(this->register_address_, this->read_delay_);
  }

  optional<std::vector<uint8_t>> maybe_read_bytes(size_t count) {
    return this->device_->maybe_read_bytes(this->register_address_, count, this->read_delay_);
  }

  bool write_bytes(uint8_t *data, size_t count) {
    return this->device_->write_bytes(this->register_address_, data, count);
  }

  template<size_t bytes> bool write_bytes(std::array<uint8_t, bytes> data) {
    return this->device_->write_bytes(this->register_address_, data);
  }

  bool write_bytes(std::vector<uint8_t> data) { return this->device_->write_bytes(this->register_address_, data); }

  template<typename T> optional<T> maybe_read_as() {
    return this->device_->maybe_read_as<T>(this->register_address_, this->read_delay_);
  }

  template<typename T> T read_as() { return this->device_->read_as<T>(this->register_address_, this->read_delay_); }

  template<typename T> bool write_as(T value) { return this->device_->write_as<T>(this->register_address_, value); }

  uint8_t read_unsigned_int_8() { return this->read_as<uint8_t>(); }
  uint8_t read_unsigned_int_16() { return this->read_as<uint16_t>(); }
  uint8_t read_unsigned_int_32() { return this->read_as<uint32_t>(); }
  uint8_t read_signed_int_8() { return this->read_as<int8_t>(); }
  uint8_t read_signed_int_16() { return this->read_as<int16_t>(); }
  uint8_t read_signed_int_32() { return this->read_as<int32_t>(); }

  void write_unsigned_int_8(uint8_t value) { this->write_as<uint8_t>(value); }
  void write_unsigned_int_16(uint16_t value) { this->write_as<uint16_t>(value); }
  void write_unsigned_int_32(uint32_t value) { this->write_as<uint32_t>(value); }
  void write_signed_int_8(int8_t value) { this->write_as<int8_t>(value); }
  void write_signed_int_16(int16_t value) { this->write_as<int16_t>(value); }
  void write_signed_int_32(int32_t value) { this->write_as<int32_t>(value); }

 protected:
  CustomI2CDevice *device_{};
  RegisterAddress register_address_{};
  int32_t read_delay_ = -1;
};

// TODO
// template<typename T> class CustomI2CValueTypedRegister : public CustomI2CRegister {
//   T read() { return read_as<T>(); }
//   void write(T value) { return write_as<T>(value); }
// };

template<typename... Ts> class MessageBuilder {
 public:
  virtual void to_message(std::vector<uint8_t> &message, Ts... x) = 0;
};

template<typename T, typename... Ts> class ValueMessageBuilder : public MessageBuilder<Ts...> {
 public:
  TEMPLATABLE_VALUE(T, value);

  void to_message(std::vector<uint8_t> &message, Ts... x) override {
    std::array<uint8_t, sizeof(T)> data = to_i2c<T>(this->value_.value(x...));
    message.insert(message.end(), data.begin(), data.end());
  }
};

// Optimization for the common case of arrays of bytes
template<size_t bytes, typename... Ts> class BytesMessageBuilder : public MessageBuilder<Ts...> {
 public:
  BytesMessageBuilder(std::array<uint8_t, bytes> data) : data_(data) {}

  void to_message(std::vector<uint8_t> &message, Ts... x) override {
    message.insert(message.end(), this->data_.begin(), this->data_.end());
  }

 protected:
  std::array<uint8_t, bytes> data_;
};

// Only used with lambdas; for static lists of bytes declared in config, BytesMessageBuilder is generated instead
template<typename... Ts> class VectorMessageBuilder : public MessageBuilder<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, vector);

  void to_message(std::vector<uint8_t> &message, Ts... x) override {
    std::vector<uint8_t> data = this->vector_.value(x...);
    message.insert(message.end(), data.begin(), data.end());
  }
};

// NOTE: The current implementation where each message builder is separately instantiated and pointed to by a
// CompositeMessageBuilder is a farcical waste of memory, but the alternative - storing them directly in a tuple - runs
// into issues because that would require this class be templated on two parameter packs: the types of the child
// builders and the types of any lambda arguments.
// I haven't figured a way around that so we're doing the pointer-based approach for now, but contributions gladly
// welcome.
// (And note that the memory waste is 4 bytes of RAM for each separate non-byte-array component of a message, so it's
// actually not as bad as it seems at first glance.)
template<size_t builder_count, typename... Ts> class CompositeMessageBuilder : public MessageBuilder<Ts...> {
 public:
  CompositeMessageBuilder(std::array<MessageBuilder<Ts...> *, builder_count> builders) : builders_(builders) {}

  void to_message(std::vector<uint8_t> &message, Ts... x) override {
    for (MessageBuilder<Ts...> *builder : this->builders_) {
      builder->to_message(message, x...);
    }
  }

 protected:
  std::array<MessageBuilder<Ts...> *, builder_count> builders_;
};

// Ts = lambda argument types
template<typename... Ts> class WriteAction : public Action<Ts...> {
 public:
  void set_device(CustomI2CDevice *device) { this->device_ = device; }
  void set_register(CustomI2CRegister *register__) { this->register_ = register__; }
  void set_message_builder(MessageBuilder<Ts...> *message_builder) { this->message_builder_ = message_builder; }

  void play(Ts... x) override {
    std::vector<uint8_t> message;
    this->message_builder_->to_message(message, x...);

    if (this->device_) {
      this->device_->write_bytes({}, message);
    } else if (this->register_) {
      this->register_->write_bytes(message);
    }
  }

 protected:
  CustomI2CDevice *device_{};
  CustomI2CRegister *register_{};
  MessageBuilder<Ts...> *message_builder_{};
};

// TODO: move all the output related stuff into its own header that's only included when outputs are used
#ifdef USE_OUTPUT
template<uint8_t bytes> class CustomI2COutput : public output::FloatOutput, public Component {
 public:
  // mind you, I'll eat my hat if anyone so much as has a need for 4 bytes in the first place
  static_assert(bytes > 0 && bytes <= 4,
                "custom_i2c float outputs only support writing up to 4 bytes for now. If you need "
                "support for more, please open an issue.");

  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot a setup order annoyance
    ESP_LOGV(TAG, "Setting up custom_i2c output");  // NOLINT
    this->turn_off();
  }
  void dump_config() override {}
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_register(CustomI2CRegister *register__) { this->register_ = register__; }
  void write_state(float state) override {
    float max = static_cast<float>(std::numeric_limits<uint32_t>::max());
    float multiplied_state = state * max;

    uint32_t result;
    if (std::nextafter(multiplied_state, INFINITY) >= max) {
      result = std::numeric_limits<uint32_t>::max();
    } else {
      result = static_cast<uint32_t>(multiplied_state);
    }

    ESP_LOGVV(FLOAT_OUTPUT_TAG, "converted state %.6f to float value %.6f to uint32_t value 0x%08x", state,
              multiplied_state, result);  // NOLINT

    result = convert_big_endian(result);
    std::array<uint8_t, bytes> data;
    memcpy(data.data(), reinterpret_cast<uint8_t *>(&result), bytes);

    this->register_->write_bytes(data);
  }

 protected:
  CustomI2CRegister *register_{};
};
#endif

template<size_t bytes> class CustomI2CPinBank : public Component {
 public:
  using Word = std::array<uint8_t, bytes>;

  class PinLocation {
   public:
    PinLocation(uint8_t bit_index, uint8_t byte_index)
        : bit_index(bit_index), byte_index(byte_index), mask(uint8_t{1} << bit_index) {}
    uint8_t bit_index;
    uint8_t byte_index;
    uint8_t mask;

    bool is_valid() { return !(this->bit_index == 255 && this->byte_index == 255); }
    static PinLocation invalid() { return PinLocation(255, 255); }

    static PinLocation for_pin(uint8_t pin) {
      uint8_t byte_index = (pin / 8);
      uint8_t bit_index = pin % 8;
      if (byte_index >= bytes) {
        ESP_LOGE(TAG, "pin number %d too high for pin bank with %d bytes (max pin number is %d)", pin, bytes,
                 (bytes * 8) - 1);  // NOLINT
        return invalid();
      }

      byte_index = bytes - byte_index - 1;  // so that the lowest numbered pins go in the last byte instead of the first

      return PinLocation(bit_index, byte_index);
    }
  };

  class PinBankRegister {
   public:
    void set_register(CustomI2CRegister *register__) { this->register_ = register__; }
    void enable_cache() { this->cache_enabled_ = true; }

    bool read_bit(PinLocation pin) {
      // Ideally we'd statically assert whether or not this particular register had been configured and blow up if not
      if (!this->register_) {
        return false;
      }

      Word data;
      if (this->cache_enabled_) {
        data = this->cache;
      } else {
        data = this->register_->read_bytes<bytes>();
      }

      return (data[pin.byte_index] & pin.mask) != 0;
    }

    void update_bit(PinLocation pin, bool value) {
      if (!this->register_) {
        return;
      }

      Word data;
      if (this->cache_enabled_) {
        data = this->cache;
      } else {
        data = this->register_->read_bytes<bytes>();
      }

      if (value) {
        data[pin.byte_index] |= pin.mask;
      } else {
        data[pin.byte_index] &= ~pin.mask;
      }

      this->register_->write_bytes(data);
      if (this->cache_enabled_) {
        this->cache = data;
      }
    }

    void write_a_single_1_bit(PinLocation pin) {
      if (!this->register_) {
        return;
      }

      ESP_LOGD(TAG, "writing a 1 to bit %d of byte %d", pin.bit_index, pin.byte_index);  // NOLINT

      Word data{};
      data[pin.byte_index] |= pin.mask;

      this->register_->write_bytes(data);
    }

   protected:
    CustomI2CRegister *register_{};
    bool cache_enabled_ = false;
    Word cache{};
  };

  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot a setup order annoyance
    ESP_LOGV(TAG, "setting up custom_i2c pin bank");  // NOLINT
  }

  void pin_mode(uint8_t pin_number, gpio::Flags flags) {
    PinLocation pin = PinLocation::for_pin(pin_number);
    if (!pin.is_valid()) {
      return;
    }

    if (flags & gpio::FLAG_INPUT) {
      this->write_io_directions_register.update_bit(pin, true);
      this->write_inverted_io_directions_register.update_bit(pin, false);
      this->write_ones_to_set_to_inputs_register.write_a_single_1_bit(pin);

      if (flags & gpio::FLAG_PULLUP) {
        this->write_pull_resistors_enabled_register.update_bit(pin, true);
        this->write_inverted_pull_resistors_enabled_register.update_bit(pin, false);
        this->write_ones_to_enable_pull_resistors_register.write_a_single_1_bit(pin);

        this->write_pull_directions_register.update_bit(pin, true);
        this->write_inverted_pull_directions_register.update_bit(pin, false);
        this->write_ones_to_enable_pullups_register.write_a_single_1_bit(pin);
      } else if (flags & gpio::FLAG_PULLDOWN) {
        this->write_pull_resistors_enabled_register.update_bit(pin, true);
        this->write_inverted_pull_resistors_enabled_register.update_bit(pin, false);
        this->write_ones_to_enable_pull_resistors_register.write_a_single_1_bit(pin);

        this->write_pull_directions_register.update_bit(pin, false);
        this->write_inverted_pull_directions_register.update_bit(pin, true);
        this->write_ones_to_enable_pulldowns_register.write_a_single_1_bit(pin);
      } else {
        this->write_pull_resistors_enabled_register.update_bit(pin, false);
        this->write_inverted_pull_resistors_enabled_register.update_bit(pin, true);
        this->write_ones_to_disable_pull_resistors_register.write_a_single_1_bit(pin);
      }
    } else if (flags & gpio::FLAG_OUTPUT) {
      this->write_io_directions_register.update_bit(pin, false);
      this->write_inverted_io_directions_register.update_bit(pin, true);
      this->write_ones_to_set_to_outputs_register.write_a_single_1_bit(pin);

      if (flags & gpio::FLAG_OPEN_DRAIN) {
        ESP_LOGW(TAG, "open drain pin mode is not yet supported. (pull requests welcome!)");  // NOLINT
      } else {
        // Once we support open drain outputs, we should have a separate set of "is a non-open-drain output" registers
        // and write those here.
      }
    }
  }
  bool digital_read(uint8_t pin_number) {
    PinLocation pin = PinLocation::for_pin(pin_number);
    if (!pin.is_valid()) {
      return false;
    }

    return this->read_input_states_register.read_bit(pin);
  }
  void digital_write(uint8_t pin_number, bool value) {
    PinLocation pin = PinLocation::for_pin(pin_number);
    if (!pin.is_valid()) {
      return;
    }

    this->write_output_states_register.update_bit(pin, value);
    if (value) {
      this->write_ones_to_set_outputs_high_register.write_a_single_1_bit(pin);
    } else {
      this->write_ones_to_set_outputs_low_register.write_a_single_1_bit(pin);
    }
  };

  // I don't love having all these data structures where most of them are going to be empty for any given pin bank.
  // Haven't settled on a better approach though, so this'll do for now.
  // (but: said better approach is probably going to look like a separate global pin bank register object for each
  // specified register, and the registers will all, ahem, register themselves with the pin bank, and the pin bank
  // will then inform them all in order whenever pin_mode or digital_write are called and they'll react if they know
  // they're supposed to do something based on that. (digital_read would likely be a special case in that scenario,
  // which is fine.))
  // TODO: add support for open drain mode outputs
  // TODO: also add support for configuring per-pin interrupts on chips that support that
  PinBankRegister read_input_states_register{};
  PinBankRegister write_output_states_register{};
  PinBankRegister write_ones_to_set_outputs_high_register{};
  PinBankRegister write_ones_to_set_outputs_low_register{};
  PinBankRegister write_io_directions_register{};
  PinBankRegister write_inverted_io_directions_register{};
  PinBankRegister write_ones_to_set_to_inputs_register{};
  PinBankRegister write_ones_to_set_to_outputs_register{};
  PinBankRegister write_pull_resistors_enabled_register{};
  PinBankRegister write_inverted_pull_resistors_enabled_register{};
  PinBankRegister write_ones_to_enable_pull_resistors_register{};
  PinBankRegister write_ones_to_disable_pull_resistors_register{};
  PinBankRegister
      write_pull_directions_register{};  // TODO: Need to deal with the case where this is the same as
                                         // write_output_states_register and read_input_states_register, as in
                                         // devices powered by Adafruit's seesaw firmware... except hm actually this is
                                         // fine, the user just can't turn caching on for it - so might be worth
                                         // scanning to see if the same register is used twice in the same pin bank and
                                         // disallow caching on either register if so (or allow them to maintain a
                                         // shared cache, but I don't know of any chips that would require that so maybe
                                         // don't bother for now.)
  PinBankRegister write_inverted_pull_directions_register{};
  PinBankRegister write_ones_to_enable_pullups_register{};
  PinBankRegister write_ones_to_enable_pulldowns_register{};
};

template<size_t bytes> class CustomI2CPin : public GPIOPin {
 public:
  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot a setup order annoyance
    ESP_LOGV(TAG, "Setting up custom_i2c pin with number %d", this->pin_);  // NOLINT
    this->pin_mode(this->flags_);
  }
  void pin_mode(gpio::Flags flags) override { this->pin_bank_->pin_mode(this->pin_, this->flags_); }
  bool digital_read() override { return this->pin_bank_->digital_read(this->pin_) ^ this->inverted_; }
  void digital_write(bool value) override { this->pin_bank_->digital_write(this->pin_, value ^ this->inverted_); }
  std::string dump_summary() const override { return "CustomI2CPin (TODO)"; }

  void set_pin_bank(CustomI2CPinBank<bytes> *pin_bank) { this->pin_bank_ = pin_bank; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

 protected:
  CustomI2CPinBank<bytes> *pin_bank_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace esphome::custom_i2c
