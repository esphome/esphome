#include <benchmark/benchmark.h>

#include <array>
#include <cstring>

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

static constexpr uint8_t ADDRESS = 1;
static constexpr uint16_t REGISTER_COUNT = 10;
// Read 10 holding registers from address 0, as an energy meter poll does
static constexpr uint8_t REQUEST_PDU[] = {0x03, 0x00, 0x00, 0x00, REGISTER_COUNT};
// Address + function + byte count + 20 data bytes + CRC
static constexpr size_t RESPONSE_SIZE = 3 + REGISTER_COUNT * 2 + 2;

// Serves the same response frame each time it is rewound.
class ResponseUART : public uart::UARTComponent {
 public:
  ResponseUART() {
    this->frame_[0] = ADDRESS;
    this->frame_[1] = 0x03;
    this->frame_[2] = REGISTER_COUNT * 2;
    static constexpr uint8_t DATA[REGISTER_COUNT * 2] = {0x09, 0x1F, 0x00, 0x00, 0x01, 0xF4, 0x00, 0x00, 0x13, 0x88,
                                                         0x00, 0x32, 0x03, 0xE8, 0x00, 0x00, 0x00, 0x64, 0x12, 0x34};
    std::memcpy(&this->frame_[3], DATA, sizeof(DATA));
    put_le16(&this->frame_[RESPONSE_SIZE - 2], crc16(this->frame_.data(), RESPONSE_SIZE - 2));
  }
  void rewind() { this->pos_ = 0; }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > RESPONSE_SIZE - this->pos_)
      return false;
    std::memcpy(data, this->frame_.data() + this->pos_, len);
    this->pos_ += len;
    return true;
  }
  size_t available() override { return RESPONSE_SIZE - this->pos_; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

 protected:
  std::array<uint8_t, RESPONSE_SIZE> frame_{};
  size_t pos_{RESPONSE_SIZE};
};

class SumDevice : public ModbusClientDevice {
 public:
  using ModbusClientDevice::ModbusClientDevice;
  void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                 ResponseStatus status) override {
    for (uint16_t value : registers)
      this->sum += value;
  }
  uint32_t sum{0};
};

// Holds one continuous poll on the wire so each frame is the awaited response.
class BenchHub : public ModbusClientHub {
 public:
  void await_response() {
    this->tx_buffer_.front().state = FrameState::WAITING;
    this->waiting_for_response_ = true;
  }
  void receive() { this->Modbus::loop(); }
};

// --- Receive, CRC check and decode a read holding registers response ---

static void Modbus_ReadHoldingResponse(benchmark::State &state) {
  ResponseUART uart;
  BenchHub hub;
  hub.set_uart_parent(&uart);
  SumDevice device(&hub, ADDRESS);
  hub.queue_pdu(ADDRESS, REQUEST_PDU, &device, {.continuous = true});

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      hub.await_response();
      uart.rewind();
      hub.receive();
    }
    benchmark::DoNotOptimize(device.sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Modbus_ReadHoldingResponse);

}  // namespace esphome::modbus::benchmarks
