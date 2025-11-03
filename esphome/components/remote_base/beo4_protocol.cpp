#include "beo4_protocol.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.beo4";

// beo4 pulse width, high=carrier_pulse low=data_pulse
constexpr uint16_t PW_CARR_US = 200;     // carrier pulse length
constexpr uint16_t PW_ZERO_US = 2925;    // + 200 =  3125 µs
constexpr uint16_t PW_SAME_US = 6050;    // + 200 =  6250 µs
constexpr uint16_t PW_ONE_US = 9175;     // + 200 =  9375 µs
constexpr uint16_t PW_STOP_US = 12300;   // + 200 = 12500 µs
constexpr uint16_t PW_START_US = 15425;  // + 200 = 15625 µs

// beo4 pulse codes
constexpr uint8_t PC_ZERO = (PW_CARR_US + PW_ZERO_US) / 3125;    // =1
constexpr uint8_t PC_SAME = (PW_CARR_US + PW_SAME_US) / 3125;    // =2
constexpr uint8_t PC_ONE = (PW_CARR_US + PW_ONE_US) / 3125;      // =3
constexpr uint8_t PC_STOP = (PW_CARR_US + PW_STOP_US) / 3125;    // =4
constexpr uint8_t PC_START = (PW_CARR_US + PW_START_US) / 3125;  // =5

// beo4 number of data bits = beoLink+beoSrc+beoCmd = 1+8+8 = 17
constexpr uint32_t N_BITS = 1 + 8 + 8;

// required symbols = 2*(start_sequence + n_bits + stop) = 2*(3+17+1) = 42
constexpr uint32_t N_SYM = 2 + ((3 + 17 + 1) * 2u);  // + 2 = 44

// states finite-state-machine decoder
enum class RxSt { RX_IDLE, RX_DATA, RX_STOP };

void Beo4Protocol::encode(RemoteTransmitData *dst, const Beo4Data &data) {
  uint32_t beo_code = ((uint32_t) data.source << 8) + (uint32_t) data.command;
  uint32_t jc = 0, ic = 0;
  uint32_t cur_bit = 0;
  uint32_t pre_bit = 0;
  dst->set_carrier_frequency(455000);
  dst->reserve(N_SYM);

  // start sequence=zero,zero,start
  dst->item(PW_CARR_US, PW_ZERO_US);
  dst->item(PW_CARR_US, PW_ZERO_US);
  dst->item(PW_CARR_US, PW_START_US);

  // the data-bit BeoLink is always 0
  dst->item(PW_CARR_US, PW_ZERO_US);

  // The B&O trick to avoid extra long and extra short
  // code-frames by extracting the data-bits from left
  // to right, then comparing current with previous bit
  // and set pulse to "same" "one" or "zero"
  for (jc = 15, ic = 0; ic < 16; ic++, jc--) {
    cur_bit = ((beo_code) >> jc) & 1;
    if (cur_bit == pre_bit) {
      dst->item(PW_CARR_US, PW_SAME_US);
    } else if (1 == cur_bit) {
      dst->item(PW_CARR_US, PW_ONE_US);
    } else {
      dst->item(PW_CARR_US, PW_ZERO_US);
    }
    pre_bit = cur_bit;
  }
  // complete the frame with stop-symbol and final carrier pulse
  dst->item(PW_CARR_US, PW_STOP_US);
  dst->mark(PW_CARR_US);
}

optional<Beo4Data> Beo4Protocol::decode(RemoteReceiveData src) {
  int32_t n_sym = src.size();
  Beo4Data data{
      .source = 0,
      .command = 0,
      .repeats = 0,
  };
  // suppress dummy codes (TSO7000 hiccups)
  if (n_sym > 42) {
    static uint32_t beo_code = 0;
    RxSt fsm = RxSt::RX_IDLE;
    int32_t ic = 0;
    int32_t jc = 0;
    uint32_t pre_bit = 0;
    uint32_t cnt_bit = 0;
    ESP_LOGD(TAG, "Beo4: n_sym=%" PRId32, n_sym);
    for (jc = 0, ic = 0; ic < (n_sym - 1); ic += 2, jc++) {
      int32_t pulse_width = src[ic] - src[ic + 1];
      // suppress TSOP7000 (dummy pulses)
      if (pulse_width > 1500) {
        int32_t pulse_code = (pulse_width + 1560) / 3125;
        switch (fsm) {
          case RxSt::RX_IDLE: {
            beo_code = 0;
            cnt_bit = 0;
            pre_bit = 0;
            if (PC_START == pulse_code) {
              fsm = RxSt::RX_DATA;
            }
            break;
          }
          case RxSt::RX_DATA: {
            uint32_t cur_bit = 0;
            switch (pulse_code) {
              case PC_ZERO: {
                cur_bit = pre_bit = 0;
                break;
              }
              case PC_SAME: {
                cur_bit = pre_bit;
                break;
              }
              case PC_ONE: {
                cur_bit = pre_bit = 1;
                break;
              }
              default: {
                fsm = RxSt::RX_IDLE;
                break;
              }
            }
            beo_code = (beo_code << 1) + cur_bit;
            if (++cnt_bit == N_BITS) {
              fsm = RxSt::RX_STOP;
            }
            break;
          }
          case RxSt::RX_STOP: {
            if (PC_STOP == pulse_code) {
              data.source = (uint8_t) ((beo_code >> 8) & 0xff);
              data.command = (uint8_t) ((beo_code) &0xff);
              data.repeats++;
            }
            if ((n_sym - ic) < 42) {
              return data;
            } else {
              fsm = RxSt::RX_IDLE;
            }
            break;
          }
        }
      }
    }
  }
  return {};  // decoding failed
}

void Beo4Protocol::dump(const Beo4Data &data) {
  ESP_LOGI(TAG, "Beo4: source=0x%02x command=0x%02x repeats=%d ", data.source, data.command, data.repeats);
}

}  // namespace remote_base
}  // namespace esphome
