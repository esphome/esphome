#include "byronsx_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.byronsx";

static const uint32_t BIT_TIME_US = 333;
static const uint8_t NBITS_ADDRESS = 8;
static const uint8_t NBITS_COMMAND = 4;
static const uint8_t NBITS_START_BIT = 1;
static const uint8_t NBITS_DATA = NBITS_ADDRESS + NBITS_COMMAND /*+ NBITS_COMMAND*/;


/*
ByronSX Protocol
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

0;17;Byron;ID=c300;SWITCH=12;CMD=ON;
20;18;Byron;ID=c300;SWITCH=12;CMD=OFF;

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
packet captures online) or definitive information from Byron.

Splitting each packet in this way works well for me with esphome. Any
corrections or additional data samples would be gratefully received.

marshn

*/

void ByronSXProtocol::encode(RemoteTransmitData *dst, const ByronSXData &data) {

  uint32_t out_data = 0x0;

  ESP_LOGD(TAG, "Send ByronSX: address=%04x command=%03x", data.address, data.command);

  out_data = data.address;
  out_data <<= NBITS_COMMAND;
  out_data |= data.command;

  ESP_LOGV(TAG, "Send ByronSX: out_data %03x", out_data);

  // Initial Mark start bit
  dst->mark(1 * BIT_TIME_US);

  for (uint32_t mask = 1UL << (NBITS_DATA - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      //ESP_LOGI(TAG, "1");
      dst->space(2 * BIT_TIME_US);
      dst->mark(1 * BIT_TIME_US);
    } else {
      //ESP_LOGI(TAG, "0");
      dst->space(1 * BIT_TIME_US);
      dst->mark(2 * BIT_TIME_US);
    }
  }
  // final space at end of packet
  dst->space(17 * BIT_TIME_US);
}

optional<ByronSXData> ByronSXProtocol::decode(RemoteReceiveData src) {
  ByronSXData out{
      .address = 0,
      .command = 0,
  };
  
  if (src.size() != (NBITS_DATA + NBITS_START_BIT) * 2) {
    return {};
  }

  // Skip start bit
  if (!src.expect_mark(BIT_TIME_US)) {
    return {};
  }

  //ESP_LOGVV(TAG, "%3d: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", src.size(),
  //          src.peek(0), src.peek(1), src.peek(2), src.peek(3), src.peek(4), src.peek(5), src.peek(6), src.peek(7),
  //          src.peek(8), src.peek(9), src.peek(10), src.peek(11), src.peek(12), src.peek(13), src.peek(14),
  //          src.peek(15), src.peek(16), src.peek(17), src.peek(18), src.peek(19));
  
  //ESP_LOGVV(TAG, "     %d %d %d %d %d %d", 
  //          src.peek(20), src.peek(21), src.peek(22), src.peek(23), src.peek(24), src.peek(25));


  // Read data bits
  uint32_t out_data = 0;
  int8_t bit = NBITS_DATA;
  while (--bit >= 0) {
    if (src.expect_space(2 * BIT_TIME_US) && src.expect_mark(BIT_TIME_US)) {
      out_data |= 1 << bit;
    } else if (src.expect_space(BIT_TIME_US) && src.expect_mark(2 * BIT_TIME_US)) {
      out_data |= 0 << bit;
    } else {
      ESP_LOGV(TAG, "Decode ByronSX: Fail 2, %2d %08x", bit, out_data);
      return {};
    }
    //ESP_LOGVV(TAG, "Decode ByronSX: Data, %2d %08x", bit, out_data);
  }

  // last bit followed by a long space
  if ( !src.peek_space_at_least(BIT_TIME_US)) {
    ESP_LOGV(TAG, "Decode ByronSX: Fail 4 ");
    return {};
  }

  out.command = (uint8_t) (out_data & 0xF);
  out_data >>= NBITS_COMMAND;
  out.address = (uint16_t) (out_data & 0xFF);

  return out;
}
void ByronSXProtocol::dump(const ByronSXData &data) {
//  ESP_LOGD(TAG, "Received ByronSX: address=0x%04X (0x%04x), command=0x%03x", data.address,
//           ((data.address << 1) & 0xffff), data.command);
  ESP_LOGD(TAG, "Received ByronSX: address=0x%08X, command=0x%02x", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
