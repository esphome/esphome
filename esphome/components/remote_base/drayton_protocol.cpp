#include "drayton_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.drayton";

static const uint32_t BIT_TIME_US = 500;
static const uint8_t CARRIER_KHZ = 2;
static const uint8_t NBITS_PREAMBLE = 12;
static const uint8_t NBITS_SYNC = 4;
static const uint8_t NBITS_ADDRESS = 16;
static const uint8_t NBITS_CHANNEL = 5;
static const uint8_t NBITS_COMMAND = 7;
static const uint8_t NDATABITS = NBITS_ADDRESS + NBITS_CHANNEL + NBITS_COMMAND;
static const uint8_t MIN_RX_SRC = (NDATABITS + NBITS_SYNC / 2);

static const uint8_t CMD_ON = 0x41;
static const uint8_t CMD_OFF = 0x02;

/*
Drayton Protocol
Using an oscilloscope to capture the data transmitted by the Digistat two
distinct packets for 'On' and 'Off' are transmitted. Each transmitted bit
has a period of 500us, a bit rate of 2000 baud.

Each packet consists of an initial 1010 pattern to set up the receiver bias.
The number of these bits seen at the receiver varies depending on the state
of the bias when the packet transmission starts. The receiver algoritmn takes
account of this.

The packet appears to be Manchester encoded, with a '10' tranmitted pair
representing a '1' bit and a '01' pair representing a '0' bit. Each packet is
begun with a '1100' syncronisation symbol which breaks this rule. Following
the sync are 28 '01' or '10' pairs.

--------------------

Boiler On Command as received:
101010101010110001101001010101101001010101010101100101010101101001011001
ppppppppppppSSSS-0-1-1-0-0-0-0-1-1-0-0-0-0-0-0-0-1-0-0-0-0-0-1-1-0-0-1-0

(Where pppp represents the preamble bits and SSSS represents the sync symbol)

28 bits of data received 01100001100000001000001 10010 (bin) or 6180832 (hex)

Boiler Off Command as received:
101010101010110001101001010101101001010101010101010101010110011001011001
ppppppppppppSSSS-0-1-1-0-0-0-0-1-1-0-0-0-0-0-0-0-0-0-0-0-0-1-0-1-0-0-1-0

28 bits of data received 0110000110000000000001010010 (bin) or 6180052 (hex)

--------------------

I have used 'RFLink' software (RLink Firmware Version: 1.1 Revision: 48) to
capture and retransmit the Digistat packets. RFLink splits each packet into an
ID, SWITCH, and CMD field.

0;17;Drayton;ID=c300;SWITCH=12;CMD=ON;
20;18;Drayton;ID=c300;SWITCH=12;CMD=OFF;

--------------------

Spliting my received data into three parts of 16, 7 and 5 bits gives address,
channel and Command values of:

On  6180832  0110000110000000 1000001 10010
address: '0x6180' channel: '0x12' command: '0x41'

Off 6180052  0110000110000000 0000010 10010
address: '0x6180' channel: '0x12' command: '0x02'

These values are slightly different to those used by RFLink (the RFLink
ID/Adress value is rotated/manipulated), and I don't know who's interpretation
is correct. A larger data sample would help (I have only found five different
packet captures online) or definitive information from Drayton.

Splitting each packet in this way works well for me with esphome. Any
corrections or additional data samples would be gratefully received.

marshn

*/

