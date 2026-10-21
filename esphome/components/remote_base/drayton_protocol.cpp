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

28 bits received 0110000110000000000001010010 (bin) or 6180052 (hex)

--------------------

The 28 bits consist of 20 bits of payload and 8 bits of checksum.

In order of transmission the payload consists of the following fields:

Bits Description
12   ID of the thermostat. A Digistat receiver will only respond to a packet if its ID
     matches the ID that the receiver has previously learnt.
4    Pairing field, normally 0000. The first packets sent by the thermostat after its
     batteries are inserted have this field set to 0001, which lets a Digistat receiver
     in pairing mode learn the ID of the thermostat.
1    Thermostat on (1) or off (0).
1    Always 0.
1    Low battery (1) or battery OK (0).
1    Always 0.

The YAML/lambda 'address' is the first 16 bits as sent (the 12 bit ID followed by the
pairing field, so the pairing flag is the address LSB), and 'data' is the next 4 bits
(0x8 = on, 0x2 = low battery).

Checksum: see calc_cs(); the +1 when the sum is above 0x7F is empirical. The captured
packets above give 0x32 (On) and 0x52 (Off). Packets that fail it are logged at verbose level.

Legacy split (from RFLink): address (16), command (7: data + top 3 checksum bits), channel (5: low checksum bits).
When channel and command are set they are sent as is, with no checksum calculated.

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
    out_data = data.address;
    out_data <<= NBITS_COMMAND;
    out_data |= data.command;
    out_data <<= NBITS_CHANNEL;
    out_data |= data.channel;
  } else {
    out_data = data.address << (NBITS_DATA + NBITS_CHECKSUM);
    out_data |= (data.data & ((1u << NBITS_DATA) - 1)) << NBITS_CHECKSUM;  // a lambda may exceed 4 bits
    out_data |= calc_cs(out_data);
  }
  ESP_LOGD(TAG, "addr=%04x data=%x channel=%02x command=%02x", data.address, data.data, data.channel, data.command);
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
      ESP_LOGVV(TAG, "Data %2d %08" PRIx32, bit, out_data);
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
      ESP_LOGV(TAG, "Dropped %07" PRIx32 ": checksum 0x%02x, expected 0x%02x", out_data, cs, expected_cs);
      continue;
    }

    ESP_LOGV(TAG, "Data %07" PRIx32, out_data);

    out.data = (out_data >> NBITS_CHECKSUM) & ((1u << NBITS_DATA) - 1);
    out.address = out_data >> (NBITS_CHECKSUM + NBITS_DATA);
    out.channel = out_data & ((1u << NBITS_CHANNEL) - 1);
    out.command = (out_data >> NBITS_CHANNEL) & ((1u << NBITS_COMMAND) - 1);

    return out;
  }
  return {};
}

void DraytonProtocol::dump(const DraytonData &data) {
  ESP_LOGI(TAG, "Received Drayton: address=0x%04X, data=0x%X", data.address, data.data);
}

}  // namespace esphome::remote_base
