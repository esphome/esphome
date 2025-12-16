---
description: "Instructions for setting up template Alarm Control Panels in ESPHome."
title: "Template Alarm Control Panel"
params:
  seo:
    description: Instructions for setting up template Alarm Control Panels in ESPHome.
    image: description.svg
---

The `template` alarm control panel platform allows you to turn your binary sensors into a state machine
managed alarm control panel.

```yaml
# Example configuration entry
alarm_control_panel:
  - platform: template
    name: Alarm Panel
    codes:
      - "1234"
    binary_sensors:
      - input: zone_1
      - input: zone_2
        bypass_armed_home: true
```

## Configuration variables

- **codes** (*Optional*, list of string): A list of codes for disarming the alarm, if *requires_code_to_arm* set to true
  then for arming the alarm too.
- **requires_code_to_arm** (*Optional*, boolean): Code required for arming the alarm, *codes* must be provided.
- **arming_away_time** (*Optional*, [Time](/guides/configuration-types#time)): The exit delay before the alarm is armed to away mode.
  Defaults to `0s`.
- **arming_home_time** (*Optional*, [Time](/guides/configuration-types#time)): The exit delay before the alarm is armed to home mode.
- **arming_night_time** (*Optional*, [Time](/guides/configuration-types#time)): The exit delay before the alarm is armed to night mode.
- **pending_time** (*Optional*, [Time](/guides/configuration-types#time)): The entry delay before the alarm is triggered. Defaults to `0s`.
- **trigger_time** (*Optional*, [Time](/guides/configuration-types#time)): The time after a triggered alarm before resetting to previous
  state if the sensors are cleared/off. Defaults to `0s`.
- **binary_sensors** (*Optional*, *list*): A list of binary sensors the panel should use. Each consists of:

  - **input** (**Required**, string): The id of the binary sensor component
  - **bypass_armed_home** (*Optional*, boolean): This binary sensor will not trigger the alarm when in
    `armed_home` state.
  - **bypass_armed_night** (*Optional*, boolean): This binary sensor will not trigger the alarm when in `armed_night`
    state.
  - **bypass_auto** (*Optional*, boolean): This binary sensor will be automatically bypassed if left on/open at the
    time of arming.
  - **trigger_mode** (*Optional*, string): Sets the trigger mode for this sensor. One of `delayed`, `instant`,
    `instant_always`, or `delayed_follower`. (`delayed` is the default if not specified)
  - **chime** (*Optional*, boolean): When set `true`, the chime callback will be called whenever the sensor goes from
    closed to open. (`false` is the default if not specified)

- **restore_mode** (*Optional*, enum):

  - `ALWAYS_DISARMED` (Default): Always start in `disarmed` state.
  - `RESTORE_DEFAULT_DISARMED`  : Restore state or default to `disarmed` state if no saved state was found.

- All other options from [Alarm Control Panel](/components/alarm_control_panel#config-alarm_control_panel)

> [!NOTE]
> If `binary_sensors` is omitted then you're expected to trigger the alarm using
> [`pending` Action](/components/alarm_control_panel#alarm_control_panel_pending_action) or [`triggered` Action](/components/alarm_control_panel#alarm_control_panel_triggered_action).

{{< anchor "template_alarm_control_panel-trigger_modes" >}}

## Trigger Modes

Each binary sensor "zone" supports 4 trigger modes. The modes are:

- delayed
- instant
- instant_always
- delayed_follower

The `delayed` trigger mode is typically specified for exterior doors where entry is required to access an alarm keypad
or other arm/disarm method. If the alarm panel is armed, and a zone set to `delayed` is "faulted" (i.e. the zone state
is `true`  ) the alarm state will change from the `armed` state to the `pending` state. During the `pending` state, the
user has a preset time to disarm the alarm before it changes to the `triggered` state. This is the default trigger mode
if not specified.

The `instant` trigger mode is typically used for exterior zones (e.g. windows, and glass break detectors). If the alarm
control panel is armed, a fault on this type of zone will cause the alarm to go from the `armed` state directly to the
`triggered` state.

The `instant_always` trigger mode is typically used for tamper inputs. Irrespective of whether the alarm control panel
is armed, a fault will always cause the alarm to go directly to the `triggered` state.

The `delayed_follower` trigger mode is typically specified for interior passive infrared (PIR) or microwave sensors. One
of two things happen when a `delayed_follower` zone is faulted:

1. When the alarm panel is in the armed state, a fault on a zone with `delayed_follower` specified will cause the alarm
   control panel to go directly to the `triggered` state.

1. When the alarm panel is in the pending state, a fault on a zone with `delayed_follower` specified will remain in
   the `pending` state.

The `delayed_follower` trigger mode offers better protection if someone enters a premises via an unprotected window
or door. If there is a PIR guarding the main hallway, it will cause an instant trigger of the alarm panel as someone
entered the premises in an unusual manner. Likewise, if someone enters the premises through a door set to the `delayed`
trigger mode, and then triggers the PIR, the alarm will stay in the `pending` state until either they disarm the alarm,
or the pending timer expires.

{{< anchor "template_alarm_control_panel-state_flow" >}}

## State Flow

1. The alarm starts in `DISARMED` state
1. When the `arm_...` method is invoked

    - `arming_..._time` is greater than 0 the state is `ARMING`
    - `arming_..._time` is 0 or after the delay the state is `ARMED_...`

1. When the alarm is tripped by a sensor state changing to `on` or `alarm_control_panel_pending_action` invoked

1. If `trigger_mode` is set to `delayed`  :

    - `pending_time` greater than 0 the state is `PENDING`
    - `pending_time` is 0 or after the `pending_time` delay the state is `TRIGGERED`

1. If `trigger_mode` is set to `instant` or `instant_always`  :

    - The state is set to `TRIGGERED`

1. If the `trigger_mode` is set to `interior_follower`:

   - If the current state is `ARMED_...` the state will be set to `TRIGGERED`
   - If the current state is `PENDING` then nothing will happen and it will stay in the `PENDING` state.

1. If `trigger_time` greater than 0 and no sensors are `on` after `trigger_time` delay
   the state returns to `ARM_...`

> [!NOTE]
> Although the interface supports all arming modes only `away`, `home` and `night` have been implemented for now.
> `arm_...` is for either `arm_away` or `arm_home`
> `arming_..._time` is for either `arming_away_time`, `arming_home_time`, or `arming_night_time`
> `ARMED_...` is for either `ARMED_AWAY`, `ARMED_HOME`, or `ARMED_NIGHT`

## Example

```yaml
alarm_control_panel:
  platform: template
  name: Alarm Panel
  id: acp1
  codes:
    - "1234"
  requires_code_to_arm: true
  arming_away_time: 30s
  arming_home_time: 5s
  pending_time: 30s
  trigger_time: 5min
  binary_sensors:
    - input: zone_1
      chime: true
      trigger_mode: delayed
    - input: zone_2
      chime: true
      trigger_mode: delayed
    - input: zone_3
      bypass_armed_home: true
      trigger_mode: delayed_follower
    - input: zone_3_tamper
      trigger_mode: instant_always
    - input: zone_4
      trigger_mode: instant
    - input: ha_test
  on_state:
    then:
      - lambda: !lambda |-
          ESP_LOGD("TEST", "State change %s", alarm_control_panel_state_to_string(id(acp1)->get_state()));
  on_triggered:
    then:
      - switch.turn_on: siren
  on_cleared:
    then:
      - switch.turn_off: siren
  on_ready:
    then:
     - lambda: !lambda |-
         ESP_LOGD("TEST", "Sensor ready change to: %s",
           (id(acp1).get_all_sensors_ready())) ? (const char *) "True" : (const char *) "False");
  on_chime:
    then:
     - lambda: !lambda |-
         ESP_LOGD("TEST", "Zone with chime mode set opened");

binary_sensor:
  - platform: gpio
    id: zone_1
    name: Zone 1
    device_class: door
    pin:
      number: GPIOXX
      mode: INPUT_PULLUP
      inverted: True
  - platform: gpio
    id: zone_2
    name: Zone 2
    device_class: door
    pin:
      number: GPIOXX
      mode: INPUT_PULLUP
      inverted: True
  - platform: gpio
    id: zone_3
    name: Zone 3
    device_class: motion
    pin:
      number: GPIOXX
      mode: INPUT_PULLUP
      inverted: True
  - platform: gpio
    id: zone_3_tamper
    name: Zone 3 Tamper
    device_class: tamper
    pin:
      number: GPIOXX
      mode: INPUT_PULLUP
      inverted: True
  - platform: gpio
    id: zone_4
    name: Zone 4
    device_class: door
    pin:
      number: GPIOXX
      mode: INPUT_PULLUP
      inverted: True
  - platform: homeassistant
    id: ha_test
    name: HA Test
    entity_id: input_boolean.test_switch

switch:
  - platform: gpio
    id: siren
    name: Siren
    icon: mdi:alarm-bell
    pin: GPIOXX
```

## See Also

- {{< docref "index/" >}}
- {{< docref "/components/binary_sensor" >}}
- {{< apiref "template/alarm_control_panel/template_alarm_control_panel.h"
      "template/alarm_control_panel/template_alarm_control_panel.h" >}}