void DraytonProtocol::encode(RemoteTransmitData *dst, const DraytonData &data) {
  uint16_t khz = CARRIER_KHZ;
  dst->set_carrier_frequency(khz * 1000);

  // Preamble = 101010101010
  uint32_t out_data = 0x0AAA;
  for (uint32_t mask = 1UL << (NBITS_PREAMBLE - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      dst->mark(BIT_TIME_US);
    } else {
      dst->space(BIT_TIME_US);
    }
  }

  // Sync = 1100
  out_data = 0x000C;
  for (uint32_t mask = 1UL << (NBITS_SYNC - 1); mask != 0; mask >>= 1) {
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

  ESP_LOGV(TAG, "Send Drayton: out_data %08" PRIx32, out_data);

  for (uint32_t mask = 1UL << (NDATABITS - 1); mask != 0; mask >>= 1) {
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

  while (src.size() - src.get_index() >= MIN_RX_SRC) {
    ESP_LOGVV(TAG,
              "Decode Drayton: %" PRId32 ", %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
              " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
              " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "",
              src.size() - src.get_index(), src.peek(0), src.peek(1), src.peek(2), src.peek(3), src.peek(4),
              src.peek(5), src.peek(6), src.peek(7), src.peek(8), src.peek(9), src.peek(10), src.peek(11), src.peek(12),
              src.peek(13), src.peek(14), src.peek(15), src.peek(16), src.peek(17), src.peek(18), src.peek(19));

    // If first preamble item is a space, skip it
    if (src.peek_space_at_least(1)) {
      src.advance(1);
    }

    // Look for sync pulse, after. If sucessful index points to space of sync symbol
    while (src.size() - src.get_index() >= MIN_RX_SRC) {
      ESP_LOGVV(TAG, "Decode Drayton: sync search %" PRIu32 ", %" PRId32 " %" PRId32, src.size() - src.get_index(),
                src.peek(), src.peek(1));
      if (src.peek_mark(2 * BIT_TIME_US) &&
          (src.peek_space(2 * BIT_TIME_US, 1) || src.peek_space(3 * BIT_TIME_US, 1))) {
        src.advance(1);
        ESP_LOGVV(TAG, "Decode Drayton: Found SYNC, - %" PRIu32, src.get_index());
        break;
      } else {
        src.advance(2);
      }
    }

    // No point continuing if not enough samples remaining to complete a packet
    if (src.size() - src.get_index() < NDATABITS) {
      ESP_LOGV(TAG, "Decode Drayton: Fail 1, - %" PRIu32, src.get_index());
      break;
    }

    // Read data. Index points to space of sync symbol
    // Extract first bit
    // Checks next bit to leave index pointing correctly
    uint32_t out_data = 0;
    uint8_t bit = NDATABITS - 1;
    ESP_LOGVV(TAG, "Decode Drayton: first bit %" PRId32 "  %" PRId32 ", %" PRId32, src.peek(0), src.peek(1),
              src.peek(2));
    if (src.expect_space(3 * BIT_TIME_US) && (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
      out_data |= 0 << bit;
    } else if (src.expect_space(2 * BIT_TIME_US) && src.expect_mark(BIT_TIME_US) &&
               (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
      out_data |= 1 << bit;
    } else {
      ESP_LOGV(TAG, "Decode Drayton: Fail 2, - %" PRId32 " %" PRId32 " %" PRId32, src.peek(-1), src.peek(0),
               src.peek(1));
      continue;
    }

    // Before/after each bit is read the index points to the transition at the start of the bit period or,
    // if there is no transition at the start of the bit period, then the transition in the middle of
    // the previous bit period.
    while (--bit >= 1) {
      ESP_LOGVV(TAG, "Decode Drayton: Data, %2d %08" PRIx32, bit, out_data);
      if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
          (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
        out_data |= 0 << bit;
      } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
                 (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
        out_data |= 1 << bit;
      } else {
        break;
      }
    }

    if (bit > 0) {
      ESP_LOGVV(TAG, "Decode Drayton: Fail 3, %" PRId32 " %" PRId32 " %" PRId32, src.peek(-1), src.peek(0),
                src.peek(1));
      continue;
    }

    if (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) {
      out_data |= 0;
    } else if (src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) {
      out_data |= 1;
    } else {
      continue;
    }

    ESP_LOGV(TAG, "Decode Drayton: Data, %2d %08" PRIx32, bit, out_data);

    out.channel = (uint8_t) (out_data & 0x1F);
    out_data >>= NBITS_CHANNEL;
    out.command = (uint8_t) (out_data & 0x7F);
    out_data >>= NBITS_COMMAND;
    out.address = (uint16_t) (out_data & 0xFFFF);

    return out;
  }
  return {};
}
void DraytonProtocol::dump(const DraytonData &data) {
  ESP_LOGI(TAG, "Received Drayton: address=0x%04X (0x%04x), channel=0x%03x command=0x%03X", data.address,
           ((data.address << 1) & 0xffff), data.channel, data.command);
}

}  // namespace remote_base
}  // namespace esphome
