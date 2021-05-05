#pragma once

#include <queue>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace simpleevse {

extern const char *TAG;

const uint32_t STATUS_POLL_INTERVAL{5000};

const uint16_t FIRST_STATUS_REGISTER{1000};
const uint16_t COUNT_STATUS_REGISTER{7};

enum StatusRegister : uint16_t {
  REGISTER_CHARGE_CURRENT = 0,
  REGISTER_ACTUAL_CURRENT = 1,
  REGISTER_VEHICLE_STATE = 2,
  REGISTER_MAX_CURRENT = 3,
  REGISTER_CTRL_BITS = 4,
  REGISTER_FIRMWARE = 5,
  REGISTER_EVSE_STATE = 6,
};

enum VehicleState : uint16_t {
  VEHICLE_UNKNOWN = 0,
  VEHICLE_READY = 1,
  VEHICLE_EV_PRESENT = 2,
  VEHICLE_CHARGING = 3,
  VEHICLE_CHARGING_WITH_VENT = 4,
};

const uint16_t CHARGING_ENABLED_ON{0x0000};
const uint16_t CHARGING_ENABLED_OFF{0x0001};
const uint16_t CHARGING_ENABLED_MASK{0x0001};

const uint16_t FIRST_CONFIG_REGISTER{2000};
const uint16_t COUNT_CONFIG_REGISTER{18};

// modbus constants
const uint32_t BAUD_RATE{9600};
const uint8_t STOP_BITS{1};
const uint8_t PARITY_BITS{8};
const size_t MIN_FRAME_LEN{4}; // min. size for evaluation (address + function + 2* CRC)
const uint8_t MODBUS_ADDRESS{1}; // default SimpleEVSE Modbus address
const uint8_t EXCEPTION_MASK{0x7F};
const uint32_t RESPONSE_TIMEOUT{1000}; // timeout in ms

const uint8_t MAX_REQUEST_ATTEMPTS{3};

// Utility class to read data from a received frame
class BufferStreamReader 
{
  public:
    BufferStreamReader(std::vector<uint8_t>::const_iterator begin, std::vector<uint8_t>::const_iterator end) : it_(begin), end_(end) {}

    BufferStreamReader &operator>>(uint8_t &value) {
      if (this->it_ != this->end_) {
        value = *this->it_++;
      } else {
        value = 0;
        this->error_ = true;
      }

      return *this;
    }

    BufferStreamReader &operator>>(uint16_t &value) {
      if ((this->end_ - this->it_) >= 2) {
        uint8_t msb = *this->it_++;
        uint8_t lsb = *this->it_++;
        value = encode_uint16(msb, lsb);
      } else {
        value = 0;
        this->error_ = true;
      }

      return *this;
    }

    uint8_t remaining_size() const { return this->end_ - this->it_; }
    bool error() const { return this->error_; }

  protected:
    std::vector<uint8_t>::const_iterator it_;
    const std::vector<uint8_t>::const_iterator end_;
    bool error_{false};
};

// Utility class to write data to a frame
class BufferStreamWriter {
  public:
    explicit BufferStreamWriter(std::back_insert_iterator<std::vector<uint8_t>> inserter) : inserter_(inserter) {}

    BufferStreamWriter &operator<<(uint8_t value) {
      this->inserter_ = value;
      return *this;
    }

    BufferStreamWriter &operator<<(uint16_t value) {
      auto decoded = decode_uint16(value);
      this->inserter_ = decoded[0];
      this->inserter_ = decoded[1];
      return *this;
    }

  protected:
    std::back_insert_iterator<std::vector<uint8_t>> inserter_;
};


/// Enumeration of possible modbus transaction results.
enum class ModbusTransactionResult {
  SUCCESS,    // transaction was successfully executed
  EXCEPTION,  // modbus slaved returned an exception
  TIMEOUT,    // no response from modbus slave
  CANCELLED,  // transaction was cancelled by master
};

