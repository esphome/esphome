---
description: "Instructions for hacking your power meter at home to measure your power usage."
title: "Non-Invasive Power Meter"
params:
  seo:
    description: Instructions for hacking your power meter at home to measure your power usage.
    image: power_meter.jpg
---

So an essential part of making your home smart is knowing how much power it uses over the day. Tracking this can be
difficult, often you need to install a completely new power meter which can often cost a bunch of money. However,
quite a few power meters have a red LED on the front that blinks every time that one Wh has been used.

The simple idea therefore is: Why don't we just abuse that functionality to make the power-meter IoT enabled? We just
have to hook up a simple photoresistor in front of that aforementioned LED and track the number of pulses we receive.
Then using ESPHome we can instantly have the power meter show up in Home Assistant 🎉

Hooking it all up is quite easy: Just buy a suitable photoresistor (make sure the wave length approximately matches
the one from your power meter). Then connect it using a simple variable resistor divider (see
[this article](https://blog.udemy.com/arduino-ldr/) for inspiration). And... that should already be it :)

{{< img src="power_meter-header.jpg" alt="Image" width="80.0%" class="align-center" >}}

> [!NOTE]
> Some energy meters have an exposed [S0 port](https://en.wikipedia.org/wiki/S_interface) (which essentially just is
> a switch that closes), if that is the case the photodiode can be replaced with the following connection.
>
> ```text
> S0 ------------ VCC
> S0 --+-- 10k -- GND
> .    |
> .    +--------- GPIO12
> ```

For ESPHome, you can then use the {{< docref "/components/sensor/pulse_meter" "pulse meter sensor" >}}
using below configuration:

```yaml
sensor:
  - platform: pulse_meter
    name: 'Power'
    id: sensor_pulse_meter # Optional ID, necessary if you want to calculate the total daily energy
    unit_of_measurement: 'W'
    device_class: power
    state_class: measurement
    accuracy_decimals: 0
    pin: GPIO12
    filters:
      - multiply: 6 # (60s / impulse constant) * (1000W / 1kW)
```

Adjust `GPIO12` to match your set up of course. The output from the pulse counter sensor is in pulses/min and we
also know that 10000 pulses from the LED should equal 1kWh of power usage. Thus, rearranging the expression yields
a proportional factor of 6 from pulses/min to *W*.

> [!NOTE]
> The `pulse_meter` sensor sends an update every time a pulse is detected. This can quickly lead to sub-second updates
> which can be a bit much for Home Assistant to handle. To avoid this, you can use the `throttle_average` filter to
> only send updates up to a desired interval:
>
> ```yaml
> sensor:
>   - platform: pulse_meter
>     # ...
>     filters:
>       - throttle_average: 10s
>       - filter_out: NaN
> ```

> [!NOTE]
> The `pulse_meter` sensor has an internal filter that is used to debounce the input signal. This filter is set to
> `13us` by default. To increase the safety margin, you can increase this value. For this you need to know the
> *minimum pulse width* that you can expect to see from your power meter within the expected load range.
>
> The minimum pulse width threshold is determined by considering the upper limit of the *load* the meter is designed
> to handle, as well as the meter's *impulse constant* (x pulses / kWh). Here's the calculation involved:
>
> - **Load Limit in Watts**: Establish the *upper load limit* that the meter is designed to measure. For example, if
> - the limit is 16 kW (16,000 Watts), this becomes a reference point.
>
> - **Pulse Rate Calculation**: Determine the pulse rate corresponding to this load limit. For this we need to know
> - the impulse constant. In our example, the power meter has an impulse constant of 10000 pulses/kWh, resulting in
> - 160000 pulses per hour at the maximum 16 kW load, i.e. 44.4 pulses/second.
>
> - **Minimum Pulse Width Calculation**: Use the pulse rate to calculate the minimum pulse width threshold. In our
> - example, with a maximum pulse rate of 44.4/sec, the minimum pulse width we expect to see is approximately 22.5
> - milliseconds. Choose a slightly smaller value than this to avoid missing pulses.
>
> ```yaml
> sensor:
>   - platform: pulse_meter
>     # ...
>     internal_filter: 20ms
> ```

If a technician shows up and he looks confused about what the heck you have done to your power meter, tell them
about ESPHome 😉

## Counting total generated energy

When the total sensor is configured, `pulse_meter` also reports the total number of pulses measured. When used on a
power meter, this can be used to measure the total transmitted energy in kWh.

```yaml
# Example configuration entry
sensor:
  - platform: pulse_meter
  # ...
    total:
      name: "Electricity Total"
      unit_of_measurement: "kWh"
      device_class: energy
      state_class: total_increasing
      accuracy_decimals: 3
      filters:
        - multiply: 0.0001  # (1/10000 pulses per kWh)
        # - throttle_average: 10s
        # - filter_out: NaN
```

## (Re)Setting the total energy value

Using this action, you are able to reset/set the total pulse count. This can be useful if you would like the `total`
sensor to match what you see on your meter you are trying to match.

```yaml
api:
  actions:
    - action: set_total
      variables:
        new_total: int
      then:
        - pulse_meter.set_total_pulses:
            id: sensor_pulse_meter
            value: !lambda 'return new_total * 1000;'
```

## Tracking Total Daily Energy

Additionally you can also calculate the total daily energy generated, for which you can use the
{{< docref "/components/sensor/total_daily_energy" "Total Daily Energy" >}} using below configuration:

```yaml
sensor:
 #(...)
  - platform: total_daily_energy
    name: 'Total Daily Energy'
    id: sensor_total_daily_energy
    power_id: sensor_pulse_meter
    unit_of_measurement: 'kWh'
    state_class: total_increasing
    device_class: energy
    accuracy_decimals: 3
    filters:
      # Multiplication factor from W to kW is 0.001
      - multiply: 0.001

time:
  - platform: homeassistant
    id: homeassistant_time
```

While you can in theory also do this with the home assistant
[integration](https://www.home-assistant.io/integrations/integration/) integration, the benefit of this is that
it continues to integrate the power during times home assistant is unable to work with values, i.e. during updates,
restarts and so on.

## See Also

- {{< docref "/components/sensor/pulse_counter" >}}
- {{< docref "/components/sensor/pulse_meter" >}}
- {{< docref "/components/sensor/total_daily_energy" >}}
- {{< docref "/components/time/homeassistant" >}}
- [Home Assistant Glow 🌟](https://github.com/klaasnicolaas/home-assistant-glow/).
