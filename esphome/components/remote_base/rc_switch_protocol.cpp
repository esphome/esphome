#include "rc_switch_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.rc_switch";

const RCSwitchBase RC_SWITCH_PROTOCOLS[9] = {RCSwitchBase(0, 0, 0, 0, 0, 0, false),
                                             RCSwitchBase(350, 10850, 350, 1050, 1050, 350, false),
                                             RCSwitchBase(650, 6500, 650, 1300, 1300, 650, false),
                                             RCSwitchBase(3000, 7100, 400, 1100, 900, 600, false),
                                             RCSwitchBase(380, 2280, 380, 1140, 1140, 380, false),
                                             RCSwitchBase(3000, 7000, 500, 1000, 1000, 500, false),
                                             RCSwitchBase(10350, 450, 450, 900, 900, 450, true),
                                             RCSwitchBase(300, 9300, 150, 900, 900, 150, false),
                                             RCSwitchBase(250, 2500, 250, 1250, 250, 250, false)};

RCSwitchBase::RCSwitchBase(uint32_t sync_high, uint32_t sync_low, uint32_t zero_high, uint32_t zero_low,
                           uint32_t one_high, uint32_t one_low, bool inverted)
    : sync_high_(sync_high),
      sync_low_(sync_low),
      zero_high_(zero_high),
      zero_low_(zero_low),
      one_high_(one_high),
      one_low_(one_low),
      inverted_(inverted) {}

void RCSwitchBase::one(RemoteTransmitData *dst) const {
  if (!this->inverted_) {
    dst->mark(this->one_high_);
    dst->space(this->one_low_);
  } else {
    dst->space(this->one_high_);
    dst->mark(this->one_low_);
  }
}
void RCSwitchBase::zero(RemoteTransmitData *dst) const {
  if (!this->inverted_) {
    dst->mark(this->zero_high_);
    dst->space(this->zero_low_);
  } else {
    dst->space(this->zero_high_);
    dst->mark(this->zero_low_);
  }
}
void RCSwitchBase::sync(RemoteTransmitData *dst) const {
  if (!this->inverted_) {
    dst->mark(this->sync_high_);
    dst->space(this->sync_low_);
  } else {
    dst->space(this->sync_high_);
    dst->mark(this->sync_low_);
  }
}
void RCSwitchBase::transmit(RemoteTransmitData *dst, uint64_t code, uint8_t len) const {
  dst->set_carrier_frequency(0);
  this->sync(dst);
  for (int16_t i = len - 1; i >= 0; i--) {
    if (code & ((uint64_t) 1 << i))
      this->one(dst);
    else
      this->zero(dst);
  }
}

bool RCSwitchBase::expect_one(RemoteReceiveData &src) const {
  if (!this->inverted_) {
    if (!src.peek_mark(this->one_high_))
      return false;
    if (!src.peek_space(this->one_low_, 1))
      return false;
  } else {
    if (!src.peek_space(this->one_high_))
      return false;
    if (!src.peek_mark(this->one_low_, 1))
      return false;
  }
  src.advance(2);
  return true;
}
bool RCSwitchBase::expect_zero(RemoteReceiveData &src) const {
  if (!this->inverted_) {
    if (!src.peek_mark(this->zero_high_))
      return false;
    if (!src.peek_space(this->zero_low_, 1))
      return false;
  } else {
    if (!src.peek_space(this->zero_high_))
      return false;
    if (!src.peek_mark(this->zero_low_, 1))
      return false;
  }
  src.advance(2);
  return true;
}
bool RCSwitchBase::expect_sync(RemoteReceiveData &src) const {
  if (!this->inverted_) {
    if (!src.peek_mark(this->sync_high_))
      return false;
    if (!src.peek_space(this->sync_low_, 1))
      return false;
  } else {
    // We cant peek a space at the beginning because signals starts with a low to high transition.
    // this long space at the beginning is the separation between the transmissions itself, so it is actually
    // added at the end kind of artificially (by the value given to "idle:" option by the user in the yaml)
    if (!src.peek_mark(this->sync_low_))
      return false;
    src.advance(1);
    return true;
  }
  src.advance(2);
  return true;
}
bool RCSwitchBase::decode(RemoteReceiveData &src, uint64_t *out_data, uint8_t *out_nbits) const {
  // ignore if sync doesn't exist
  this->expect_sync(src);

  *out_data = 0;
  for (*out_nbits = 0; *out_nbits < 64; *out_nbits += 1) {
    if (this->expect_zero(src)) {
      *out_data <<= 1;
      *out_data |= 0;
    } else if (this->expect_one(src)) {
      *out_data <<= 1;
      *out_data |= 1;
    } else {
      return *out_nbits >= 8;
    }
  }
  return true;
}
optional<RCSwitchData> RCSwitchBase::decode(RemoteReceiveData &src) const {
  RCSwitchData out;
  uint8_t out_nbits;
  for (uint8_t i = 1; i <= 8; i++) {
    src.reset();
    const RCSwitchBase *protocol = &RC_SWITCH_PROTOCOLS[i];
    if (protocol->decode(src, &out.code, &out_nbits) && out_nbits >= 3) {
      out.protocol = i;
      return out;
    }
  }
  return {};
}

