#include "xiaomi_ble.h"
#include "esphome/core/log.h"
#include "mbedtls/ccm.h"
#include "mbedtls/error.h"

#ifdef ARDUINO_ARCH_ESP32

#define CCM_ENCRYPT 0
#define CCM_DECRYPT 1
#define MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED 23334 //something
#define UPDATE_CBC_MAC                                                       \
  for (i = 0; i < 16; i++)                                                   \
    y[i] ^= b[i];                                                            \
                                                                             \
  if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, y, 16, y, &olen)) != 0) \
    return (ret);

#define CTR_CRYPT(dst, src, len)                            \
  do                                                        \
  {                                                         \
    if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctr, \
                                     16, b, &olen)) != 0)   \
    {                                                       \
      return (ret);                                         \
    }                                                       \
                                                            \
    for (i = 0; i < (len); i++)                             \
      (dst)[i] = (src)[i] ^ b[i];                           \
  } while (0)

static void mbedtls_zeroize( void *v, size_t n ) {
    volatile unsigned char *p = (unsigned char*)v; while( n-- ) *p++ = 0;
}

static int ccm_auth_crypt(mbedtls_ccm_context *ctx, int mode, size_t length,
                          const unsigned char *iv, size_t iv_len,
                          const unsigned char *add, size_t add_len,
                          const unsigned char *input, unsigned char *output,
                          unsigned char *tag, size_t tag_len, const unsigned char* aad=NULL, size_t aad_len=0)
{
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  unsigned char i;
  unsigned char q;
  size_t len_left, olen;
  unsigned char b[16];
  unsigned char y[16];
  unsigned char ctr[16];
  const unsigned char *src;
  unsigned char *dst;

  /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     *
     * Also, loosen the requirements to enable support for CCM* (IEEE 802.15.4).
     */
  if (tag_len == 2 || tag_len > 16 || tag_len % 2 != 0)
    return (MBEDTLS_ERR_CCM_BAD_INPUT);

  /* Also implies q is within bounds */
  if (iv_len < 7 || iv_len > 13)
    return (MBEDTLS_ERR_CCM_BAD_INPUT);

  if (add_len > 0xFF00)
    return (MBEDTLS_ERR_CCM_BAD_INPUT);

  q = 16 - 1 - (unsigned char)iv_len;

  /*
     * First block B_0:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       length
     *
     * With flags as (bits):
     * 7        0
     * 6        add present?
     * 5 .. 3   (t - 2) / 2
     * 2 .. 0   q - 1
     */
  b[0] = 0;
  b[0] |= (add_len > 0) << 6;
  b[0] |= ((tag_len - 2) / 2) << 3;
  b[0] |= q - 1;

  memcpy(b + 1, iv, iv_len);

  for (i = 0, len_left = length; i < q; i++, len_left >>= 8)
    b[15 - i] = (unsigned char)(len_left & 0xFF);

  if (len_left > 0)
    return (MBEDTLS_ERR_CCM_BAD_INPUT);

  /* Start CBC-MAC with first block */
  memset(y, 0, 16);
  UPDATE_CBC_MAC;

  /*
     * If there is additional data, update CBC-MAC with
     * add_len, add, 0 (padding to a block boundary)
     */
  if (add_len > 0)
  {
    size_t use_len;
    len_left = add_len;
    src = add;

    memset(b, 0, 16);
    b[0] = (unsigned char)((add_len >> 8) & 0xFF);
    b[1] = (unsigned char)((add_len)&0xFF);

    use_len = len_left < 16 - 2 ? len_left : 16 - 2;
    memcpy(b + 2, src, use_len);
    len_left -= use_len;
    src += use_len;

    UPDATE_CBC_MAC;

    while (len_left > 0)
    {
      use_len = len_left > 16 ? 16 : len_left;

      memset(b, 0, 16);
      memcpy(b, src, use_len);
      UPDATE_CBC_MAC;

      len_left -= use_len;
      src += use_len;
    }
  }

  if (NULL != aad && aad_len > 0)
  {
    mbedtls_cipher_update_ad(&ctx->cipher_ctx, aad, aad_len);
  }

  /*
     * Prepare counter block for encryption:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       counter (initially 1)
     *
     * With flags as (bits):
     * 7 .. 3   0
     * 2 .. 0   q - 1
     */
  ctr[0] = q - 1;
  memcpy(ctr + 1, iv, iv_len);
  memset(ctr + 1 + iv_len, 0, q);
  ctr[15] = 1;

  /*
     * Authenticate and {en,de}crypt the message.
     *
     * The only difference between encryption and decryption is
     * the respective order of authentication and {en,de}cryption.
     */
  len_left = length;
  src = input;
  dst = output;

  while (len_left > 0)
  {
    size_t use_len = len_left > 16 ? 16 : len_left;

    if (mode == CCM_ENCRYPT)
    {
      memset(b, 0, 16);
      memcpy(b, src, use_len);
      UPDATE_CBC_MAC;
    }

    CTR_CRYPT(dst, src, use_len);

    if (mode == CCM_DECRYPT)
    {
      memset(b, 0, 16);
      memcpy(b, dst, use_len);
      UPDATE_CBC_MAC;
    }

    dst += use_len;
    src += use_len;
    len_left -= use_len;

    /*
         * Increment counter.
         * No need to check for overflow thanks to the length check above.
         */
    for (i = 0; i < q; i++)
      if (++ctr[15 - i] != 0)
        break;
  }

  /*
     * Authentication: reset counter and crypt/mask internal tag
     */
  for (i = 0; i < q; i++)
    ctr[15 - i] = 0;

  CTR_CRYPT(y, y, 16);
  int diff = 0;
  if (mode == CCM_DECRYPT)
  {
    /* Check tag in "constant-time" */
    for (i = 0; i < tag_len; i++)
    {
      diff |= tag[i] ^ y[i];
    }
  }

  if (diff != 0)
  {
    mbedtls_zeroize(output, length);
    return (MBEDTLS_ERR_CCM_AUTH_FAILED);
  }

  return (0);
}

