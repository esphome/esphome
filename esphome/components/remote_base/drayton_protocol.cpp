#include "drayton_protocol.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.drayton");

static constexpr uint32_t BIT_TIME_US = 500;
static constexpr uint8_t CARRIER_KHZ = 2;
static constexpr uint8_t NBITS_PREAMBLE = 12;
static constexpr uint8_t NBITS_SYNC = 4;
static constexpr uint8_t NBITS_ADDRESS = 16;
static constexpr uint8_t NBITS_DATA = 4;
static constexpr uint8_t NBITS_CHECKSUM = 8;
static constexpr uint8_t NBITS_COMMAND = 7;
static constexpr uint8_t NBITS_CHANNEL = 5;
static constexpr uint8_t NBITS_PKT = NBITS_ADDRESS + NBITS_DATA + NBITS_CHECKSUM;
static constexpr uint8_t MIN_RX_SRC = (NBITS_PKT + NBITS_SYNC / 2);

/*
Drayton Protocol
Using an oscilloscope to capture the data transmitted by the Digistat two
distinct packets for 'On' and 'Off' are transmitted. Each transmitted bit
has a period of 500us, a bit rate of 2000 baud.

Each packet consists of an initial 1010 pattern to set up the receiver bias.
The number of these bits seen at the receiver varies depending on the state
of the bias when the packet transmission starts. The receiver algoritmn takes
account of this.

The packet is Manchester coded, with a '10' tranmitted pair representing
a '1' bit and a '01' pair representing a '0' bit. Each packet is begun
with a '1100' syncronisation symbol which breaks this rule. Following
the sync are 28 '01' or '10' pairs.

--------------------

Sample Boiler On Command as received:
101010101010110001101001010101101001010101010101100101010101101001011001
ppppppppppppSSSS-0-1-1-0-0-0-0-1-1-0-0-0-0-0-0-0-1-0-0-0-0-0-1-1-0-0-1-0

(Where pppp represents the preamble bits and SSSS represents the sync symbol)

28 bits received 01100001100000001000001 10010 (bin) or 6180832 (hex)

Sample Boiler Off Command as received:
101010101010110001101001010101101001010101010101010101010110011001011001
ppppppppppppSSSS-0-1-1-0-0-0-0-1-1-0-0-0-0-0-0-0-0-0-0-0-0-1-0-1-0-0-1-0

28 bits of data received 0110000110000000000001010010 (bin) or 6180052 (hex)

--------------------

The 28 bits consist of 20 bits of data and 8 bits of checksum.

In order of transmission the data consists of the following fields:

Bits Description
12   ID of the thermostat. LSB transmitted first. Range is 0 to 4095 (decimal).
     A digistat receiver will only respond to a packet if its ID/address matches the
     ID that the receiver has previously learnt.
4    This field is normally set to 0000. The first packets transmitted by thermostat
     after its batteries are inserted has this field set to 0001. This allows a
     Digistat receiver in 'learn' mode to synchronise to the ID of the thermostat.
     The 'learn' parameter allows this bit to be set so that a Drayton reciever can
     learn the ID of an esphome transmitter.
1    Thermostat on (1) or off (0). Can be set with the switch parameter.
1    Always 0.
1    Low battery (1) or battery OK (0). Can be set with the loBatt parameter.
1    Always 0.
8    Checksum

By inspection, observing how carry bits ripple through, the checksum is transmitted
LSB first.  The working assuption is that the ID is also transmitted LSB first.

The checksum is calculated by summing the 20 bits of the 'payload'.

CS    ID bit/             Payload offset (hex)
Bit   Function

0     None - always 0
1     on/off              0
2                        (1),(4)
3     1, Lo Batt          2,     (5), 8
4     2,16               (3),(6), 9,  C
5     4,32,256,Boot           7,  A,  D, 10
6     8,64,512                    B,  E, 11
7     128,1024                        F, 12
8     2048                               13

The first payload bit transmitted is at offset 0x13. Bit at offset 0 is sent last.

Any overflow in bits 8 or 9 wraps around into bit 1 of the sum.
Only bits 0-7 of the checksum are sent in the packet, bit 0 first.

The checksum is calculated and appended when a packet is transmitted.

When a packet is receieved the checksum is verified. Packets which fail the checksum
verification are discarded.

To maintain backward compatibility with the initial revision of this module the address
is reported without taking the endianness of the address nibbles.

The initial revision split the 28 bits into address (16), command (7) and channel (5),
following RFLink. That split does not match the fields above but is kept for existing
configs: when channel or command is set, the packet is built from them with no checksum.
They will be deprecated.

*/

