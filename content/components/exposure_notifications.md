---
description: "Instructions for setting up exposure notification listeners for ESPHome."
title: "Exposure Notification Listener"
params:
  seo:
    description: Instructions for setting up exposure notification listeners for ESPHome.
    image: exposure_notifications.png
---

The `exposure_notifications` component uses the {{< docref "/components/esp32_ble_tracker" >}} to discover
nearby COVID-19 exposure notification bluetooth messages sent by phones running the
[Google/Apple Exposure Notification service](https://www.google.com/covid19/exposurenotifications/).

```yaml
# Example configuration entry
esp32_ble_tracker:

exposure_notifications:
  on_exposure_notification:
    then:
      - lambda: |
          ESP_LOGD("main", "Got notification:");
          ESP_LOGD("main", "  RPI: %s", format_hex_pretty(x.rolling_proximity_identifier).c_str());
          ESP_LOGD("main", "  RSSI: %d", x.rssi);
```

## Configuration variables

- **on_exposure_notification** (*Optional*, [Automation](/automations)): An automation
  to run when an exposure notification bluetooth message is received.

  A variable `x` of type {{< apistruct "exposure_notifications::ExposureNotification" "exposure_notifications::ExposureNotification" >}} is passed to the automation.

An exposure notification payload contains:

- Rolling proximity identifier (RPI): A 16-byte long value used to identify a given device in a 10-minute window.
- Associated encrypted metadata (AEM): Additional encrypted metadata, like transmit power.

Because the GAEN framework is designed to prevent tracking an individual, this data can essentially
only be used to check whether a device with enabled exposure notifications is nearby (and to limited degree
also count them).

## Indicator of device with exposure notifications

The following configuration can be used as an indicator whether an exposure-notifications
enabled device is nearby. As long as an exposure notification has been received in the last
minute, the indicator will be on.

```yaml
esp32_ble_tracker:

switch:
  - platform: gpio
    pin: GPIOXX
    id: led

script:
  - id: start_led
    then:
      - switch.turn_on: led
      - delay: 1min
      - switch.turn_off: led

exposure_notifications:
  on_exposure_notification:
    then:
      - lambda: |
          ESP_LOGD("main", "Got notification:");
          ESP_LOGD("main", "  RPI: %s", format_hex_pretty(x.rolling_proximity_identifier).c_str());
          ESP_LOGD("main", "  RSSI: %d", x.rssi);

      # Stop existing timer so that turn_off doesn't get called
      - script.stop: start_led
      - script.execute: start_led
```

## See Also

- {{< docref "esp32_ble_tracker/" >}}
- {{< apiref "exposure_notifications/exposure_notifications.h" "exposure_notifications/exposure_notifications.h" >}}
