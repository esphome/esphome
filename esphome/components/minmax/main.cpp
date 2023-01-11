#include <iostream>
#include <cmath>
#include "min_max_sensor.h"

using namespace std;

static float global_offset = 0.0;

float get_in_temp(int period) {
       if (period < 500) {
            return 17;
        } else {
            if (period >= 500 && period < 9499) {
                return (float) (26 + 7*std::sin((period - 500) / 360.0));
            } else {
                return 16;
            }
        }
}

float get_out_temp(int period) {
    return 16;
}

static float max_heating_pwm = 0.6;
static bool boost = false;
static float boost_factor = 1.2f;

float compute_pwm_output(MinMaxSensor in_sensor, MinMaxSensor out_sensor) {
    if (in_sensor.get_current_state() > out_sensor.get_current_state()) {
        // Heating
        float local_max = (in_sensor.get_local_max() < in_sensor.get_current_state()) ? in_sensor.get_current_state() : in_sensor.get_local_max();

        float current_min = isnan(in_sensor.get_long_term_min()) ? in_sensor.get_local_min() : in_sensor.get_long_term_min();

        float current_range = (local_max == current_min) ? 0.0 : (1.0 / (local_max - current_min));

        float base_pwm = max_heating_pwm * (in_sensor.get_current_state() - current_min) * current_range;

        if (boost) {
            base_pwm *= boost_factor;
        }

        return min(max_heating_pwm, base_pwm);
    } else {
        if (in_sensor.get_current_state() == out_sensor.get_current_state()) {
            // at equilibrium
        } else {
            // Cooling
        }
    }

    return 0.0;
}

int main()
{
    MinMaxSensor in_sensor;
    MinMaxSensor out_sensor;


    for (int i = 0; i < 10000; i++) {
        in_sensor.set_current_state(get_in_temp(i));
        out_sensor.set_current_state(get_out_temp(i));
        cout << i << ": IN " << in_sensor << endl;
        cout << i << ": OUT " << out_sensor << endl;
        cout << i << ": PWM=" << compute_pwm_output(in_sensor, out_sensor) << endl;

        // automatic calibration between in and out sensors at equilibrium
        if (in_sensor.stable() && out_sensor.stable()) {
            // the system is at equilibrium (considering the defined threshold)
            float correction_offset = in_sensor.get_stable_average() - out_sensor.get_stable_average();

            // if correction_offset is negative then output is higher than input
            // => output value needs to be decreased by correction_offset (the sign of correction_offset will do)
            // if correction_offset is positive then input is higher than output
            // => output value needs to be increased by correction_offset (the sign of correction_offset will do as well)
            out_sensor.apply_correction_offset(correction_offset + global_offset);
            in_sensor.apply_correction_offset(global_offset);
        }
    }

    return 0;
}