namespace esphome
{
namespace xiaomi_ble
{
std::string hexencode_string(const std::string &raw_data)
{
  return hexencode(reinterpret_cast<const uint8_t *>(raw_data.c_str()), raw_data.size());
}
static const char *TAG = "xiaomi_ble";

bool parse_xiaomi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, XiaomiParseResult &result)
{
  switch (data_type)
  {
  case 0x0D:
  { // temperature+humidity, 4 bytes, 16-bit signed integer (LE) each, 0.1 °C, 0.1 %
    if (data_length != 4)
      return false;
    const int16_t temperature = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    const int16_t humidity = uint16_t(data[2]) | (uint16_t(data[3]) << 8);
    result.temperature = temperature / 10.0f;
    result.humidity = humidity / 10.0f;
    return true;
  }
  case 0x0A:
  { // battery, 1 byte, 8-bit unsigned integer, 1 %
    if (data_length != 1)
      return false;
    result.battery_level = data[0];
    return true;
  }
  case 0x06:
  { // humidity, 2 bytes, 16-bit signed integer (LE), 0.1 %
    if (data_length != 2)
      return false;
    const int16_t humidity = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    result.humidity = humidity / 10.0f;
    return true;
  }
  case 0x04:
  { // temperature, 2 bytes, 16-bit signed integer (LE), 0.1 °C
    if (data_length != 2)
      return false;
    const int16_t temperature = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    result.temperature = temperature / 10.0f;
    return true;
  }
  case 0x09:
  { // conductivity, 2 bytes, 16-bit unsigned integer (LE), 1 µS/cm
    if (data_length != 2)
      return false;
    const uint16_t conductivity = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    result.conductivity = conductivity;
    return true;
  }
  case 0x07:
  { // illuminance, 3 bytes, 24-bit unsigned integer (LE), 1 lx
    if (data_length != 3)
      return false;
    const uint32_t illuminance = uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16);
    result.illuminance = illuminance;
    return true;
  }
  case 0x08:
  { // soil moisture, 1 byte, 8-bit unsigned integer, 1 %
    if (data_length != 1)
      return false;
    result.moisture = data[0];
    return true;
  }
  default:
    return false;
  }
}

