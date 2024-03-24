// This component was developed using knowledge gathered by a number
// of people who reverse-engineered the Shelly 3EM:
//
// @AndreKR on GitHub
// Axel (@Axel830 on GitHub)
// Marko (@goodkiller on GitHub)
// Michaël Piron (@michaelpiron on GitHub)
// Theo Arends (@arendst on GitHub)

#include "ade7880.h"

namespace esphome {
namespace ade7880 {

// adapted from https://stackoverflow.com/a/55912127/1886371
template<size_t Bits, typename T> inline T sign_extend(const T &v) noexcept {
  using S = struct { signed Val : Bits; };
  return reinterpret_cast<const S *>(&v)->Val;
}

// Register types
// unsigned 8-bit (uint8_t)
// signed 10-bit - 16-bit ZP on wire (int16_t, needs sign extension)
// unsigned 16-bit (uint16_t)
// unsigned 20-bit - 32-bit ZP on wire (uint32_t)
// signed 24-bit - 32-bit ZPSE on wire (int32_t, needs sign extension)
// signed 24-bit - 32-bit ZP on wire (int32_t, needs sign extension)
// signed 24-bit - 32-bit SE on wire (int32_t)
// signed 28-bit - 32-bit ZP on wire (int32_t, needs sign extension)
// unsigned 32-bit (uint32_t)
// signed 32-bit (int32_t)

uint8_t ADE7880::read_u8_register16_(uint16_t a_register) {
  uint8_t in;
  this->read_register16(a_register, &in, sizeof(in));
  return in;
}

int16_t ADE7880::read_s16_register16_(uint16_t a_register) {
  int16_t in;
  this->read_register16(a_register, reinterpret_cast<uint8_t *>(&in), sizeof(in));
  return convert_big_endian(in);
}

uint16_t ADE7880::read_u16_register16_(uint16_t a_register) {
  uint16_t in;
  this->read_register16(a_register, reinterpret_cast<uint8_t *>(&in), sizeof(in));
  return convert_big_endian(in);
}

int32_t ADE7880::read_s24zp_register16_(uint16_t a_register) {
  // s24zp means 24 bit signed value in the lower 24 bits of a 32-bit register
  int32_t in;
  this->read_register16(a_register, reinterpret_cast<uint8_t *>(&in), sizeof(in));
  return sign_extend<24>(convert_big_endian(in));
}

int32_t ADE7880::read_s32_register16_(uint16_t a_register) {
  int32_t in;
  this->read_register16(a_register, reinterpret_cast<uint8_t *>(&in), sizeof(in));
  return convert_big_endian(in);
}

uint32_t ADE7880::read_u32_register16_(uint16_t a_register) {
  uint32_t in;
  this->read_register16(a_register, reinterpret_cast<uint8_t *>(&in), sizeof(in));
  return convert_big_endian(in);
}

void ADE7880::write_u8_register16_(uint16_t a_register, uint8_t value) {
  this->write_register16(a_register, &value, sizeof(value));
}

void ADE7880::write_s10zp_register16_(uint16_t a_register, int16_t value) {
  int16_t out = convert_big_endian(value & 0x03FF);
  this->write_register16(a_register, reinterpret_cast<uint8_t *>(&out), sizeof(out));
}

void ADE7880::write_u16_register16_(uint16_t a_register, uint16_t value) {
  uint16_t out = convert_big_endian(value);
  this->write_register16(a_register, reinterpret_cast<uint8_t *>(&out), sizeof(out));
}

void ADE7880::write_s24zpse_register16_(uint16_t a_register, int32_t value) {
  // s24zpse means a 24-bit signed value, sign-extended to 28 bits, in the lower 28 bits of a 32-bit register
  int32_t out = convert_big_endian(value & 0x0FFFFFFF);
  this->write_register16(a_register, reinterpret_cast<uint8_t *>(&out), sizeof(out));
}

void ADE7880::write_s32_register16_(uint16_t a_register, int32_t value) {
  int32_t out = convert_big_endian(value);
  this->write_register16(a_register, reinterpret_cast<uint8_t *>(&out), sizeof(out));
}

void ADE7880::write_u32_register16_(uint16_t a_register, uint32_t value) {
  uint32_t out = convert_big_endian(value);
  this->write_register16(a_register, reinterpret_cast<uint8_t *>(&out), sizeof(out));
}

}  // namespace ade7880
}  // namespace esphome
