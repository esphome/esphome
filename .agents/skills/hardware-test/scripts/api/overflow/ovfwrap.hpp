#pragma once
// Counts calls into the API overflow buffer through -Wl,--wrap (test only).
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
struct iovec;
extern "C" {
uint32_t ovf_enq_calls = 0;
uint32_t ovf_enq_bytes = 0;
uint32_t ovf_drain_calls = 0;
bool __real__ZN7esphome3api17APIOverflowBuffer11enqueue_iovEPK5iovecijj(void *self, const iovec *iov, int n,
                                                                        size_t total, size_t skip);
bool __wrap__ZN7esphome3api17APIOverflowBuffer11enqueue_iovEPK5iovecijj(void *self, const iovec *iov, int n,
                                                                        size_t total, size_t skip) {
  bool ok = __real__ZN7esphome3api17APIOverflowBuffer11enqueue_iovEPK5iovecijj(self, iov, n, total, skip);
  if (ok) {
    ovf_enq_calls++;
    ovf_enq_bytes += total - skip;
  }
  return ok;
}
ssize_t OVF_DRAIN_REAL(void *self, void *sock);
ssize_t OVF_DRAIN_WRAP(void *self, void *sock) {
  ovf_drain_calls++;
  return OVF_DRAIN_REAL(self, sock);
}
}