/// Base class for all Modbus transactions.
class ModbusTransaction {
  public:
    virtual ~ModbusTransaction() = default; // important to avoid memory leaks when bases clases are destroyed
    /// returns the complete frame for this request
    std::vector<uint8_t> encode_request();
    /// tries to decode a frame, returns true on success
    bool decode_response(const std::vector<uint8_t> &buffer);
    /// returns true if the timeout for this request is expired
    bool check_timeout() { return (millis() - this->send_time_) >= RESPONSE_TIMEOUT; }
    /// cancels this request
    void cancel() { this->on_error(ModbusTransactionResult::CANCELLED); }
    /// times out this request
    void timeout() { this-> on_error(ModbusTransactionResult::TIMEOUT); }
    /// Returns if the request should be retried after a timeout
    bool should_retry() const { return this->attempt_ > 0; }

  protected:
    ModbusTransaction(uint8_t function_code, uint8_t request_size, uint8_t response_size) : function_code_(function_code), request_size_(request_size), response_size_(response_size) {}
    // subclasses must insert their request data without header and crc
    virtual void on_get_data(BufferStreamWriter &writer) = 0;
    // subclasses must handle their response data without header and crc
    virtual void on_handle_data(BufferStreamReader &reader) = 0;
    // subclasses must handle exception responses
    virtual void on_error(ModbusTransactionResult result) = 0;

    const uint8_t frame_size_{MIN_FRAME_LEN};
    const uint8_t function_code_;
    const uint8_t request_size_;
    const uint8_t response_size_;

    uint32_t send_time_{0};
    uint8_t attempt_{MAX_REQUEST_ATTEMPTS};
};

/// Implements a Modbus transaction for reading multiple holding registers.
class ModbusReadHoldingRegistersTransaction : public ModbusTransaction {
  public:
    using callback_t = std::function<void(ModbusTransactionResult, const std::vector<uint16_t>&)>;

    ModbusReadHoldingRegistersTransaction(uint16_t address, uint16_t count, callback_t &&callback) : ModbusTransaction(0x03, 4, count*2+1), address_(address), count_(count), callback_(std::move(callback)) {}
    virtual ~ModbusReadHoldingRegistersTransaction() = default;

  protected:
    void on_get_data(BufferStreamWriter &writer) override;
    void on_handle_data(BufferStreamReader &reader) override;
    void on_error(ModbusTransactionResult result) override;

    const uint16_t address_;
    const uint16_t count_;
    const callback_t callback_;
};

/// Implements a Modbus transaction for writing a single holding register.
class ModbusWriteHoldingRegistersTransaction : public ModbusTransaction {
  public:
    using callback_t = std::function<void(ModbusTransactionResult)>;

    // for simplification only one register is implemented even this function provides multiple
    ModbusWriteHoldingRegistersTransaction(uint16_t address, uint16_t value, callback_t &&callback) : ModbusTransaction(0x10, 7, 4), address_(address), value_(value), callback_(std::move(callback)) {}
    virtual ~ModbusWriteHoldingRegistersTransaction() = default;

  protected:
    void on_get_data(BufferStreamWriter &writer) override;
    void on_handle_data(BufferStreamReader &reader) override;
    void on_error(ModbusTransactionResult result) override;

    const uint16_t address_;
    const uint16_t value_;
    const callback_t callback_;
};

/** This class implements a generic Modbus interface based on the UARTDevice.
 *  
 * This implementation supports Modbus transactions which are implementations
 * of the ModbusTransaction class and handles all specific parts of the
 * communication. Transaction can be processed by calling the execute() method.
 */
class ModbusDeviceComponent : public uart::UARTDevice, public Component {
  public:
    float get_setup_priority() const { return setup_priority::BUS - 1.0f; /* after UART */ }
    void loop() override;

  protected:
    /// Executes the transaction, that is sends a request and waits for a response.
    void execute(std::unique_ptr<ModbusTransaction> &&transaction);
    /// Called when no modbus activity is running and a new transaction can be started
    virtual void idle() {};
    // sends the request from the current active transaction
    void send_request();