optional<XiaomiParseResult> parse_xiaomi_header(const esp32_ble_tracker::ESPBTDevice &device)
{
  ESP_LOGVV(TAG, "Parsing XIAOMI");
  if (!device.get_service_data_uuid().has_value())
  {
    ESP_LOGVV(TAG, "Xiaomi no service/uuid data");
    return {};
  }

  if (!device.get_service_data_uuid()->contains(0x95, 0xFE))
  {
    ESP_LOGVV(TAG, "No Xiaomi UUID magic bytes");
    return {};
  }

  const auto *raw = reinterpret_cast<const uint8_t *>(device.get_service_data().data());
  ESP_LOGVV(TAG, "payload: %s", hexencode_string(std::string(reinterpret_cast<const char *>(device.get_service_data().data()), device.get_service_data().size())).c_str());

  if (device.get_service_data().size() < 4)
    return {}; //Nothing to see here
               /* Original
  bool is_lywsdcgq = (raw[1] & 0x20) == 0x20 && raw[2] == 0xAA && raw[3] == 0x01;
  bool is_hhccjcy01 = (raw[1] & 0x20) == 0x20 && raw[2] == 0x98 && raw[3] == 0x00;
  bool is_lywsd02 = (raw[1] & 0x20) == 0x20 && raw[2] == 0x5b && raw[3] == 0x04;
  bool is_lywsd03 = (raw[1] == 0x58) && raw[2] == 0x5b && raw[3] == 0x05;
  bool is_cgg1 = (raw[1] & 0x30) == 0x30 && raw[2] == 0x47 && raw[3] == 0x03;


Now switch and capabilities. Looking through other examples, it looks like the 'fourth' byte is the type of sensor (this would allow for 255 sensors). 
The first two bytes describe the capabilities of the sensor and the particular message
*/

  XiaomiParseResult result; //HERE - isn't this just wrong?
  switch (raw[3])
  {
  case 0x00:
    result.type = XiaomiParseResult::TYPE_HHCCJCY01;
    break;
  case 0x01:
    result.type = XiaomiParseResult::TYPE_LYWSDCGQ;
    break;
  //case 0x02:
  //Dunno!
  case 0x03:
    result.type = XiaomiParseResult::TYPE_CGG1;
    break;
  case 0x04:
    result.type = XiaomiParseResult::TYPE_LYWSD02;
    break;
  case 0x05:
    result.type = XiaomiParseResult::TYPE_LYWSD03;
    ESP_LOGVV(TAG, "Xiaomi Received 003");
    break;
  default:
    ESP_LOGD(TAG, "Xiaomi unkown type received %ul", raw[3]);
    return {};
    break;
    ;
  }

  char capabilities = raw[1];
  if (capabilities & 0x40)
    result.has_data = true;
  if (capabilities & 0x20)
    result.has_capability = true;
  if (capabilities & 0x08)
    result.has_encryption = true;

  result.data_length = device.get_service_data().size();
  return result;
}

void decrypt_message(const esp32_ble_tracker::ESPBTDevice &device, xiaomi_ble::XiaomiParseResult &result, const uint8_t *bindkey, uint8_t *message)
{

  memset(message, 0, sizeof((const unsigned char *)message));
  uint8_t token[4], payload_counter[3], nonce[12];
  char xiaomi[2]= { 0x95,0xfe };
  long long ret = 0;
  uint8_t aad[8] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
  size_t aadlen = 1;
  int enc_length = result.data_length - 11 - 7; //magic FTW (well actually it is the position of the MAC address plus the length from the start [11], then token and payload from the end [7])
  if (enc_length < 1)
    return;
  uint8_t *encrypted_payload = new uint8_t[enc_length];
  strncpy((char *)&token, &(device.get_service_data().data()[result.data_length - 4]), result.data_length);
  strncpy((char *)&payload_counter, &(device.get_service_data().data()[result.data_length - 7]), 3);
  int pos = device.get_service_data().data() - strstr(device.get_service_data().data(), xiaomi);
  strncpy((char *)nonce, device.get_service_data().data() + 5, 6);
  strncpy((char *)nonce + 6, device.get_service_data().data() + 2, 3);
  strncpy((char *)nonce + 9, (char*)payload_counter, 3);
  strncpy((char *)encrypted_payload, device.get_service_data().data() + 11, enc_length);

  ESP_LOGVV(TAG, "Data: %s", hexencode_string(std::string(reinterpret_cast<const char *>(device.get_service_data().data()), result.data_length)).c_str());
  ESP_LOGVV(TAG, "Token: %s", hexencode_string(std::string(reinterpret_cast<const char *>(token), 4)).c_str());
  ESP_LOGVV(TAG, "payload_counter: %s", hexencode_string(std::string(reinterpret_cast<const char *>(payload_counter), 3)).c_str());
  ESP_LOGVV(TAG, "Nonce: %s", hexencode_string(std::string(reinterpret_cast<const char *>(nonce), 12)).c_str());
  ESP_LOGVV(TAG, "encrypted_payload: %s", hexencode_string(std::string(reinterpret_cast<const char *>(encrypted_payload), enc_length)).c_str());
  ESP_LOGVV(TAG, "bindkey: %s", hexencode_string(std::string(reinterpret_cast<const char *>(bindkey), 16)).c_str());


  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);
  ret = mbedtls_ccm_setkey(&ctx,MBEDTLS_CIPHER_ID_AES , bindkey, 16 * 8);
  if (ret != 0)
  {
    char error[200];
    ESP_LOGVV(TAG, "setkey failure failure: %lli", ret);
    mbedtls_strerror(ret, error, 199);
    ESP_LOGVV(TAG, "setkey failure: %s", error);
  }
  //unsigned char iv[14];
  //memset( iv, 0, sizeof( iv ) );
  unsigned char check_tag[16];
  size_t tag_len = 4;

  ret = ccm_auth_crypt(&ctx, CCM_DECRYPT, 5 /* length*/,
                       nonce, 12 /*iv_len*/, aad /*add*/, aadlen /* add_len*/,
                       encrypted_payload /*input*/, message /* output*/, token, tag_len);
  

  if (ret != 0)
  {
    char error[200];
    ESP_LOGVV(TAG, "Decrypt failure failure: %lli", ret);
    mbedtls_strerror(ret, error, 199);
    ESP_LOGVV(TAG, "Decrypt failure: %s", error);
  }
  mbedtls_ccm_free(&ctx);
