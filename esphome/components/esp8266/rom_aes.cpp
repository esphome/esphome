#if defined(USE_ESP8266)

#include <cstdint>

namespace esphome::esp8266 {}

// The boot ROM's copy is AES-128 only and returns nothing, while the SDK's aes_encrypt_init()
// stores the return value as the round count, so wrap it in the SDK's signature.
static constexpr uint32_t ROM_RIJNDAEL_KEY_SETUP_ENC_ADDR = 0x40009300;

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" int __wrap_rijndaelKeySetupEnc(uint32_t rk[], const uint8_t cipher_key[], int key_bits) {
  if (key_bits != 128)
    return -1;
  using RomKeySetup = void (*)(uint32_t[], const uint8_t[]);
  reinterpret_cast<RomKeySetup>(ROM_RIJNDAEL_KEY_SETUP_ENC_ADDR)(rk, cipher_key);  // NOLINT(performance-no-int-to-ptr)
  return 10;
}

#endif  // USE_ESP8266
