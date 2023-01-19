#ifdef USE_ESP32

#include <sstream>
#include <cstring>
#include <mbedtls/aes.h>
#include "crypto.h"

namespace esphome {
namespace motion_blinds {

static const char *const KEY = "a3q8r8c135sqbn66";
static const uint8_t BLOCK_SIZE = 16;

static void pad_pkcs7(std::vector<uint8_t> &data);
static void unpad_pkcs7(std::vector<uint8_t> &data);
static std::string to_hex(const std::vector<uint8_t> &data);
static std::vector<uint8_t> from_hex(const std::string &str);

void Crypto::encrypt(const std::string &data, MotionBlindsMessage &message) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  auto input = from_hex(data);
  pad_pkcs7(input);
  message.length = input.size();

  mbedtls_aes_setkey_enc(&aes, (const unsigned char *) KEY, BLOCK_SIZE * 8);
  for (size_t i = 0; i < input.size(); i += BLOCK_SIZE) {
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, (const unsigned char *) input.data() + i, message.bytes + i);
  }
  mbedtls_aes_free(&aes);
}

std::string Crypto::decrypt(const uint8_t *data, size_t size) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  auto buffer = std::vector<uint8_t>(size, 0);

  mbedtls_aes_setkey_dec(&aes, (const unsigned char *) KEY, BLOCK_SIZE * 8);
  for (size_t i = 0; i < size; i += BLOCK_SIZE) {
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char *) data + i, buffer.data() + i);
  }
  mbedtls_aes_free(&aes);

  unpad_pkcs7(buffer);
  return to_hex(buffer);
}

void pad_pkcs7(std::vector<uint8_t> &data) {
  const uint8_t padding = BLOCK_SIZE - (data.size() % BLOCK_SIZE);
  data.insert(data.end(), padding, padding);
}

void unpad_pkcs7(std::vector<uint8_t> &data) {
  const uint8_t padding = data.back();
  data.erase(data.end() - padding, data.end());
}

std::string to_hex(const std::vector<uint8_t> &data) {
  std::stringstream stream;
  for (const auto &character : data) {
    char buffer[3] = {0x0, 0x0, 0x0};
    itoa(character, buffer, 16);
    if (strlen(buffer) < 2) {
      stream << "0";
    }
    stream << buffer;
  }
  return stream.str();
}

std::vector<uint8_t> from_hex(const std::string &str) {
  std::vector<uint8_t> data;
  data.reserve(str.length() / 2);
  for (size_t i = 0; i < str.length(); i += 2) {
    auto byte = str.substr(i, 2);
    auto val = (uint8_t) strtol(byte.c_str(), nullptr, 16);
    data.push_back(val);
  }
  return data;
}

}  // namespace motion_blinds
}  // namespace esphome

#endif  // USE_ESP32
