#include <benchmark/benchmark.h>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/helpers.h"
#include "fake_uart.h"

namespace esphome::modbus::benchmarks {

using esphome::benchmarks::FakeUART;

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

static constexpr uint8_t ADDRESS = 1;
static constexpr uint8_t REGISTER_COUNT = 10;
// Read 10 holding registers from address 0, as an energy meter poll does
static constexpr uint8_t REQUEST_PDU[] = {0x03, 0x00, 0x00, 0x00, REGISTER_COUNT};
// Address + function + byte count + 20 data bytes + CRC
static constexpr size_t RESPONSE_SIZE = 3 + REGISTER_COUNT * 2 + 2;

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

class NoDelayHub : public ModbusClientHub {
 protected:
  // No wait before a send, so every loop() can send the next poll
  int32_t tx_delay_remaining() override { return 0; }
};

// --- One continuous poll cycle per loop(): receive and CRC check the response, decode it, requeue and resend ---

static void Modbus_ReadHoldingPollCycle(benchmark::State &state) {
  // Address 1, function 3, byte count 20, 10 registers; the CRC goes in the last two bytes
  uint8_t response[RESPONSE_SIZE] = {0x01, 0x03, 0x14, 0x09, 0x1F, 0x00, 0x00, 0x01, 0xF4, 0x00, 0x00, 0x13,
                                     0x88, 0x00, 0x32, 0x03, 0xE8, 0x00, 0x00, 0x00, 0x64, 0x12, 0x34};
  put_le16(&response[RESPONSE_SIZE - 2], crc16(response, RESPONSE_SIZE - 2));

  FakeUART uart;
  uart.set_baud_rate(9600);
  NoDelayHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  SumDevice device(&hub, ADDRESS);
  hub.queue_pdu(ADDRESS, REQUEST_PDU, &device, {.continuous = true});
  // The first loop() sends the poll; every send rewinds the response, so the device answers each one
  hub.loop();
  uart.load(response, RESPONSE_SIZE);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      hub.loop();
    }
    benchmark::DoNotOptimize(device.sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Modbus_ReadHoldingPollCycle);

}  // namespace esphome::modbus::benchmarks
