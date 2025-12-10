---
description: "Instructions for setting up the deep sleep support for minimizing power consumption on ESPs."
title: "Deep Sleep Component"
params:
  seo:
    description: Instructions for setting up the deep sleep support for minimizing power consumption on ESPs.
    image: hotel.svg
---

{{< anchor "deep_sleep-component" >}}

The `deep_sleep` component can be used to automatically enter a deep sleep mode on the
ESP8266/ESP32 after a certain amount of time. This is especially useful with nodes that operate
on batteries and therefore need to conserve as much energy as possible.

To use `deep_sleep` first specify how long the node should be active, i.e. how long it should
check sensor values and report them, using the `run_duration` option.

Next, tell the node how it should wakeup. On the ESP8266, you can only put the node into deep sleep
for a duration using `sleep_duration`, note that on the ESP8266 `GPIO16` must be connected to
the `RST` pin so that it will wake up again. On the ESP32, you additionally have the option
to wake up on any RTC pin (`GPIO0`, `GPIO2`, `GPIO4`, `GPIO12`, `GPIO13`, `GPIO14`,
`GPIO15`, `GPIO25`, `GPIO26`, `GPIO27`, `GPIO32`, `GPIO39`).

While in deep sleep mode, the node will not do any work and not respond to any network traffic,
even Over The Air updates. If the device's entities are appearing as **Unavailable** while your device is actively
sleeping, this component was likely added after the device was added to Home Assistant. To prevent this behavior,
you can remove and re-add the device in Home Assistant.

```yaml
# Example configuration entry
deep_sleep:
  run_duration: 10s
  sleep_duration: 10min
```

> [!NOTE]
> Some ESP8266s have an onboard USB chip (e.g. D1 mini) on the chips' control line that is connected to the RST pin. This enables the flasher to reboot the ESP when required. This may interfere with deep sleep on some devices and prevent the ESP from waking when it's powered through its USB connector. Powering the ESP from a separate 3.3V source connected to the 3.3V pin and GND will solve this issue. In these cases, using a USB to TTL adapter will allow you to log ESP activity.

## Configuration variables

