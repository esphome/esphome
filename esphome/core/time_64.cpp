#include "esphome/core/defines.h"

#ifndef USE_NATIVE_64BIT_TIME

#include "time_64.h"

#include "esphome/core/helpers.h"
#ifdef ESPHOME_DEBUG_SCHEDULER
#include "esphome/core/log.h"
#include <cinttypes>
#endif
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
#include <atomic>
#endif
#include <limits>

namespace esphome {

#ifdef ESPHOME_DEBUG_SCHEDULER
static const char *const TAG = "time_64";
#endif

#ifdef ESPHOME_THREAD_SINGLE
// Storage for Millis64Impl inline compute() — defined here so all TUs share one copy.
uint32_t Millis64Impl::last_millis{0};
uint16_t Millis64Impl::millis_major{0};
#else

uint64_t Millis64Impl::compute(uint32_t now) {
  // Half the 32-bit range - used to detect rollovers vs normal time progression
  static constexpr uint32_t HALF_MAX_UINT32 = std::numeric_limits<uint32_t>::max() / 2;

  // State variables for rollover tracking - static to persist across calls
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  // Mutex for rollover serialization (taken only every ~49.7 days).
  // A spinlock would be smaller (~1 byte vs ~80-100 bytes) but is unsafe on
  // preemptive single-core RTOS platforms due to priority inversion: a high-priority
  // task spinning would prevent the lock holder from running to release it.
  static Mutex lock;
  /*
   * Multi-threaded platforms with atomic support: last_millis needs atomic for lock-free updates.
   * Writers publish last_millis with memory_order_release and readers use memory_order_acquire.
   * This ensures that once a reader sees the new low word, it also observes the corresponding
   * increment of millis_major.
   */
  static std::atomic<uint32_t> last_millis{0};
  /*
   * Upper 16 bits of the 64-bit millis counter. Incremented only while holding lock;
   * read concurrently. Atomic (relaxed) avoids a formal data race. Ordering relative
   * to last_millis is provided by its release store and the corresponding acquire loads.
   */
  static std::atomic<uint16_t> millis_major{0};
#else /* ESPHOME_THREAD_MULTI_NO_ATOMICS */
  static Mutex lock;
  static uint32_t last_millis{0};
  static uint16_t millis_major{0};
#endif

  // THREAD SAFETY NOTE:
  // This function has two out-of-line implementations, based on the preprocessor flags:
  // - ESPHOME_THREAD_MULTI_NO_ATOMICS - Runs on multi-threaded platforms without atomics (LibreTiny BK72xx)
  // - ESPHOME_THREAD_MULTI_ATOMICS - Runs on multi-threaded platforms with atomics (LibreTiny RTL87xx/LN882x, etc.)
  //
  // The ESPHOME_THREAD_SINGLE path is inlined in time_64.h.
  // Make sure all changes are synchronized if you edit this function.
  //
  // IMPORTANT: Always pass fresh millis() values to this function. The implementation
  // handles out-of-order timestamps between threads, but minimizing time differences
  // helps maintain accuracy.

#if defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
  // Without atomics, this implementation uses locks more aggressively:
  // 1. Always locks when near the rollover boundary (within 10 seconds)
  // 2. Always locks when detecting a large backwards jump
  // 3. Updates without lock in normal forward progression (accepting minor races)
  // This is less efficient but necessary without atomic operations.
  uint16_t major = __atomic_load_n(&millis_major, __ATOMIC_RELAXED);
  uint32_t last = __atomic_load_n(&last_millis, __ATOMIC_RELAXED);

  // Define a safe window around the rollover point (10 seconds)
  // This covers any reasonable scheduler delays or thread preemption
  static constexpr uint32_t ROLLOVER_WINDOW = 10000;  // 10 seconds in milliseconds

  // Check if we're near the rollover boundary (close to std::numeric_limits<uint32_t>::max() or just past 0)
  bool near_rollover = (last > (std::numeric_limits<uint32_t>::max() - ROLLOVER_WINDOW)) || (now < ROLLOVER_WINDOW);

  if (near_rollover || (now < last && (last - now) > HALF_MAX_UINT32)) {
    // Near rollover or detected a rollover - need lock for safety
    LockGuard guard{lock};
    // Re-read both values with lock held. last_millis can be updated
    // unlocked from the forward-progression branch below, so use an atomic
    // load. millis_major can only be updated under this lock, but another
    // thread may have completed a rollover between our unlocked loads above
    // and the lock acquisition — reload or we'd return a stale high word.
    last = __atomic_load_n(&last_millis, __ATOMIC_RELAXED);
    major = __atomic_load_n(&millis_major, __ATOMIC_RELAXED);

    if (now < last && (last - now) > HALF_MAX_UINT32) {
      // True rollover detected (happens every ~49.7 days).
      // Use the already-loaded `major` local; avoids a second read of the
      // global (equivalent under the held lock).
      major++;
      __atomic_store_n(&millis_major, major, __ATOMIC_RELAXED);
#ifdef ESPHOME_DEBUG_SCHEDULER
      ESP_LOGD(TAG, "Detected true 32-bit rollover at %" PRIu32 "ms (was %" PRIu32 ")", now, last);
#endif /* ESPHOME_DEBUG_SCHEDULER */
    }
    // Update last_millis while holding lock
    __atomic_store_n(&last_millis, now, __ATOMIC_RELAXED);
  } else if (now > last) {
    // Normal case: Not near rollover and time moved forward
    // Update without lock. While this may cause minor races (microseconds of
    // backwards time movement), they're acceptable because:
    // 1. The scheduler operates at millisecond resolution, not microsecond
    // 2. We've already prevented the critical rollover race condition
    // 3. Any backwards movement is orders of magnitude smaller than scheduler delays
    __atomic_store_n(&last_millis, now, __ATOMIC_RELAXED);
  }
  // If now <= last and we're not near rollover, don't update
  // This minimizes backwards time movement

  // Combine major (high 32 bits) and now (low 32 bits) into 64-bit time
  return now + (static_cast<uint64_t>(major) << 32);

#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
  // Uses atomic operations with acquire/release semantics to ensure coherent
  // reads of millis_major and last_millis across cores. Features:
  // 1. Epoch-coherency retry loop to handle concurrent updates
  // 2. Lock only taken for actual rollover detection and update
  // 3. Lock-free CAS updates for normal forward time progression
  // 4. Memory ordering ensures cores see consistent time values

  for (;;) {
    uint16_t major = millis_major.load(std::memory_order_acquire);

    /*
     * Acquire so that if we later decide **not** to take the lock we still
     * observe a millis_major value coherent with the loaded last_millis.
     * The acquire load ensures any later read of millis_major sees its
     * corresponding increment.
     */
    uint32_t last = last_millis.load(std::memory_order_acquire);

    // If we might be near a rollover (large backwards jump), take the lock
    // This ensures rollover detection and last_millis update are atomic together
    if (now < last && (last - now) > HALF_MAX_UINT32) {
      // Potential rollover - need lock for atomic rollover detection + update
      LockGuard guard{lock};
      // Re-read with lock held; mutex already provides ordering
      last = last_millis.load(std::memory_order_relaxed);

      if (now < last && (last - now) > HALF_MAX_UINT32) {
        // True rollover detected (happens every ~49.7 days)
        millis_major.fetch_add(1, std::memory_order_relaxed);
        major++;
#ifdef ESPHOME_DEBUG_SCHEDULER
        ESP_LOGD(TAG, "Detected true 32-bit rollover at %" PRIu32 "ms (was %" PRIu32 ")", now, last);
#endif /* ESPHOME_DEBUG_SCHEDULER */
      }
      /*
       * Update last_millis while holding the lock to prevent races.
       * Publish the new low-word *after* bumping millis_major (done above)
       * so readers never see a mismatched pair.
       */
      last_millis.store(now, std::memory_order_release);
    } else {
      // Normal case: Try lock-free update, but only allow forward movement within same epoch
      // This prevents accidentally moving backwards across a rollover boundary
      while (now > last && (now - last) < HALF_MAX_UINT32) {
        if (last_millis.compare_exchange_weak(last, now,
                                              std::memory_order_release,     // success
                                              std::memory_order_relaxed)) {  // failure
          break;
        }
        // CAS failure means no data was published; relaxed is fine
        // last is automatically updated by compare_exchange_weak if it fails
      }
    }
    uint16_t major_end = millis_major.load(std::memory_order_relaxed);
    if (major_end == major)
      return now + (static_cast<uint64_t>(major) << 32);
  }
  // Unreachable - the loop always returns when major_end == major
  __builtin_unreachable();

#else
#error \
    "No platform threading model defined. One of ESPHOME_THREAD_SINGLE, ESPHOME_THREAD_MULTI_NO_ATOMICS, or ESPHOME_THREAD_MULTI_ATOMICS must be defined."
#endif
}

#endif  // !ESPHOME_THREAD_SINGLE

}  // namespace esphome

#endif  // !USE_NATIVE_64BIT_TIME
