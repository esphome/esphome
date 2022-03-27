#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/automation.h"
namespace esphome {
namespace remote {

using arg_t = int64_t;

class RemoteCapabilities {
 public:
  std::vector<std::string> supports_transmit;
  std::vector<std::string> supports_receive;
};

/** Base class for all remotes.
 *
 * A Remote allows to send IR/RF commands.
 */
class Remote : public EntityBase {
 public:
  explicit Remote(){};

  const RemoteCapabilities &get_capabilities() { return capabilities_; }

  /** Turn this remote on. This is called by the front-end.
   *
   * For implementing remotes, please override write_state.
   */
  void turn_on();
  /** Turn this remote off. This is called by the front-end.
   *
   * For implementing switches, please override write_state.
   */
  void turn_off();
  /** Send specific command thru remote. This is called by the front-end.
   *
   * For implementing switches, please override transmit.
   */
  void send_command(int repeat, int wait, const std::string &protocol, const std::vector<arg_t> &args) {
    transmit(repeat, wait, protocol, args);
  }

  /** Set callback for state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void(bool)> &&callback) { state_callback_.add(std::move(callback)); }

 protected:
  /** Write the given command to hardware. You should implement this
   * abstract method if you want to create your own Remote.
   *
   * In the implementation of this method, you should also call
   * publish_state to acknowledge that the state was written to the hardware.
   *
   */
  virtual void transmit(int repeat, int wait, const std::string &protocol, const std::vector<arg_t> &args) = 0;

  virtual void write_state(bool state){};

  uint32_t hash_base() override { return 1671990586UL; };

  CallbackManager<void(bool)> state_callback_{};
  RemoteCapabilities capabilities_{};
};

}  // namespace remote
}  // namespace esphome