- **run_duration** (*Optional*, [Time](/guides/configuration-types#time)): The time duration the node should be active, i.e. run code.

  Only on ESP32, instead of time, it is possible to specify run duration according to the wakeup reason from deep-sleep:

  - **default** (**Required**, [Time](/guides/configuration-types#time)): default run duration for timer wakeup and any unspecified wakeup reason.
  - **gpio_wakeup_reason** (*Optional*, [Time](/guides/configuration-types#time)): run duration if woken up by GPIO.
  - **touch_wakeup_reason** (*Optional*, [Time](/guides/configuration-types#time)): run duration if woken up by touch.

- **sleep_duration** (*Optional*, [Time](/guides/configuration-types#time)): The time duration to stay in deep sleep mode.
- **touch_wakeup** (*Optional*, boolean): Only on ESP32. Use a touch event to wakeup from deep sleep. To be able
  to wakeup from a touch event, [Binary Sensor](/components/binary_sensor/esp32_touch#esp32-touch-binary-sensor) must be configured properly.

- **wakeup_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): Only on ESP32. A pin to wake up to once
  in deep sleep mode. Use the inverted property to wake up to LOW signals.

- **wakeup_pin_mode** (*Optional*): Only on ESP32. Specify how to handle waking up from a `wakeup_pin` if
  the wakeup pin is already in the state with which it would wake up when attempting to enter deep sleep.
  See [ESP32 Wakeup Pin Mode](#deep_sleep-esp32_wakeup_pin_mode). Defaults to `IGNORE`

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

Advanced features:

- **esp32_ext1_wakeup** (*Optional*): Use the EXT1 wakeup source of the ESP32 to wake from deep sleep to
  wake up on multiple pins. This cannot be used together with wakeup pin.

  - **pins** (**Required**, list of pin numbers): The pins to wake up on.
  - **mode** (**Required**): The mode to use for the wakeup source. Must be one of:
    - `ANY_LOW`: wake up when any selected pin is LOW (ESP32‑C5/C6/C61/H2/P4/S2/S3 only)
    - `ALL_LOW`: wake up when all selected pins are LOW (ESP32 only)
    - `ANY_HIGH`: wake up when any selected pin is HIGH

> [!NOTE]
> Only one deep sleep component may be configured.

{{< anchor "deep_sleep-esp32_wakeup_pin_mode" >}}

## ESP32 Wakeup Pin Mode

On the ESP32, you have the option of waking up on any RTC pin. However, there's one scenario that you need
to tell ESPHome how to handle: What if the wakeup pin is already in the state with which it would wake up
when the deep sleep should start? There are three ways of handling this using the `wakeup_pin_mode` option:

- `IGNORE` (Default): Ignore the fact that we will immediately exit the deep sleep mode because the wakeup
  pin is already active.

- `KEEP_AWAKE`  : Keep the ESP32 awake while the wakeup pin is still active. Or in other words: defer the
  activation of the deep sleep until the wakeup pin is no longer active.

- `INVERT_WAKEUP`  : When deep sleep was set up to wake up on a HIGH signal, but the wakeup pin is already HIGH,
  then re-configure deep sleep to wake up on a LOW signal and vice versa. Useful in situations when you want to
  use observe the state changes of a pin using deep sleep and the ON/OFF values last longer.

## ESP32 Wakeup Cause

On the ESP32, the `esp_sleep_get_wakeup_cause()` function can be used to check which wakeup source has triggered
wakeup from sleep mode.

```yaml
sensor:
  - platform: template
    name: "Wakeup Cause"
    accuracy_decimals: 0
    lambda: return esp_sleep_get_wakeup_cause();
```

The following integers are the wakeup causes:

- **0** - `ESP_SLEEP_WAKEUP_UNDEFINED`  : In case of deep sleep, reset was not caused by exit from deep sleep
- **1** - `ESP_SLEEP_WAKEUP_ALL`  : Not a wakeup cause, used to disable all wakeup sources with esp_sleep_disable_wakeup_source
- **2** - `ESP_SLEEP_WAKEUP_EXT0`  : Wakeup caused by external signal using RTC_IO
- **3** - `ESP_SLEEP_WAKEUP_EXT1`  : Wakeup caused by external signal using RTC_CNTL
- **4** - `ESP_SLEEP_WAKEUP_TIMER`  : Wakeup caused by timer
- **5** - `ESP_SLEEP_WAKEUP_TOUCHPAD`  : Wakeup caused by touchpad
- **6** - `ESP_SLEEP_WAKEUP_ULP`  : Wakeup caused by ULP program
- **7** - `ESP_SLEEP_WAKEUP_GPIO`  : Wakeup caused by GPIO (light sleep only on ESP32, S2 and S3)
- **8** - `ESP_SLEEP_WAKEUP_UART`  : Wakeup caused by UART (light sleep only)
- **9** - `ESP_SLEEP_WAKEUP_WIFI`  : Wakeup caused by WIFI (light sleep only)
- **10** - `ESP_SLEEP_WAKEUP_COCPU`  : Wakeup caused by COCPU int
- **11** - `ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG`  : Wakeup caused by COCPU crash
- **12** - `ESP_SLEEP_WAKEUP_BT`  : Wakeup caused by BT (light sleep only)

{{< anchor "deep_sleep-enter_action" >}}

## `deep_sleep.enter` Action

This action makes the given deep sleep component enter deep sleep immediately.

```yaml
on_...:
  then:
    - deep_sleep.enter:
        id: deep_sleep_1
        sleep_duration: 20min

# Sleep duration is also templatable
on_...:
  then:
    - deep_sleep.enter:
        id: deep_sleep_1
        sleep_duration: !lambda "return 20 * 60 * 1000;"

# ESP32 can sleep until a specific time of day.
on_...:
  then:
    - deep_sleep.enter:
        id: deep_sleep_1
        until: "16:00:00"
        time_id: sntp_id
```

Configuration options:

- **sleep_duration** (*Optional*, [templatable](/automations/templates), [Time](/guides/configuration-types#time)): The time duration to stay in deep sleep mode. If a template is used, it should return a value in milliseconds.
- **until** (*Optional*, string): The time of day to wake up. Only on ESP32.
- **time_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the time component to use for the `until` option. Only on ESP32.

{{< anchor "deep_sleep-prevent_action" >}}

## `deep_sleep.prevent` Action

This action prevents the given deep sleep component from entering deep sleep.
Useful for keeping the ESP active during data transfer or OTA updating (See note below for more information).

```yaml
on_...:
  then:
    - deep_sleep.prevent: deep_sleep_1
```

> [!NOTE]
> For example, if you want to upload a binary via OTA with deep sleep mode it can be difficult to
> catch the ESP being active.
>
> You can use this automation to automatically prevent deep sleep when a MQTT message on the topic
> `livingroom/ota_mode` is received. Then, to do the OTA update, just
> use a MQTT client to publish a retained MQTT message described below. When the node wakes up again
> it will no longer enter deep sleep mode and you can upload your OTA update.
>
> Remember to turn "OTA mode" off again after the OTA update by sending a MQTT message with the payload
> `OFF`. To enter the deep sleep again after the OTA update send a message on the topic `livingroom/sleep_mode`
> with payload `ON`. Deep sleep will start immediately. Don't forget to delete the payload before the node
> wakes up again.
>
> ```yaml
> deep_sleep:
>   # ...
>   id: deep_sleep_1
> mqtt:
>   # ...
>   on_message:
>     - topic: livingroom/ota_mode
>       payload: 'ON'
>       then:
>         - deep_sleep.prevent: deep_sleep_1
>     - topic: livingroom/sleep_mode
>       payload: 'ON'
>       then:
>         - deep_sleep.enter: deep_sleep_1
> ```

{{< anchor "deep_sleep-allow_action" >}}

## `deep_sleep.allow` Action

This action allows the given deep sleep component to enter deep sleep, after previously being prevented.

```yaml
on_...:
  then:
    - deep_sleep.allow: deep_sleep_1
```

## See Also

- {{< docref "switch/shutdown" >}}
- [Automation](/automations)
- {{< apiref "deep_sleep/deep_sleep_component.h" "deep_sleep/deep_sleep_component.h" >}}
