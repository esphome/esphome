#include <atomic>
#include <cassert>
#include <cstdint>

#define ESP_LOGD(...) ((void) 0)
#define ESP_LOGE(...) ((void) 0)
#define ESP_LOGW(...) ((void) 0)
#define ESP_LOGV(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
using esp_err_t = int;
using esp_event_base_t = int;
constexpr int ESP_OK = 0;
constexpr int ETHERNET_EVENT_START = 1;
constexpr int ETHERNET_EVENT_STOP = 2;
constexpr int ETHERNET_EVENT_CONNECTED = 3;
constexpr int ETHERNET_EVENT_DISCONNECTED = 4;
int stop_result = ESP_OK;
int start_result = ESP_OK;
int stop_calls = 0;
int start_calls = 0;
esp_err_t esp_eth_stop(void *) {
  ++stop_calls;
  return stop_result;
}
esp_err_t esp_eth_start(void *) {
  ++start_calls;
  return start_result;
}
struct MockApp {
  uint32_t get_loop_component_start_time() { return 0; }
} App;
enum class EthernetComponentState { STOPPED, CONNECTING, CONNECTED };
class EthernetComponent {
 public:
  void enable();
  void disable();
  void loop();
  static void eth_event_handler(void *, esp_event_base_t, int32_t, void *);
  // PRODUCTION_ACCESSORS
  void ethernet_lazy_init_() {}
  void enable_loop() {}
  void disable_loop() {}
  void start_connect_() {}
  void finish_connect_() {}
  void dump_connect_params_() {}
  void status_clear_warning() {}
  void enable_loop_soon_any_context() {}
  void *eth_handle_{this};
  bool disabled_{false};
  bool ethernet_initialized_{true};
  bool started_{true};
  bool connected_{true};
  std::atomic<bool> driver_stopped_{false};
  bool pending_enable_{false};
  uint32_t connect_begin_{0};
  EthernetComponentState state_{EthernetComponentState::CONNECTED};
};
EthernetComponent *global_eth_component;

// PRODUCTION_METHODS

int main() {
  EthernetComponent eth;
  global_eth_component = &eth;
  stop_result = -1;
  eth.disable();
  assert(!eth.disabled_ && !eth.is_driver_stopped());
  assert(eth.is_connected());
  stop_result = ESP_OK;
  eth.disable();
  assert(eth.disabled_ && !eth.is_driver_stopped());
  assert(!eth.is_connected());
  eth.enable();
  assert(start_calls == 0);
  assert(eth.pending_enable_);
  eth.loop();
  assert(start_calls == 0);
  const int accepted_stop_calls = stop_calls;
  eth.disable();
  assert(!eth.pending_enable_);  // A later disable cancels the earlier enable.
  assert(stop_calls == accepted_stop_calls);
  EthernetComponent::eth_event_handler(nullptr, 0, ETHERNET_EVENT_START, nullptr);
  assert(!eth.is_driver_stopped());
  EthernetComponent::eth_event_handler(nullptr, 0, ETHERNET_EVENT_STOP, nullptr);
  assert(eth.is_driver_stopped());
  eth.loop();
  assert(start_calls == 0);  // Cancelled intent stays cancelled after STOP.
  eth.enable();
  assert(start_calls == 1 && !eth.disabled_);
  assert(!eth.is_connected() && !eth.is_driver_stopped());
  eth.disable();
  eth.enable();
  assert(eth.pending_enable_ && start_calls == 1);
  EthernetComponent::eth_event_handler(nullptr, 0, ETHERNET_EVENT_STOP, nullptr);
  eth.loop();
  assert(start_calls == 2 && !eth.disabled_ && !eth.pending_enable_);
  // Failed start attempts cleanup but cannot retry before its STOP event.
  eth.disable();
  EthernetComponent::eth_event_handler(nullptr, 0, ETHERNET_EVENT_STOP, nullptr);
  start_result = -1;
  eth.enable();
  assert(start_calls == 3 && eth.disabled_ && !eth.is_driver_stopped());
  const int cleanup_stop_calls = stop_calls;
  eth.enable();
  assert(start_calls == 3 && stop_calls == cleanup_stop_calls);
  EthernetComponent::eth_event_handler(nullptr, 0, ETHERNET_EVENT_STOP, nullptr);
  start_result = ESP_OK;
  eth.enable();
  assert(start_calls == 4 && !eth.disabled_ && !eth.pending_enable_);
  // Ambiguous cleanup failure must remain blocked, even if the next start would succeed.
  eth.disable();
  EthernetComponent::eth_event_handler(nullptr, 0, ETHERNET_EVENT_STOP, nullptr);
  start_result = -1;
  stop_result = -1;
  eth.enable();
  assert(start_calls == 5 && eth.disabled_ && !eth.is_driver_stopped());
  start_result = ESP_OK;
  eth.enable();
  eth.loop();
  assert(start_calls == 5);  // Missing STOP never retries an ambiguous start.
}