    /// Time in us after a frame is consideres as complete.
    const uint32_t inter_char_timeout_{1563}; // (1 + 8 + 1) * 1.5 / 9600
    /// Time in us of silence before a new frame can be sent.
    const uint32_t inter_frame_timeout_{3646}; // (1 + 8 + 1) * 3.5 / 9600

    /// Buffer for the received data from the UART.
    std::vector<uint8_t> rx_buffer_;
    /// Relative time of the last received byte in us.
    uint32_t last_modbus_byte_{0};
    /// True if the last serial activity is less than the inter frame timeout.
    bool after_char_delay_{false};
    /// Contains the current active transaction.
    std::unique_ptr<ModbusTransaction> active_transaction_{nullptr};
};

/** Interface for classes which will be notified about status updates. */
class UpdateListener {
  public:
    virtual void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register) = 0;
};

/** Implements the SimpleEVSE communication on top of the Modbus
 * class. */
class SimpleEvseComponent : public ModbusDeviceComponent {
  public:
    void dump_config() override;
    /// Adds a transaction to the execution queue.
    void add_transaction(std::unique_ptr<ModbusTransaction> &&transaction);

    void add_observer(UpdateListener *observer) { this->observer_.push_back(observer); }

    std::array<uint16_t, COUNT_STATUS_REGISTER> get_register() const { return this->status_register_; }

    void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
    uint32_t get_update_interval() const { return this->update_interval_; }

    void set_unplugged_trigger(Trigger<> *trigger) { this->unplugged_trigger_ = trigger; }
    void set_plugged_trigger(Trigger<> *trigger) { this->plugged_trigger_ = trigger; }

  protected:

    void idle() override;

    void on_status_received(ModbusTransactionResult result, const std::vector<uint16_t> &reg);
    void process_triggers();

    // status of SimpleEVSE - will be updated regularly
    bool running_{false};
    std::array<uint16_t, COUNT_STATUS_REGISTER> status_register_;
    uint32_t update_interval_{STATUS_POLL_INTERVAL};

    /// Time of last status update.
    uint32_t last_state_udpate_{0};
    bool force_update_{true};

    std::vector<UpdateListener*> observer_;

    /// Queued transactions which should be executed.
    std::queue<std::unique_ptr<ModbusTransaction>> transactions_;

    // Triggers
    Trigger<> *unplugged_trigger_{nullptr};
    Trigger<> *plugged_trigger_{nullptr};
    bool was_plugged_{false};
};

/** Action for setting the charging current.
 * 
 * This action expects a uint8_t value 'current' with the current to set.
 */
template<typename... Ts> class SetChargingCurrent : public Action<Ts...> {
 public:
  explicit SetChargingCurrent(SimpleEvseComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint8_t, current)

  virtual void play_complex(Ts... x) {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);
    
    uint8_t current = this->current_.value(x...);
    ESP_LOGI(TAG, "Set charging current by action to %d.", current);
    auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(1000, current, std::bind(&SetChargingCurrent::on_update, this, std::placeholders::_1));
    this->parent_->add_transaction(std::move(trans));
  }

  void play(Ts... x) override { /* ignore - see play complex */ }

 protected:
  SimpleEvseComponent *parent_;
  std::tuple<Ts...> var_{};

  void on_update(ModbusTransactionResult) {
    if (this->num_running_ > 0) {
      this->play_next_tuple_(this->var_);
    }
  }
};

/** Action for enable/disable the charging.
 * 
 * This action expects a bool value 'enable' with true or false.
 */
