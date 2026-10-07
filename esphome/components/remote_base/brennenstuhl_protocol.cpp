#include "brennenstuhl_protocol.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::remote_base {

static const char *const TAG = "remote.brennenstuhl";

// receiver timing ranges [µs]
constexpr uint32_t START_PULSE_MIN = 200;
constexpr uint32_t START_PULSE_MAX = 500;
constexpr uint32_t START_SYMBOL_MIN = 2600;
constexpr uint32_t START_SYMBOL_MAX = 2700;
constexpr uint32_t DATA_SYMBOL_MIN = 1500;
constexpr uint32_t DATA_SYMBOL_MAX = 1600;

// transmitter timings [µs]
constexpr uint32_t PW_SHORT_US = 390;
constexpr uint32_t PW_LONG_US = 1160;
constexpr uint32_t PW_START_US = 2300;

// number of data bits
constexpr uint32_t N_BITS = 24;

// number of required symbols = 2 x (start + N_BITS) = 50
constexpr uint32_t N_SYMBOLS_REQ = 2u * (N_BITS + 1);

// number of bs codes within received frame
constexpr int32_t N_FRAME_CODES = 4;

// decoder finite-state-machine
enum class RxSt { START_PULSE, START_SYMBOL, PULSE, DATA_SYMBOL };

// The encode() member function reserves and fills a complete frame, to be send. The Brennenstuhl
// RC receivers demand a frame with a start-symbol followed by 4 repeated codes.
void BrennenstuhlProtocol::encode(RemoteTransmitData *dst, const BrennenstuhlData &data) {
  uint32_t code = data.code;
  dst->reserve((N_SYMBOLS_REQ * N_FRAME_CODES) + 1);
  for (int32_t kc = 0; kc != N_FRAME_CODES; kc++) {
    dst->item(PW_SHORT_US, PW_START_US);
    for (int32_t ic = (N_BITS - 1); ic != -1; ic--) {
      if ((code >> ic) & 1) {
        dst->item(PW_LONG_US, PW_SHORT_US);
      } else {
        dst->item(PW_SHORT_US, PW_LONG_US);
      }
    }
  }
}

// The decode() member function extracts Brennenstuhl codes from the received frame. Instead
// of validating the pulse width of the carriers and pauses individually, it is more accurate
// to validate the symbols (symbol=carrier+pause) The symbol pulsewidth is around 1550µs, but
// the pulse with of the carrier and the pauses vary greatly. Once the symbol pulsewidth is
// valid, a code bit becomes "1" if the carrier is longer then the pause and "0" else. A total
// frame consists of a start symbol and up to four codes. The decoder decodes all codes and
// returns the best code (the one with the most identical codes)
optional<BrennenstuhlData> BrennenstuhlProtocol::decode(RemoteReceiveData src) {
  uint32_t n_received = static_cast<uint32_t>(src.size());
  BrennenstuhlData data{
      .code = 0,
  };
  // suppress noisy frames, at least a complete bs_code should be available
  if (n_received > N_SYMBOLS_REQ) {
    uint32_t bs_codes[4] = {0, 0, 0, 0};  // internal codes
    int32_t bs_cnt = 0;                   // number of bs codes found within frame
    int32_t bs_idx = -1;                  // index to best bs code
    uint32_t bit_cnt = 0;                 // bit counter [0..23]
    uint32_t pw_pre = 0;                  // pulsewidth of previous carrier (abs value)
    RxSt fsm = RxSt::START_PULSE;
    for (uint32_t ic = 0; (ic != n_received) && (bs_cnt != N_FRAME_CODES); ic++) {
      uint32_t pw_cur = (uint32_t) (src[ic] < 0 ? -src[ic] : src[ic]);  // current pulsewidth
      uint32_t pw_sym = pw_cur + pw_pre;                                // symbol=pulse+pause
      switch (fsm) {
        case RxSt::START_PULSE: {  // check if start pulse is valid
          if ((src[ic] > 0) && (pw_cur >= START_PULSE_MIN) && (pw_cur <= START_PULSE_MAX)) {
            bs_codes[bs_cnt] = 0;
            bit_cnt = 0;
            pw_pre = pw_cur;
            fsm = RxSt::START_SYMBOL;
          }
          break;
        }
        case RxSt::START_SYMBOL: {  // check if start symbol is valid
          if ((src[ic] < 0) && (pw_sym >= START_SYMBOL_MIN) && (pw_sym <= START_SYMBOL_MAX)) {
            fsm = RxSt::PULSE;
          } else {
            fsm = RxSt::START_PULSE;
          }
          break;
        }
        case RxSt::PULSE: {  // just grab pulse, validation is done in DATA_SYMBOL state
          if (src[ic] > 0) {
            pw_pre = pw_cur;
            fsm = RxSt::DATA_SYMBOL;
          } else {
            fsm = RxSt::START_PULSE;
          }
          break;
        }
        case RxSt::DATA_SYMBOL: {  // check if data symbol is valid and append bit to data
          if ((src[ic] < 0) && (pw_sym >= DATA_SYMBOL_MIN) && (pw_sym <= DATA_SYMBOL_MAX)) {
            bs_codes[bs_cnt] <<= 1;
            bs_codes[bs_cnt] += (pw_cur < pw_pre) ? 1 : 0;
            if (++bit_cnt < N_BITS) {
              fsm = RxSt::PULSE;
            } else {
              bs_cnt++;                 // complete code found
              fsm = RxSt::START_PULSE;  // start over for further codes in frame
            }
          } else {
            fsm = RxSt::START_PULSE;  // decoding failed, start over for further codes
          }
          break;
        }
      }
    }
    if (bs_cnt > 0) {  // complete codes found, find best code in list now
      int32_t identical_max = 0;
      for (int32_t ic = 0; ic != bs_cnt; ic++) {
        int32_t identical_cnt = 0;
        for (int32_t jc = 0; jc != bs_cnt; jc++) {
          identical_cnt += (bs_codes[ic] == bs_codes[jc]) ? 1 : 0;
        }
        if (identical_cnt > identical_max) {
          identical_max = identical_cnt;
          bs_idx = ic;  // save index to best code
        }
      }
      if (bs_idx > -1) {
        data.code = bs_codes[bs_idx];
        return data;  // return best bs code of list
      }
    }
  }
  return {};
}

void BrennenstuhlProtocol::dump(const BrennenstuhlData &data) {
  ESP_LOGI(TAG, "Brennenstuhl: code=0x%06" PRIx32, data.code);
}

}  // namespace esphome::remote_base