//  ESP_LOGCONFIG(TAG, "decrypted message: %s", hexencode_string(std::string(reinterpret_cast<const char *>(message), 5)).c_str());
//  ESP_LOGCONFIG(TAG, "decrypted token: %s", hexencode_string(std::string(reinterpret_cast<const char *>(check_tag), 6)).c_str());

  delete (encrypted_payload);
}
//void parse_xiaomi_message(const esp32_ble_tracker::ESPBTDevice &device,XiaomiParseResult &result) {
void parse_xiaomi_message(const uint8_t *message, XiaomiParseResult &result)
{

  // const auto *raw = reinterpret_cast<const uint8_t *>(device.get_service_data().data());

  uint8_t raw_offset; //
  switch (result.type)
  {
  case XiaomiParseResult::TYPE_LYWSDCGQ:
  case XiaomiParseResult::TYPE_CGG1:
    raw_offset = 11;
    break;
  case XiaomiParseResult::TYPE_LYWSD03:
    raw_offset = 0;
    break;
  default:
    raw_offset = 12;
    break;
  };

  //= (result.type == XiaomiParseResult::TYPE_LYWSDCGQ || result.type == XiaomiParseResult::TYPE_CGG1) ? 11 : 12; //Not sure this is 100% anyway
  uint8_t data_length = result.data_length - raw_offset;
  const uint8_t *raw_data = &message[raw_offset];
  uint8_t data_offset = 0;
  bool success = false;
  // Data point specs
  // Byte 0: type
  // Byte 1: fixed 0x10
  // Byte 2: length
  // Byte 3..3+len-1: data point value

  while (true)
  {
    if (data_length < 4)
      // at least 4 bytes required
      // type, fixed 0x10, length, 1 byte value
      break;

    const uint8_t datapoint_type = raw_data[data_offset + 0];
    const uint8_t datapoint_length = raw_data[data_offset + 2];

    if (data_length < 3 + datapoint_length)
      // 3 fixed bytes plus value length
      break;

    const uint8_t *datapoint_data = &raw_data[data_offset + 3];

    if (parse_xiaomi_data_byte(datapoint_type, datapoint_data, datapoint_length, result))
      success = true;

    data_length -= data_offset + 3 + datapoint_length;
    data_offset += 3 + datapoint_length;
  }

  return;
}

bool XiaomiListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device)
{
  //HERE - This smells like code duplication.
  auto res = parse_xiaomi_header(device);
  if (!res.has_value())
    return false;
  // No encryption values by default
  const char *name;
  switch (res->type)
  {
  case XiaomiParseResult::TYPE_HHCCJCY01:
    name = "HHCCJCY01";
    break;
  case XiaomiParseResult::TYPE_LYWSDCGQ:
    name = "LYWSDCGQ";
    break;
  case XiaomiParseResult::TYPE_LYWSD02:
    name = "LYWSD02";
    break;
  case XiaomiParseResult::TYPE_LYWSD03:
    name = "LYWSD03";
    break;
  case XiaomiParseResult::TYPE_CGG1:
    name = "CGG1";
    break;
  default:
    name = "HHCCJCY01";
  }

  ESP_LOGV(TAG, "Got Xiaomi %s (%s):", name, device.address_str().c_str());

  if (res->temperature.has_value())
  {
    ESP_LOGV(TAG, "  Temperature: %.1f°C", *res->temperature);
  }
  if (res->humidity.has_value())
  {
    ESP_LOGV(TAG, "  Humidity: %.1f%%", *res->humidity);
  }
  if (res->battery_level.has_value())
  {
    ESP_LOGV(TAG, "  Battery Level: %.0f%%", *res->battery_level);
  }
  if (res->conductivity.has_value())
  {
    ESP_LOGV(TAG, "  Conductivity: %.0fµS/cm", *res->conductivity);
  }
  if (res->illuminance.has_value())
  {
    ESP_LOGV(TAG, "  Illuminance: %.0flx", *res->illuminance);
  }
  if (res->moisture.has_value())
  {
    ESP_LOGV(TAG, "  Moisture: %.0f%%", *res->moisture);
  }

  return true;
}

} // namespace xiaomi_ble
} // namespace esphome

#endif
