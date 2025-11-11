---
description: "Instructions for setting up generic Alarm Control Panels in ESPHome."
title: "Alarm Control Panel Component"
params:
  seo:
    description: Instructions for setting up generic Alarm Control Panels in ESPHome.
    image: alarm-panel.svg
---

{{< anchor "config-alarm_control_panel" >}}

## Base Alarm Control Panel Configuration

```yaml
alarm_control_panel:
  - platform: ...
    name: Alarm Panel
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name of the alarm control panel. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the switch to use that name, you can set `name: None`.

- **on_state** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm changes state. See [`on_state` Trigger](#alarm_control_panel_on_state_trigger).

- **on_arming** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm state changes to `arming`. See [`on_arming` Trigger](#alarm_control_panel_on_arming_trigger).

- **on_pending** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm state changes to `pending`. See [`on_pending` Trigger](#alarm_control_panel_on_pending_trigger).

- **on_armed_home** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm state changes to `armed_home`. See [`on_armed_home` Trigger](#alarm_control_panel_on_armed_home_trigger).

- **on_armed_night** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm state changes to `armed_night`. See [`on_armed_night` Trigger](#alarm_control_panel_on_armed_night_trigger).

- **on_armed_away** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm state changes to `armed_away`. See [`on_armed_away` Trigger](#alarm_control_panel_on_armed_away_trigger).

- **on_triggered** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm triggers. See [`on_triggered` Trigger](#alarm_control_panel_on_triggered_trigger).

- **on_disarmed** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm state changes to `disarmed`. See [`on_disarmed` Trigger](#alarm_control_panel_on_disarmed_trigger).

- **on_cleared** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the alarm clears. See [`on_cleared` Trigger](#alarm_control_panel_on_cleared_trigger).

- **on_ready** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the logical 'and' of all the zone sensors change state. See [`on_ready` Trigger](#alarm_control_panel_on_ready_trigger).

- **on_chime** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when a zone has been marked as chime in the configuration, and it changes from closed to open.
  See [`on_chime` Trigger](#alarm_control_panel_on_chime_trigger).

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).
- If MQTT enabled, all other options from [MQTT Component](/components/mqtt#config-mqtt-component).

## Automation

{{< anchor "alarm_control_panel_on_state_trigger" >}}

### `on_state` Trigger

This trigger is activated each time the alarm changes state.

```yaml
alarm_control_panel:
  # ...
  on_state:
    then:
      - logger.log: "Alarm Panel State Changed!"
```

{{< anchor "alarm_control_panel_on_pending_trigger" >}}

### `on_pending` Trigger

This trigger is activated when the alarm changes to pending state.

```yaml
alarm_control_panel:
  # ...
  on_pending:
    then:
      - logger.log: "Alarm Pending!"
```

{{< anchor "alarm_control_panel_on_arming_trigger" >}}

### `on_arming` Trigger

This trigger is activated when the alarm changes to arming state.

```yaml
alarm_control_panel:
  # ...
  on_arming:
    then:
      - logger.log: "Alarm Arming!"
```

{{< anchor "alarm_control_panel_on_armed_home_trigger" >}}

### `on_armed_home` Trigger

This trigger is activated when the alarm changes to armed_home state.

```yaml
alarm_control_panel:
  # ...
  on_armed_home:
    then:
      - logger.log: "Alarm armed_home!"
```

{{< anchor "alarm_control_panel_on_armed_night_trigger" >}}

### `on_armed_night` Trigger

This trigger is activated when the alarm changes to armed_night state.

```yaml
alarm_control_panel:
  # ...
  on_armed_night:
    then:
      - logger.log: "Alarm armed_night!"
```

{{< anchor "alarm_control_panel_on_armed_away_trigger" >}}

### `on_armed_away` Trigger

This trigger is activated when the alarm changes to armed_away state.

```yaml
alarm_control_panel:
  # ...
  on_armed_away:
    then:
      - logger.log: "Alarm armed_away!"
```

{{< anchor "alarm_control_panel_on_triggered_trigger" >}}

### `on_triggered` Trigger

This trigger is activated when the alarm changes to triggered state.

```yaml
alarm_control_panel:
  # ...
  on_triggered:
    then:
      - logger.log: "Alarm Triggered!"
