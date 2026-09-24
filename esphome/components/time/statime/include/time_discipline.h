#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Internal experimental ABI; compiled together with the pinned Rust adapter.
// Device stack validation is still experimental; see README.md for measured
// frames and the remaining full-call-chain and physical-device checks.
// All timestamps are nonnegative TAI nanoseconds since the TAI Unix epoch.
// UTC conversion and legacy-source uncertainty belong to the common time bridge.
// Callbacks return zero on success and must initialize their output on success.
// They must not throw, unwind, or retain output pointers. The context must remain
// alive until destroy; callbacks must support serialized calls on the owner thread.
typedef struct {
  void *context;
  int32_t (*now)(void *context, int64_t *timestamp_ns);
  int32_t (*set_frequency)(void *context, double ratio, int64_t *timestamp_ns);
  int32_t (*get_frequency)(void *context, double *ratio);
  int32_t (*step)(void *context, int64_t offset_ns, int64_t *timestamp_ns);
  double max_frequency_ratio;
} EsphomeTimeClock;

typedef struct {
  double offset_seconds;
  double offset_variance;
  double frequency_ratio;
  double frequency_variance;
  double dispersion_seconds;
  uint32_t active_sources;
} EsphomeTimeEstimate;

enum {
  ESPHOME_TIME_OK = 0,
  ESPHOME_TIME_INVALID_ARGUMENT = 1,
  ESPHOME_TIME_BUSY = 2,
  ESPHOME_TIME_CLOCK_ERROR = 3,
  ESPHOME_TIME_NONMONOTONIC = 4,
  ESPHOME_TIME_ALGORITHM_ERROR = 5,
  ESPHOME_TIME_FAULTED = 6,
};

// Allocate storage once at setup using these exact size/alignment requirements.
// The storage is opaque: do not copy/move/reinitialize it while live. A successful
// init must be paired with destroy before freeing it. Failed init leaves it dead.
// No concurrent calls are permitted. Init callbacks must not access the handle.
// Operations on live handles reject reentry with BUSY. Except init, every handle
// must name a successfully initialized, not yet destroyed instance.
size_t esphome_time_size(void);
size_t esphome_time_alignment(void);
uint32_t esphome_time_init(void *storage, size_t size, EsphomeTimeClock clock, uint32_t sources);
uint32_t esphome_time_destroy(void *storage);

// Sources are stable indices [0, sources), at most two for this first adapter.
// Uncertainty is a positive standard deviation in nanoseconds, not a hard bound.
// Caller timestamps must describe the same observation, before any new steering.
uint32_t esphome_time_observe(void *storage, uint32_t source, int64_t reference_ns, int64_t local_ns,
                              int64_t uncertainty_ns);
uint32_t esphome_time_set_usable(void *storage, uint32_t source, uint32_t usable);
uint32_t esphome_time_update(void *storage);
uint32_t esphome_time_estimate(void *storage, EsphomeTimeEstimate *estimate);

// Algorithm/clock errors latch FAULTED: completed clock changes cannot be rolled
// back. Stop steering and explicitly destroy/reinitialize after diagnosing them.
// Invalid arguments and BUSY do not change controller state.
// Provide this fatal diagnostic hook; Rust calls abort() after it returns.
void esphome_time_panic(const char *message, size_t length);

#ifdef __cplusplus
}
#endif