static uint8_t calc_cs(uint32_t out_data) {
  uint16_t cs = 0;
  uint32_t rev = reverse_bits(out_data);

  for (uint8_t i = 4; i <= 20; i += 4) {
    cs <<= 1;
    cs += (rev >> i) & 0x0F;
  }

  if (cs > 0x7F) {
    cs += 1;
  }
  // Sent LSB first
  return reverse_bits(uint8_t(cs << 1));
}

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

  if (data.channel || data.command) {
    ESP_LOGD(TAG, "addr=%04" PRIx16 " channel=%03" PRIx8 " cmd=%02" PRIx8, data.address, data.channel, data.command);
    out_data = data.address;
    out_data <<= NBITS_COMMAND;
    out_data |= data.command;
    out_data <<= NBITS_CHANNEL;
    out_data |= data.channel;
  } else {
    ESP_LOGD(TAG, "addr=%04" PRIx16 " data=%01" PRIx8, data.address, data.data);
    out_data = data.address << (NBITS_DATA + NBITS_CHECKSUM);
    out_data |= data.data << NBITS_CHECKSUM;
    out_data |= calc_cs(out_data);
  }
  ESP_LOGV(TAG, "out_data %07" PRIx32, out_data);

  for (uint32_t mask = 1UL << (NBITS_PKT - 1); mask != 0; mask >>= 1) {
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
  DraytonData out{};

  while (src.size() - src.get_index() >= MIN_RX_SRC) {
    // If first preamble item is a space, skip it
    if (src.peek_space_at_least(1)) {
      src.advance(1);
    }

    // Look for sync pulse, after. If sucessful index points to space of sync symbol
    while (src.size() - src.get_index() >= MIN_RX_SRC) {
      ESP_LOGVV(TAG, "sync search %" PRIu32 ", %" PRId32 " %" PRId32, src.size() - src.get_index(), src.peek(),
                src.peek(1));
      if (src.peek_mark(2 * BIT_TIME_US) &&
          (src.peek_space(2 * BIT_TIME_US, 1) || src.peek_space(3 * BIT_TIME_US, 1))) {
        src.advance(1);
        ESP_LOGVV(TAG, "Found SYNC %" PRIu32, src.get_index());
        break;
      } else {
        src.advance(2);
      }
    }

    // No point continuing if not enough samples remaining to complete a packet
    if (src.size() - src.get_index() < NBITS_PKT) {
      ESP_LOGV(TAG, "Fail1 %" PRIu32, src.get_index());
      break;
    }

    // Read data. Index points to space of sync symbol
    // Extract first bit
    // Checks next bit to leave index pointing correctly
    uint32_t out_data = 0;
    uint8_t bit = NBITS_PKT - 1;
    if (src.expect_space(3 * BIT_TIME_US) && (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
      out_data |= 0 << bit;
    } else if (src.expect_space(2 * BIT_TIME_US) && src.expect_mark(BIT_TIME_US) &&
               (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
      out_data |= 1 << bit;
    } else {
      ESP_LOGVV(TAG, "Fail2 %" PRId32 " %" PRId32 " %" PRId32, src.peek(-1), src.peek(0), src.peek(1));
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
      ESP_LOGVV(TAG, "Fail3 %" PRId32 " %" PRId32 " %" PRId32, src.peek(-1), src.peek(0), src.peek(1));
      continue;
    }

    if (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) {
      out_data |= 0;
    } else if (src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) {
      out_data |= 1;
    } else {
      continue;
    }

    uint8_t cs = out_data & 0xFF;
    uint8_t expected_cs = calc_cs(out_data);

    if (cs != expected_cs) {
      ESP_LOGV(TAG, "Fail4 CS fail %" PRIx8 " %" PRIx8, cs, expected_cs);
      continue;
    }

    ESP_LOGV(TAG, "Data %07" PRIx32, out_data);

    out.data = (out_data >> NBITS_CHECKSUM) & 0x0F;
    out.address = out_data >> (NBITS_CHECKSUM + NBITS_DATA);
    out.channel = out_data & 0x1F;
    out.command = (out_data >> NBITS_CHANNEL) & 0x7F;

    return out;
  }
  return {};
}

void DraytonProtocol::dump(const DraytonData &data) {
  ESP_LOGI(TAG, "Received Drayton: address=0x%04X (0x%04x), data=0x%X channel=0x%03x command=0x%03X", data.address,
           ((data.address << 1) & 0xffff), data.data, data.channel, data.command);
}

}  // namespace esphome::remote_base