void RCSwitchBase::simple_code_to_tristate(uint16_t code, uint8_t nbits, uint64_t *out_code) {
  *out_code = 0;
  for (int8_t i = nbits - 1; i >= 0; i--) {
    *out_code <<= 2;
    if (code & (1 << i))
      *out_code |= 0b01;
    else
      *out_code |= 0b00;
  }
}
void RCSwitchBase::type_a_code(uint8_t switch_group, uint8_t switch_device, bool state, uint64_t *out_code,
                               uint8_t *out_nbits) {
  uint16_t code = 0;
  code = switch_group ^ 0b11111;
  code <<= 5;
  code |= switch_device ^ 0b11111;
  code <<= 2;
  code |= state ? 0b01 : 0b10;
  simple_code_to_tristate(code, 12, out_code);
  *out_nbits = 24;
}
void RCSwitchBase::type_b_code(uint8_t address_code, uint8_t channel_code, bool state, uint64_t *out_code,
                               uint8_t *out_nbits) {
  uint16_t code = 0;
  code |= (address_code == 1) ? 0 : 0b1000;
  code |= (address_code == 2) ? 0 : 0b0100;
  code |= (address_code == 3) ? 0 : 0b0010;
  code |= (address_code == 4) ? 0 : 0b0001;
  code <<= 4;
  code |= (channel_code == 1) ? 0 : 0b1000;
  code |= (channel_code == 2) ? 0 : 0b0100;
  code |= (channel_code == 3) ? 0 : 0b0010;
  code |= (channel_code == 4) ? 0 : 0b0001;
  code <<= 4;
  code |= 0b1110;
  code |= state ? 0b1 : 0b0;
  simple_code_to_tristate(code, 12, out_code);
  *out_nbits = 24;
}
void RCSwitchBase::type_c_code(uint8_t family, uint8_t group, uint8_t device, bool state, uint64_t *out_code,
                               uint8_t *out_nbits) {
  uint16_t code = 0;
  code |= (family & 0b0001) ? 0b1000 : 0;
  code |= (family & 0b0010) ? 0b0100 : 0;
  code |= (family & 0b0100) ? 0b0010 : 0;
  code |= (family & 0b1000) ? 0b0001 : 0;
  code <<= 4;
  code |= ((device - 1) & 0b01) ? 0b1000 : 0;
  code |= ((device - 1) & 0b10) ? 0b0100 : 0;
  code |= ((group - 1) & 0b01) ? 0b0010 : 0;
  code |= ((group - 1) & 0b10) ? 0b0001 : 0;
  code <<= 4;
  code |= 0b0110;
  code |= state ? 0b1 : 0b0;
  simple_code_to_tristate(code, 12, out_code);
  *out_nbits = 24;
}
void RCSwitchBase::type_d_code(uint8_t group, uint8_t device, bool state, uint64_t *out_code, uint8_t *out_nbits) {
  *out_code = 0;
  *out_code |= (group == 0) ? 0b11000000 : 0b01000000;
  *out_code |= (group == 1) ? 0b00110000 : 0b00010000;
  *out_code |= (group == 2) ? 0b00001100 : 0b00000100;
  *out_code |= (group == 3) ? 0b00000011 : 0b00000001;
  *out_code <<= 6;
  *out_code |= (device == 1) ? 0b110000 : 0b010000;
  *out_code |= (device == 2) ? 0b001100 : 0b000100;
  *out_code |= (device == 3) ? 0b000011 : 0b000001;
  *out_code <<= 6;
  *out_code |= 0b000000;
  *out_code <<= 4;
  *out_code |= state ? 0b1100 : 0b0011;
  *out_nbits = 24;
}

uint64_t decode_binary_string(const std::string &data) {
  uint64_t ret = 0;
  for (char c : data) {
    ret <<= 1UL;
    ret |= (c != '0');
  }
  return ret;
}

uint64_t decode_binary_string_mask(const std::string &data) {
  uint64_t ret = 0;
  for (char c : data) {
    ret <<= 1UL;
    ret |= (c != 'x');
  }
  return ret;
}

bool RCSwitchRawReceiver::matches(RemoteReceiveData src) {
  uint64_t decoded_code;
  uint8_t decoded_nbits;
  if (!this->protocol_.decode(src, &decoded_code, &decoded_nbits))
    return false;

  return decoded_nbits == this->nbits_ && (decoded_code & this->mask_) == (this->code_ & this->mask_);
}
bool RCSwitchDumper::dump(RemoteReceiveData src) {
  for (uint8_t i = 1; i <= 8; i++) {
    src.reset();
    uint64_t out_data;
    uint8_t out_nbits;
    const RCSwitchBase *protocol = &RC_SWITCH_PROTOCOLS[i];
    if (protocol->decode(src, &out_data, &out_nbits) && out_nbits >= 3) {
      char buffer[65];
      for (uint8_t j = 0; j < out_nbits; j++)
        buffer[j] = (out_data & ((uint64_t) 1 << (out_nbits - j - 1))) ? '1' : '0';

      buffer[out_nbits] = '\0';
      ESP_LOGD(TAG, "Received RCSwitch Raw: protocol=%u data='%s'", i, buffer);

      // only send first decoded protocol
      return true;
    }
  }
  return false;
}

}  // namespace remote_base
}  // namespace esphome
