#pragma once

#include <cstdint>

namespace esphome::zephyr {

/* Host (Linux) side of the I2C passthrough bridge.
 *
 * This header is included from both Zephyr application code and the native
 * simulator's host-side "bottom" implementation, so it must only use basic C
 * types -- no Zephyr or POSIX headers. */

/* Opens linux_bus (e.g. "/dev/i2c-1"). Returns a file descriptor >= 0 on
 * success, or a negative errno value on failure. */
int i2c_passthrough_open(const char *linux_bus);

void i2c_passthrough_close(int fd);

/* Performs one write-then-read I2C transaction against `addr` on the bus
 * referenced by `fd`, mirroring the two-message pattern used by Zephyr i2c
 * sensor drivers (write register address, then read N bytes).
 *
 * Either half may be skipped by passing a zero length. Returns 0 on success,
 * or a negative errno value on failure. */
int i2c_passthrough_transfer(int fd, uint16_t addr, const unsigned char *write_buf, unsigned int write_len,
                             unsigned char *read_buf, unsigned int read_len);
}  // namespace esphome::zephyr