```

{{< anchor "alarm_control_panel_on_cleared_trigger" >}}

### `on_cleared` Trigger

This trigger is activated when the alarm changes from triggered back to either the previous armed state or disarmed.

```yaml
alarm_control_panel:
  # ...
  on_cleared:
    then:
      - logger.log: "Alarm Cleared!"
```

{{< anchor "alarm_control_panel_on_disarmed_trigger" >}}

### `on_disarmed` Trigger

This trigger is activated when the alarm changes from to disarmed.

```yaml
alarm_control_panel:
  # ...
  on_disarmed:
    then:
      - logger.log: "Alarm Disarmed!"
```

{{< anchor "alarm_control_panel_on_ready_trigger" >}}

### `on_ready` Trigger

This trigger is activated when the logical 'and' of all the alarm sensors change state. This is useful for implementing
"alarm ready" LEDs. Once this trigger is called, you can get the ready state by calling get_all_sensors_ready() in a
lambda block.

```yaml
alarm_control_panel:
  # ...
  on_disarmed:
    then:
      - lambda: !lambda |-
          ESP_LOGI("AlarmPanel", "Sensor ready change to: %s", ((id(acp1).get_all_sensors_ready()) ? (const char *) "True" : (const char *) "False"));
```

{{< anchor "alarm_control_panel_on_chime_trigger" >}}

### `on_chime` Trigger

This trigger is activated when a zone sensor marked with chime:true changes from closed to open. This is useful for
implementing keypad chimes when a zone opens.

```yaml
alarm_control_panel:
  # ...
  on_chime:
    then:
      - logger.log: "Alarm Chime!"
```

{{< anchor "alarm_control_panel_arm_away_action" >}}

### `arm_away` Action

This action arms the alarm in away mode. The `code` is required when *requires_code_to_arm* is *true*.

```yaml
on_...:
  then:
    - alarm_control_panel.arm_away:
        id: acp1
        code: "1234"
```

{{< anchor "alarm_control_panel_arm_home_action" >}}

### `arm_home` Action

This action arms the alarm in home mode. The `code` is required when *requires_code_to_arm* is *true*.

```yaml
on_...:
  then:
    - alarm_control_panel.arm_home:
        id: acp1
        code: "1234"
```

{{< anchor "alarm_control_panel_arm_night_action" >}}

### `arm_night` Action

This action arms the alarm in night mode. The `code` is required when *requires_code_to_arm* is *true*.

```yaml
on_...:
  then:
    - alarm_control_panel.arm_night:
        id: acp1
        code: "1234"
```

{{< anchor "alarm_control_panel_disarm_action" >}}

### `disarm` Action

This action disarms the alarm. The `code` is required when *codes* is not empty.

```yaml
on_...:
  then:
    - alarm_control_panel.disarm:
        id: acp1
        code: "1234"
```

{{< anchor "alarm_control_panel_pending_action" >}}

### `pending` Action

This action puts the alarm in pending state (the state before triggered after *pending_time*).

```yaml
on_...:
  then:
    - alarm_control_panel.pending: acp1
```

{{< anchor "alarm_control_panel_triggered_action" >}}

### `triggered` Action

This action puts the alarm in triggered state.

```yaml
on_...:
  then:
    - alarm_control_panel.triggered: acp1
```

{{< anchor "alarm_control_panel_is_armed_condition" >}}

### `is_armed` Condition

This [Condition](/automations/actions#all-conditions) checks if the alarm control panel is armed.

```yaml
on_...:
  if:
    condition:
      alarm_control_panel.is_armed: acp1
```

{{< anchor "alarm_control_panel_lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call the following methods:

- `arm_away(code)`
- `arm_home(code)`
- `arm_night(code)`
- `disarm(code)`
- `get_all_sensors_ready()`

```cpp
id(acp1).arm_away();
id(acp1).arm_home();
id(acp1).arm_night();
id(acp1).disarm(std::string("1234"));
bool all_sensors_ready = id(acp1).get_all_sensors_ready();
```

## Platforms

## See Also

- {{< docref "/components/binary_sensor" >}}
- {{< apiref "alarm_control_panel/alarm_control_panel.h" "alarm_control_panel/alarm_control_panel.h" >}}