template<typename... Ts> class SetChargingEnabled : public Action<Ts...> {
 public:
  explicit SetChargingEnabled(SimpleEvseComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(bool, enabled)

  virtual void play_complex(Ts... x) {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);

    bool enable = this->enabled_.value(x...);
    ESP_LOGI(TAG, "Set charging enabled by action to %d.", enable);
    auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(1004, enable ? 0x0 : 0x1, std::bind(&SetChargingEnabled::on_update, this, std::placeholders::_1));
    this->parent_->add_transaction(std::move(trans));
  }  

  void play(Ts... x) override { /* ignore - see play complex */ }

 protected:
  SimpleEvseComponent *parent_;
  std::tuple<Ts...> var_{};

  void on_update(ModbusTransactionResult) {
    if (this->num_running_ > 0) {
      this->play_next_tuple_(this->var_);
    }
  }  
};

// Sensor platform
#ifdef USE_SENSOR
class SimpleEvseSensors : public UpdateListener {
  public:
    explicit SimpleEvseSensors(SimpleEvseComponent *parent) : parent_(parent) { parent->add_observer(this); }

    /* setter methods for sensor configuration */
    void set_connected_sensor(binary_sensor::BinarySensor *connected) { connected_ = connected; };
    void set_set_charge_current_sensor(sensor::Sensor *set_charge_current) { set_charge_current_ = set_charge_current; }
    void set_actual_charge_current_sensor(sensor::Sensor *actual_charge_current) { actual_charge_current_ = actual_charge_current; }
    void set_vehicle_state_sensor(text_sensor::TextSensor *vehicle_state_sensor) { vehicle_state_sensor_ = vehicle_state_sensor; }
    void set_max_current_limit_sensor(sensor::Sensor *max_current_limit) { max_current_limit_ = max_current_limit; }
    void set_firmware_revision_sensor(sensor::Sensor *firmware_revision) { firmware_revision_ = firmware_revision; }
    void set_evse_state_sensor(text_sensor::TextSensor *evse_state_sensor) { evse_state_sensor_ = evse_state_sensor; }

    void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register);

  protected:
    SimpleEvseComponent *const parent_;

    binary_sensor::BinarySensor *connected_{nullptr};
    sensor::Sensor *set_charge_current_{nullptr};
    sensor::Sensor *actual_charge_current_{nullptr};
    text_sensor::TextSensor *vehicle_state_sensor_{nullptr};
    sensor::Sensor *max_current_limit_{nullptr};
    sensor::Sensor *firmware_revision_{nullptr};  
    text_sensor::TextSensor *evse_state_sensor_{nullptr};    
};
#endif

// switch platform
#ifdef USE_SWITCH
class SimpleEvseChargingSwitch : public switch_::Switch, public UpdateListener {
  public:
    explicit SimpleEvseChargingSwitch(SimpleEvseComponent *parent) : parent_(parent) { parent->add_observer(this); }
    void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register);

  protected:
    void write_state(bool state) override;

    SimpleEvseComponent *const parent_;
    uint16_t active_writes_{0};
};
#endif

#ifdef USE_SIMPLEEVSE_WEB_CONFIG
/** Http handler for the setup/config register of the SimpleEVSE.
 * 
 * This handler provides a web page with the config register of the SimpleEVSE and
 * allows also the value to change.
 */
class SimpleEvseHttpHandler : public AsyncWebHandler, public Component {
 public:
  SimpleEvseHttpHandler(web_server_base::WebServerBase *base, SimpleEvseComponent *parent) : base_(base), parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    String url = request->url();
    return url == this->index_path || url == this->set_value_path;
  }

  void handleRequest(AsyncWebServerRequest *req) override;

  bool isRequestHandlerTrivial() override { return false; } // POST data

  void setup() override {
    this->base_->init();
    this->base_->add_handler(this);
  }

  float get_setup_priority() const override {
    // After WiFi
    return setup_priority::WIFI - 1.0f;
  }
protected:
  void handleIndex(AsyncWebServerRequest *req);
  void handleSetConfig(AsyncWebServerRequest *req);

  const String index_path = "/simpleevse";
  const String set_value_path = "/simpleevse/set_value";

  web_server_base::WebServerBase *const base_;
  SimpleEvseComponent *const parent_;
};
#endif

}  // namespace simpleevse
}  // namespace esphome
