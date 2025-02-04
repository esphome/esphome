#pragma once

#include <cstdint>
#include <cstddef>

namespace esphome {
namespace xxtea {

/**
 * Encrypt a block of data in-place using XXTEA algorithm with 256-bit key
 * @param v Data to encrypt (as array of 32-bit words)
 * @param n Number of 32-bit words in data
 * @param k Key (array of 8 32-bit words)
 */
void encrypt(uint32_t *v, size_t n, const uint32_t *k);

/**
 * Decrypt a block of data in-place using XXTEA algorithm with 256-bit key
 * @param v Data to decrypt (as array of 32-bit words)
 * @param n Number of 32-bit words in data
 * @param k Key (array of 8 32-bit words)
 */
void decrypt(uint32_t *v, size_t n, const uint32_t *k);

}  // namespace xxtea
}  // namespace esphome
