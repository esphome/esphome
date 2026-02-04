#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * JD7966x IC driver implementation
 *
 * Currently tested with:
 * - JD79660 (max res: 200x200)
 *
 * May also work for other JD7966x chipset family members with minimal adaptations.
 *
 * Capabilities:
 * - HW frame buffer layout:
 *   4 colors (gray0..3, commonly BWYR). Bytes consist of 4px/2bpp.
 *   Width must be rounded to multiple of 4.
 * - Fast init/update (shorter wave forms): Yes. Controlled by CONF_FULL_UPDATE_EVERY.
 *   Needs undocumented fastinit sequence, based on likely vendor specific MTP content.
 * - Partial transfer (transfer only changed window): No. Maybe possible by HW.
 * - Partial refresh (refresh only changed window): No. Likely HW limit.
 *
 * @internal \c final saves few bytes by devirtualization. Remove \c final when subclassing.
 */
class EPaperJD79660 final : public EPaperBase {
 public:
  EPaperJD79660(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length, const uint8_t *fast_update, uint16_t fast_update_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR),
        fast_update_(fast_update),
        fast_update_length_(fast_update_length) {
    this->row_width_ = (width + 3) / 4;  // Fix base class calc (2bpp instead of 1bpp)
    this->buffer_length_ = this->row_width_ * height;
  }

  void fill(Color color) override;

 protected:
  /** Draw colored pixel into frame buffer */
  void draw_pixel_at(int x, int y, Color color) override;

  /** Reset (multistep sequence)
   * @pre this->reset_pin_ != nullptr // cv.Required check
   * @post Should be idle on successful reset. Can mark failures.
   */
  bool reset() override;

  /** Initialise (multistep sequence) */
  bool initialise(bool partial) override;

  /** Buffer transfer */
  bool transfer_data() override;

  /** Power on: Already part of init sequence (likely needed there before transferring buffers).
   * So nothing to do in FSM state.
   */
  void power_on() override {}

  /** Refresh screen
   * @param partial Ignored: Needed earlier in \a ::initialize
   * @pre Must be idle.
   * @post Should return to idle later after processing.
   */
  void refresh_screen([[maybe_unused]] bool partial) override;

  /** Power off
   * @pre Must be idle.
   * @post Should return to idle later after processing.
   *       (latter will take long period like ~15-20s on actual refresh!)
   */
  void power_off() override;

  /** Deepsleep: Must be used to avoid hardware wearout!
   * @pre Must be idle.
   * @post Will go busy, and not return idle till ::reset!
   */
  void deep_sleep() override;

  /** Internal: Send fast init sequence via undocumented vendor registers
   * @pre Must be directly after regular ::initialise sequence, before ::transfer_data
   * @pre Must be idle.
   * @post Should return to idle later after processing.
   */
  void write_fastinit_();

  /** Internal: Send raw buffer in chunks
   * \retval true Finished
   * \retval false Loop time elapsed. Need to call again next loop.
   */
  bool transfer_buffer_chunks_();

  /** @name IC commands @{ */
  static constexpr uint8_t CMD_POWEROFF = 0x02;
  static constexpr uint8_t CMD_DEEPSLEEP = 0x07;
  static constexpr uint8_t CMD_TRANSFER = 0x10;
  static constexpr uint8_t CMD_REFRESH = 0x12;
  /** @} */

  /** State machine constants for \a step_ */
  enum class FSMState : uint8_t {
    NONE = 0,  //!< Initial/default value: Unused

    /* Reset state steps */
    RESET_STEP0_H,
    RESET_STEP1_L,
    RESET_STEP2_IDLECHECK,

    /* Init state steps */
    INIT_STEP0_REGULARINIT,
    INIT_STEP1_FASTINIT,
  };

  /** Wait time (millisec) for first reset phase: High
   *
   * Wait via FSM loop.
   */
  static constexpr uint16_t SLEEP_MS_RESET0 = 200;

  /** Wait time (millisec) for second reset phase: Low
   *
   * Holding Reset Low too long may trigger "clever reset" logic
   * of e.g. Waveshare Rev2 boards: VDD is shut down via MOSFET, and IC
   * will not report idle anymore!
   * FSM loop may spuriously increase delay, e.g. >16ms.
   * Therefore, sync wait below, as allowed (code rule "delays > 10ms not permitted"),
   * yet only slightly exceeding known IC min req of >1.5ms.
   */
  static constexpr uint16_t SLEEP_MS_RESET1 = 2;

  /** Wait time (millisec) for third reset phase: High
   *
   * Wait via FSM loop.
   */
  static constexpr uint16_t SLEEP_MS_RESET2 = 200;

  // properties initialised in the constructor
  const uint8_t *const fast_update_{};
  const uint16_t fast_update_length_{};

  /** Counter for tracking substeps within FSM state */
  FSMState step_{FSMState::NONE};
};

}  // namespace esphome::epaper_spi
