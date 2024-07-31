#include "opentherm_hub.h"

#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"

#include <unordered_map>

namespace esphome {
    namespace opentherm {

        static const char *TAG = "opentherm";

        OpenthermHub::OpenthermStatusListener::OpenthermStatusListener()
                : OpenthermComponent(OpenThermMessageID::Status, true) {

        }

        void OpenthermHub::OpenthermStatusListener::process_response(const unsigned long response) {
            /* Do nothing */
        }

        unsigned int OpenthermHub::OpenthermStatusListener::build_request(const unsigned int data) {
            return data;
        }

        unsigned int OpenthermHub::build_request(OpenThermMessageID id) {
            unsigned int data = 0;

            for (auto component: components)
                if (component->message_id == id)
                    data = component->build_request(data);

            return ot.buildRequest(get_message_type(id), id, data);
        }

        OpenThermMessageType OpenthermHub::get_message_type(OpenThermMessageID id) {
            switch (id) {
                case OpenThermMessageID::Status:
                    return OpenThermMessageType::READ_DATA;
                default:
                    return OpenThermMessageType::UNKNOWN_DATA_ID;
            }
        }

        OpenthermHub::OpenthermHub(int in_pin, int out_pin, bool is_thermostat,
                                   handle_interrupt_t handle_interrupt_cb, process_response_t process_response_cb)
                : Component(),
                  ot(in_pin, out_pin, is_thermostat),
                  components(),
                  queue(),
                  status_listener(),
                  handle_interrupt_cb(handle_interrupt_cb),
                  process_response_cb(process_response_cb) {
            /* Do nothing */
        }

        void IRAM_ATTR OpenthermHub::handle_interrupt() {
            ot.handleInterrupt();
        }

        void OpenthermHub::process_response(unsigned long response, OpenThermResponseStatus status) {
            if (!ot.isValidResponse(response)) {
                ESP_LOGW(TAG, "Received invalid OpenTherm response: %s, status=%s", String(response, HEX).c_str(),
                         String(static_cast<int>(ot.getLastResponseStatus())).c_str());
                return;
            }

            auto id = static_cast<OpenThermMessageID>((response >> 16) & 0xFF);
            ESP_LOGD(TAG, "Received OpenTherm response with id %d: %s", id, String(response, HEX).c_str());

            for (auto component: components)
                if (component->message_id == id)
                    component->process_response(response);
        }

        void OpenthermHub::register_component(OpenthermComponent *component) {
            components.insert(component);
        }

        void OpenthermHub::setup() {
            ESP_LOGD(TAG, "Registering status listener");
            register_component(&status_listener);

            ESP_LOGD(TAG, "Initializing message queue");
            for (auto component: components)
                if (!component->keep_updated)
                    queue.push(component->message_id);

            ESP_LOGD(TAG, "Starting OpenTherm component");
            ot.begin(handle_interrupt_cb, process_response_cb);
        }

        void OpenthermHub::on_shutdown() {
            ot.end();
        }

        void OpenthermHub::loop() {
            if (ot.isReady()) {
                /* Determine if the queue contains elements */
                if (queue.empty()) {
                    /* Fill the queue with repeating messages if none are available */
                    for (auto component: components)
                        if (component->keep_updated)
                            queue.push(component->message_id);
                }

                /* Get the first message ID from the queue */
                if (!queue.empty()) {
                    /* Build the request */
                    unsigned int request = build_request(queue.front());
                    queue.pop();

                    /* Send the OpenTherm request */
                    ot.sendRequestAync(request);
                    ESP_LOGD(TAG, "Sent OpenTherm request: %s", String(request, HEX).c_str());
                }
            }

            ot.process();
        }

        void OpenthermHub::dump_config() {
            ESP_LOGCONFIG(TAG, "OpenTherm");
        }

    }  // namespace opentherm
}  // namespace esphome