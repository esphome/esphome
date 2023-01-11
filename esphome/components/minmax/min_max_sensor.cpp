#include "min_max_sensor.h"
#include <limits>
#include <cmath>

namespace esphome {
namespace minmax {

MinMaxSensor::MinMaxSensor() {
    this->long_term_min = std::numeric_limits<float>::quiet_NaN();
    this->long_term_max = std::numeric_limits<float>::quiet_NaN();
    this->local_min = std::numeric_limits<float>::quiet_NaN();
    this->local_max = std::numeric_limits<float>::quiet_NaN();
    this->previous_state = std::numeric_limits<float>::quiet_NaN();
    this->current_state = std::numeric_limits<float>::quiet_NaN();
}

void MinMaxSensor::set_current_state(float state) {
    this->update_long_term_min_max(state);

    this->update_local_min_max(state);

    this->current_state = state;
}

void MinMaxSensor::update_long_term_min_max(float state) {
    // remove stable states falling outside the stable threshold compared to the new current state
    //std::cout << "Deviation: " << fabs(stable_states.front() - state) << std::endl;
    while (!stable_states.empty() && fabs(stable_states.front() - state) > stability_threshold) {
        remove_oldest_state();
    }

    if (!stable_states.empty()) {
        // assimilate to no change

        add_new_state(state);

        if (stable_states.size() > long_term_period) {
            remove_oldest_state();
        }

        if (stable_states.size() == long_term_period) {
            // the long term period has elapsed
            // => we should have a long-term min and/or max

            if (std::isnan(previous_state)) {
                long_term_min = min_max_sliding_window.get_minimum();
                long_term_max = min_max_sliding_window.get_maximum();
            } else {
                if (previous_state > min_max_sliding_window.get_maximum()) {
                    // we are in a globally decreasing trend
                    long_term_min = min_max_sliding_window.get_minimum();
                } else {
                    if (previous_state < min_max_sliding_window.get_minimum()) {
                        // we are in globally increasing trend
                        long_term_max = min_max_sliding_window.get_maximum();
                    } else {
                        // the trend is globally flat within the bounds defined by the threshold
                        long_term_min = min_max_sliding_window.get_minimum();
                        long_term_max = min_max_sliding_window.get_maximum();
                    }
                }
            }
        }
    } else {
        add_new_state(state);
    }
}

void MinMaxSensor::add_new_state(float state) {
    stable_states.push_back(state);
    min_max_sliding_window.add_tail(state);
}

void MinMaxSensor::remove_oldest_state() {
    previous_state = stable_states.front();
    stable_states.pop_front();
    min_max_sliding_window.remove_head(previous_state);
    //std::cout << "removed oldest state: " << previous_state << std::endl;
}

void MinMaxSensor::update_local_min_max(float state) {
    if (std::isnan(this->current_state)) {
        local_min = local_max = state;
        running_local_min = running_local_max = state;
    } else {
        if (current_state < state) {
            running_local_max = state;
        } else {
            if (current_state > state) {
                running_local_min = state;
            }
        }

        if (!std::isnan(previous_state)) {
            if (previous_state < current_state) {
                // was increasing
                if (current_state >= state) {
                    // but now decreasing or flat
                    local_max = std::max(running_local_max, min_max_sliding_window.get_maximum());
                }
            } else {
                if (previous_state > current_state) {
                    // was decreasing
                    if (current_state <= state) {
                        // but now increasing or flat
                        local_min = std::min(running_local_min, min_max_sliding_window.get_minimum());
                    }
                }
            }
        }
    }
}

float MinMaxSensor::get_stable_average() {
    if (stable()) {
        return (min_max_sliding_window.get_minimum() + min_max_sliding_window.get_maximum()) / 2.0;
    }

    return std::numeric_limits<float>::quiet_NaN();
}

void MinMaxSensor::apply_correction_offset(float offset) {
    this->correction_offset = offset;
}

std::ostream& operator<<(std::ostream& os, const MinMaxSensor& min_max_sensor)
{
    os << "cstate=" << min_max_sensor.get_current_state()
        << ", pstate=" << min_max_sensor.previous_state
        << ", rmin=" << min_max_sensor.running_local_min
        << ", lmin=" << min_max_sensor.get_local_min()
        << ", rmax=" << min_max_sensor.running_local_max
        << ", lmax=" << min_max_sensor.get_local_max()
        << ", ltmin=" << min_max_sensor.get_long_term_min()
        << ", ltmax=" << min_max_sensor.get_long_term_max()
        << ", #ss=" << min_max_sensor.stable_states.size()
        << ", stable=" << min_max_sensor.stable();

    if (!min_max_sensor.stable_states.empty()) {
        os << ", fs=" << min_max_sensor.stable_states.front();
    }

    return os;
}


}  // namespace minmax
}  // namespace esphome