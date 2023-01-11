#include "min_max_sliding_window.h"
#include <deque>
#include <iostream>

namespace esphome {
namespace minmax {

class MinMaxSensor
{
    public:
        MinMaxSensor();

        long unsigned int get_long_term_period() const {
            return long_term_period;
        }
        void set_long_term_period(int long_term_period) {
            this->long_term_period = long_term_period;
        }

        float get_stability_threshold() const {
            return stability_threshold;
        }
        void set_stability_threshold(int stability_threshold) {
            this->stability_threshold = stability_threshold;
        }

        float get_current_state() const {
            return current_state + correction_offset;
        }
        void set_current_state(float state);

        bool stable() const {
            return (stable_states.size() == long_term_period);
        }

        float get_long_term_min() const {
            return long_term_min + correction_offset;
        }
        float get_local_min() const {
            return local_min + correction_offset;
        }
        float get_long_term_max() const {
            return long_term_max + correction_offset;
        }
        float get_local_max() const {
            return local_max + correction_offset;
        }

        float get_stable_average();

        void apply_correction_offset(float offset);

        friend std::ostream& operator<<(std::ostream& os, const MinMaxSensor& min_max_sensor);

    protected:
        long unsigned int long_term_period = 300; // in samples

        float stability_threshold = 0.1;

        std::deque<float> stable_states;

        MinMaxSlidingWindow min_max_sliding_window;

        float long_term_min;
        float long_term_max;

        float running_local_min;
        float local_min;

        float running_local_max;
        float local_max;

        float previous_state;
        float current_state;

        float correction_offset = 0;

        void update_long_term_min_max(float state);
        void update_local_min_max(float state);

        void add_new_state(float state);
        void remove_oldest_state();
};

}  // namespace minmax
}  // namespace esphome