#include "xxtea.h"

namespace esphome {
namespace xxtea {

static const uint32_t DELTA = 0x9e3779b9;
#define MX ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (k[(p ^ e) & 7] ^ z)))

void encrypt(uint32_t *v, size_t n, const uint32_t *k) {
  uint32_t z, y, sum, e;
  size_t p;
  size_t q = 6 + 52 / n;
  sum = 0;
  z = v[n - 1];
  while (q-- != 0) {
    sum += DELTA;
    e = (sum >> 2);
    for (p = 0; p != n - 1; p++) {
      y = v[p + 1];
      z = v[p] += MX;
    }
    y = v[0];
    z = v[n - 1] += MX;
  }
}

void decrypt(uint32_t *v, size_t n, const uint32_t *k) {
  uint32_t z, y, sum, e;
  size_t p;
  size_t q = 6 + 52 / n;
  sum = q * DELTA;
  y = v[0];
  while (q-- != 0) {
    e = (sum >> 2);
    for (p = n - 1; p != 0; p--) {
      z = v[p - 1];
      y = v[p] -= MX;
    }
    z = v[n - 1];
    y = v[0] -= MX;
    sum -= DELTA;
  }
}

}  // namespace xxtea
}  // namespace esphome
