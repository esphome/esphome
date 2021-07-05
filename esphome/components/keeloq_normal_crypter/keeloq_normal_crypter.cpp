#include "esphome/core/log.h"
#include "keeloq_normal_crypter.h"

namespace esphome {
namespace keeloq_normal_crypter {

static const char *TAG = "keeloq_normal_crypter";

void KeeloqNormalCrypter::setup() {}

void KeeloqNormalCrypter::loop() {}

bool KeeloqNormalCrypter::decrypt(remote_base::KeeloqData &data){
    return this->decrypt(data, this->normal_keygen(data.serial));
}

bool KeeloqNormalCrypter::encrypt(remote_base::KeeloqData &data){
    if(!mkey_)
        return false;
    this->encrypt(data, this->normal_keygen(data.serial));
    return true;
}

void KeeloqNormalCrypter::dump_config(){
    ESP_LOGCONFIG(TAG, "Keeloq Normal Crypter: key %s", this->mkey_ == 0 ? "not set": "set");
}


#define KeeLoq_NLF              0x3A5C742E
#define bit_is_set(x,n)                (((x)>>(n))&1)
#define g5(x,a,b,c,d,e) (bit_is_set(x,a)+bit_is_set(x,b)*2+bit_is_set(x,c)*4+bit_is_set(x,d)*8+bit_is_set(x,e)*16)

uint32_t keeloq_encrypt(const uint32_t data, const uint64_t key) {
  uint32_t x = data, r;

  for (r = 0; r < 528; r++) {
    x = (x>>1)^((bit_is_set(x,0)^bit_is_set(x,16)^(uint32_t)bit_is_set(key,r&63)^bit_is_set(KeeLoq_NLF,g5(x,1,9,20,26,31)))<<31);
  }

  return x;
}

uint32_t keeloq_decrypt(const uint32_t data, const uint64_t key) {
  uint32_t x = data, r;

  for (r = 0; r < 528; r++) {
    x = (x<<1)^bit_is_set(x,31)^bit_is_set(x,15)^(uint32_t)bit_is_set(key,(15-r)&63)^bit_is_set(KeeLoq_NLF,g5(x,0,8,19,25,30));
  }

  return x;
}

uint64_t KeeloqNormalCrypter::normal_keygen(uint32_t serial) {
  static uint64_t key;
  static uint32_t cached = 0;

  // make sure the function code is masked out
  serial &= 0x0fffffff;

  if (serial == cached)
    return key;

  key = keeloq_decrypt(serial | 0x60000000, mkey_);
  key = key << 32 | keeloq_decrypt(serial | 0x20000000, mkey_);

  cached = serial;

  return key;
}

bool KeeloqNormalCrypter::decrypt(remote_base::KeeloqData &data, uint64_t key) {
  uint32_t clear = keeloq_decrypt(data.encrypted, key);
  uint16_t sync = clear & 0xFFFF;
  uint16_t chk = clear >> 16;

  // compare low 10 bits of Serial Number
  if ((chk & 0x3FF) != (data.serial & 0x3FF)) {
    ESP_LOGD(TAG, "Failed to verify discrimination bits, expected %04X, got %04X", data.serial & 0x3FF, chk & 0x3FF);
    return false;
  }

  // verify button code
  if (((chk >> 12) & 0x0F) != data.button) {
    ESP_LOGD(TAG, "Failed to verify button, expected %04X, got %04X", data.button, (chk >> 12) & 0x0F);
    return false;
  }

  data.sync = sync;
  ESP_LOGV(TAG, "Decrypted: Serial %07X Sync: %04X", data.serial, data.sync);
  return true;
}

void KeeloqNormalCrypter::encrypt(remote_base::KeeloqData &data, uint64_t key) {
  uint16_t chk = ((data.button & 0xF) << 12) | (data.serial & 0x3FF);
  uint32_t clear = (chk << 16) | data.sync;
  data.encrypted = keeloq_encrypt(clear, key);

  ESP_LOGD(TAG, "Encrypt: Serial %07X Clear: %08X Encrypted: %08X", data.serial, clear, data.encrypted);
}


}  // namespace keeloq_normal_crypter
}  // namespace esphome