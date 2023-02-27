#include "drayton_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.drayton";

static const uint32_t BIT_TIME_US = 500;
static const uint8_t NBITS_PREAMBLE = 12;
static const uint8_t NBITS_SYNC = 4;
static const uint8_t NBITS_ADDRESS = 16;
static const uint8_t NBITS_CHANNEL = 5;
static const uint8_t NBITS_COMMAND = 7;
static const uint8_t NBITS = NBITS_ADDRESS + NBITS_CHANNEL + NBITS_COMMAND;

static const uint8_t CMD_ON  = 0x41;
static const uint8_t CMD_OFF = 0x02;

void DraytonProtocol::encode(RemoteTransmitData *dst, const DraytonData &data) {
  
  uint16_t khz = 2;
  ESP_LOGD(TAG, "Send Drayton: frequency=%dkHz", khz);
  dst->set_carrier_frequency(khz * 1000);

//dst->set_carrier_frequency(36000);

  // Preamble = 101010101010
  uint32_t out_data = 0x0AAA;
  for (uint16_t mask = 1UL << (NBITS_PREAMBLE - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      dst->mark(BIT_TIME_US);
    } else {
      dst->space(BIT_TIME_US);
    }
  }

  // Sync = 1100
  out_data = 0x000C;
  for (uint16_t mask = 1UL << (NBITS_SYNC - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      dst->mark(BIT_TIME_US);
    } else {
      dst->space(BIT_TIME_US);
    }
  }

  ESP_LOGD(TAG, "Send Drayton: address=%04x channel=%03x cmd=%02x", data.address, data.channel, data.command);

  out_data = data.address;
  out_data <<= NBITS_COMMAND;
  out_data |= data.command;
  out_data <<= NBITS_CHANNEL;
  out_data |= data.channel;

  ESP_LOGV(TAG, "Send Drayton: out_data %08x", out_data);
  
  for (uint32_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      dst->mark(BIT_TIME_US);
      dst->space(BIT_TIME_US);
    } else {
      dst->space(BIT_TIME_US);
      dst->mark(BIT_TIME_US);
    }
	
  }
}

optional<DraytonData> DraytonProtocol::decode(RemoteReceiveData src) {
  DraytonData out{
      .address = 0,
      .channel = 0,
	  .command = 0,
  };
  uint8_t field_bit;

  if(src.size() < 45) {
    return{};
  }

  ESP_LOGVV(TAG, "Decode Drayton: %d, %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", 
	src.size(), 
	src.peek(0), src.peek(1), src.peek(2), src.peek(3), src.peek(4), 
	src.peek(5), src.peek(6), src.peek(7), src.peek(8), src.peek(9), 
	src.peek(10), src.peek(11), src.peek(12), src.peek(13), src.peek(14), 
	src.peek(15), src.peek(16), src.peek(17), src.peek(18), src.peek(19));


  // If first preamble item is a space, skip it
  if (src.peek_space_at_least(1)) {
    src.advance(1);
  }

  // Look for sync pulse, after. If sucessful index points to space of sync symbol
  for (uint16_t preamble = 0; preamble <= NBITS_PREAMBLE * 2; preamble += 2) {
    ESP_LOGVV(TAG, "Decode Drayton: preamble %d  %d %d", preamble, src.peek(preamble), src.peek(preamble+1));
    if (src.peek_mark(2 * BIT_TIME_US, preamble) && (src.peek_space(2 * BIT_TIME_US, preamble + 1) || src.peek_space(3 * BIT_TIME_US, preamble + 1))) {
      src.advance(preamble + 1);
      break;
	}
  }
  ESP_LOGVV(TAG, "Decode Drayton: %d, %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", 
	src.get_index(), 
	src.peek(0), src.peek(1), src.peek(2), src.peek(3), src.peek(4), 
	src.peek(5), src.peek(6), src.peek(7), src.peek(8), src.peek(9), 
	src.peek(10), src.peek(11), src.peek(12), src.peek(13), src.peek(14), 
	src.peek(15), src.peek(16), src.peek(17), src.peek(18), src.peek(19));

  // Read data. Index points to space of sync symbol
  // Extract first bit
  // Checks next bit to leave index pointing correctly
  uint32_t out_data = 0;
  uint8_t bit = NBITS_ADDRESS + NBITS_COMMAND + NBITS_CHANNEL - 1;
  if (src.expect_space(3 * BIT_TIME_US) &&
      (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
    out_data |= 0 << bit;
  } else if (src.expect_space(2 * BIT_TIME_US) &&
        src.expect_mark(BIT_TIME_US) &&
	   (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
    out_data |= 1 << bit;
  } else {
    ESP_LOGV(TAG, "Decode Drayton: X2 - Err");
    return {};
  }

  // Before/after each bit is read the index points to the transition at the start of the bit period or,
  // if there is no transition at the start of the bit period, then the transition in the middle of 
  // the previous bit period.
  while(--bit >= 1) {
    ESP_LOGVV(TAG, "Decode Drayton: X3, %2d %08x", bit, out_data);
    if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
        (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
      out_data |= 0 << bit;
    } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
               (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
      out_data |= 1 << bit;
    } else {
        ESP_LOGV(TAG, "Decode Drayton: X4, %2d %08x", bit, out_data);
      return {};
    }
  }
  if (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) {
    out_data |= 0;
  } else if (src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) {
    out_data |= 1;
  }
  ESP_LOGVV(TAG, "Decode Drayton: X3, %2d %08x", bit, out_data);

  out.channel = (uint16_t)(out_data & 0x1F);
  out_data >>= NBITS_CHANNEL;
  out.command = (uint16_t)(out_data & 0x7F);
  out_data >>= NBITS_COMMAND;
  out.address = (uint16_t)(out_data & 0xFFFF);
	
  return out;
}
void DraytonProtocol::dump(const DraytonData &data) {
  ESP_LOGD(TAG, "Received Drayton: address=0x%04X (0x%04x), channel=0x%03x command=0x%03X", 
    data.address, ((data.address << 1) & 0xffff), data.channel, data.command);
}

}  // namespace remote_base
}  // namespace esphome
