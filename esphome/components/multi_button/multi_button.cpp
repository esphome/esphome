#include "esphome/core/log.h"
#include "multi_button.h"

namespace esphome {
namespace multi_button {

static const char *TAG = "multi_button";

void MultiButton::setup() {
    this->state_ = MultiButtonState::RELEASED;
    circular_buf_init(200, &(this->event_queue_));
    this->startTime_ = millis();
    this->level_ = false;
    this->press_count_ = 0;
    this->hold_count_ = 0;
    this->reset_ = true;

    this->pin_->setup();
    this->pin_->attach_interrupt(MultiButton::gpio_interrupt_handler_, this, CHANGE);
}

void MultiButton::loop() {
    while (!circular_buf_empty(this->event_queue_)) {
        GpioEvent* gpio_event;
        int error = circular_buf_get(this->event_queue_, (void**)&gpio_event);
        if (!error) {
            tick_(gpio_event->time, gpio_event->level);
            this->level_ = gpio_event->level;
            delete gpio_event;
        }
        else {
            ESP_LOGD(TAG, "Error dequeing from event queue");
        }
    }
    // for debouncing and timeout handling we need to call tick_ even if nothing happened
    tick_(millis(), this->level_);
}


void MultiButton::dump_config() {
    ESP_LOGCONFIG(TAG, "MultiButton");
    //ESP_LOGCONFIG(TAG, "  Name: %s", .c_str());
    LOG_PIN("  Pin: ", this->pin_);
    ESP_LOGCONFIG(TAG, "  Debouncing time: %dms", this->debounce_millis_);
    ESP_LOGCONFIG(TAG, "  Timeout: %dms", this->timeout_millis_);
    ESP_LOGCONFIG(TAG, "  Threshold for press&hold: %dms", this->press_hold_threshold_millis_);
    ESP_LOGCONFIG(TAG, "  Frequency of press&hold updates: %3.1fHz", 1000.0f / (float)this->press_hold_update_interval_millis_);
}



void ICACHE_RAM_ATTR MultiButton::gpio_interrupt_handler_(MultiButton *sensor) {
    GpioEvent* gpio_event = new GpioEvent;
    gpio_event->time = millis();
    gpio_event->level = sensor->pin_->digital_read() > 0;
    int error = circular_buf_put(sensor->event_queue_, gpio_event);
    if (error)
    {
        delete gpio_event;
    }
}


void MultiButton::tick_(unsigned long time, bool level) {
    //if (this->state_debug_ != this->state_) {
    //    this->state_debug_ = this->state_;
    //    this->level_debug_ = level;
    //    ESP_LOGD(TAG, "state %d  level %d", (int)this->state_debug_, (int)this->level_debug_);
    //}

    unsigned long time_since_start = time - this->startTime_;

    switch (state_) {

    case MultiButtonState::RELEASED:
        // state: the button was released
        if (level) {
            // The button is now pressed (not yet debounced)
            this->state_ = MultiButtonState::MAYBE_PRESSED;
            this->startTime_ = time;
        }
        else if (this->press_count_ && time_since_start > this->timeout_millis_) {
            // There is a press count to be published and the timeout for another press is expired
            publish_state(this->press_count_);
            // Reset counter
            this->press_count_ = 0;
            // Later reset public sensor state
            this->reset_ = true;
        }
        else if (this->reset_ && time_since_start > this->timeout_millis_) {
            // Reset sensor state in order to prevent accidental events based on old state
            publish_state(0);
            this->reset_ = false;
        }
        break;

    case MultiButtonState::MAYBE_PRESSED:
        // state: the button was pressed (or is it bouncing?)
        if (!level && time_since_start < this->debounce_millis_) {
            // Bounce detected
            this->state_ = MultiButtonState::RELEASED;
        }
        else if (time_since_start > this->debounce_millis_) {
            // The debounce window is over -> this is a press
            this->state_ = MultiButtonState::PRESSED;
        }
        break;

    case MultiButtonState::PRESSED:
        // state: the button was pressed
        if (!level) {
            // The button is now released (not yet debounced)
            this->state_ = MultiButtonState::MAYBE_RELEASED;
            this->startTime_ = time;
        }
        else if (time_since_start > this->press_hold_threshold_millis_) {
            // The button is still being pressed and it's long enough to be a press&hold
            this->state_ = MultiButtonState::PRESS_HOLD;
        }
        break;

    case MultiButtonState::MAYBE_RELEASED:
        // state: the button was released (or is it bouncing?)
        if (level && time_since_start < this->debounce_millis_) {
            // Bounce detected
            this->state_ = MultiButtonState::PRESSED;
        }
        else if (time_since_start > this->debounce_millis_) {
            // The debounce window is over -> this is a release
            this->press_count_++;
            this->state_ = MultiButtonState::RELEASED;
        }
        break;


    case MultiButtonState::PRESS_HOLD:
        // state: the button is pressed&held
        if (!level) {
            // The button is released (not yet debounced)
            this->state_ = MultiButtonState::MAYBE_NOT_PRESS_HOLD;
            this->startTime_ = time;
        }
        else if (time_since_start > this->press_hold_update_interval_millis_) {
            // The button is still being pressed and it's time to publish an update
            if (this->hold_count_ < 99) {
                this->hold_count_++;
                publish_state(((this->press_count_ + 1) * 100) + this->hold_count_);
                this->startTime_ = time;
            }
        }
        break;

    case MultiButtonState::MAYBE_NOT_PRESS_HOLD:
        // state: the button was released after a press&hold (or is it bouncing?)
        if (level && (time_since_start < this->debounce_millis_)) {
            // Bounce detected
            this->state_ = MultiButtonState::PRESS_HOLD;
        }
        else if (time_since_start >= this->debounce_millis_) {
            // The button stayed released for the full debouncing window
            this->state_ = MultiButtonState::RELEASED;
            // Reset
            this->press_count_ = 0;
            this->hold_count_ = 0;
            publish_state(0);
        }
        break;
    }
}


} //namespace multi_button
} //namespace esphome