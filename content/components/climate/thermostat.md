---
description: "Instructions for setting up Thermostat climate controllers with ESPHome."
title: "Thermostat Climate Controller"
params:
  seo:
    description: Instructions for setting up Thermostat climate controllers with ESPHome.
    image: air-conditioner.svg
---

The `thermostat` climate platform allows you to control a climate control system in much the same manner as a
physical thermostat. Its operation is similar to the {{< docref "bang_bang" "Bang-Bang" >}} controller; a sensor measures a value
(the air temperature) and the controller will try to keep this value within a range defined by the set point(s). To do this,
the controller can activate devices like a heating unit and/or a cooling unit to change the value observed by the sensor.
When configured for both heating and cooling, it is essentially two {{< docref "bang_bang" "Bang-Bang" >}} controllers in one; it
differs, however, in that interaction with the thermostat component is nearly identical to that of a real thermostat.

This component can operate in one of two ways:

- **Single-point**: A single threshold (set point) is defined; cooling may be activated when the observed temperature
  exceeds the set point **or** heating may be activated when the observed temperature drops below the set point; that is,
  the controller can only raise the temperature or lower the temperature. It cannot do both in this mode.

- **Dual-point**: Two thresholds (set points) are defined; cooling is activated when the observed temperature exceeds the
  upper set point while heating is activated when the observed temperature drops below the lower set point; in other words,
  the controller is able to both raise and lower the temperature as required.

This component/controller automatically determines which mode it should operate in based on what [actions](/automations/actions#all-actions)
are configured -- more on this in a moment. Two parameters define the set points; they are `target_temperature_low` and
`target_temperature_high`. In single-point mode, however, only one is used. The set point(s) may be adjusted through the
front-end user interface. The screenshot below illustrates a thermostat controller in dual-point mode, where two set points
are available.

{{< img src="climate-ui.png" alt="Image" caption="Dual-setpoint climate UI" width="60.0%" class="align-center" >}}

This component works by triggering a number of [actions](/automations/actions#all-actions) as required to keep the observed
temperature above/below/within the target range as defined by the set point(s). In general, when the observed temperature
drops below `target_temperature_low` the controller will trigger the `heat_action` to activate heating. When the observed
temperature exceeds `target_temperature_high`  the controller will trigger the `cool_action` or the `fan_only_action`
(as determined by the climate mode) to activate cooling. When the temperature has reached a point within the desired range, the
controller will trigger the `idle_action` to stop heating/cooling. Please see the next section for more detail.

A number of fan control modes are built into the climate/thermostat interface in Home Assistant; this component may also be
configured to trigger [actions](/automations/actions#all-actions) based on the entire range (at the time this document was written) of fan
modes that Home Assistant offers.

```yaml
# Example dual-point configuration entry
climate:
  - platform: thermostat
    name: "Thermostat Climate Controller"
    sensor: my_temperature_sensor
    min_cooling_off_time: 300s
    min_cooling_run_time: 300s
    min_heating_off_time: 300s
    min_heating_run_time: 300s
    min_idle_time: 30s
    cool_action:
      - switch.turn_on: air_cond
    heat_action:
      - switch.turn_on: heater
    idle_action:
      - switch.turn_off: air_cond
      - switch.turn_off: heater
    default_preset: Home
    preset:
      - name: Home
        default_target_temperature_low: 20 °C
        default_target_temperature_high: 22 °C
```

```yaml
# Example single-point configuration entry (for heating only)
climate:
  - platform: thermostat
    name: "Thermostat Climate Controller"
    sensor: my_temperature_sensor
    min_heating_off_time: 300s
    min_heating_run_time: 300s
    min_idle_time: 30s
    heat_action:
      - switch.turn_on: heater
    idle_action:
      - switch.turn_off: heater
    default_preset: Home
    preset:
      - name: Home
        default_target_temperature_low: 20 °C
```

```yaml
# Example single-point configuration entry (for cooling only)
climate:
  - platform: thermostat
    name: "Thermostat Climate Controller"
    sensor: my_temperature_sensor
    min_cooling_off_time: 300s
    min_cooling_run_time: 300s
    min_idle_time: 30s
    cool_action:
      - switch.turn_on: air_cond
    idle_action:
      - switch.turn_off: air_cond
    default_preset: Home
    preset:
      - name: Home
        default_target_temperature_high: 22 °C
```

## Controller Behavior and Hysteresis

In addition to the set points, hysteresis values determine how far the temperature may vary from the set point value(s)
before an [action](/automations/actions#all-actions) (cooling, heating, etc.) is triggered. They each default to 0.5 °C. They are:

- `cool_deadband`  : The minimum temperature differential (temperature above the set point) before **engaging** cooling
- `cool_overrun`  : The minimum temperature differential (cooling beyond the set point) before **disengaging** cooling
- `heat_deadband`  : The minimum temperature differential (temperature below the set point) before **engaging** heat
- `heat_overrun`  : The minimum temperature differential (heating beyond the set point) before **disengaging** heat

A question that often surfaces about this component is, "What is the expected behavior?" Let's quickly discuss
*exactly when* the configured actions are called by the controller.

Consider the low set point (the one that typically activates heating) for a moment, and assume it is set to a common room
temperature of 22 °C. Let's assume `heat_deadband` is set to 0.4 °C while `heat_overrun` is set to 0.6 °C. In this case,
the controller will allow the temperature to drop as low as the set point's value (22 °C) *minus* the `heat_deadband`
value (0.4 °C), or 21.6 °C, before calling `heat_action` to activate heating.

After heating has been activated, it will remain active until the observed temperature reaches the set point (22 °C) *plus*
the `heat_overrun` value (0.6 °C), or 22.6 °C. Once this temperature is reached, `idle_action` will be called to deactivate
heating.

The same behavior applies to the high set point, although the behavior is reversed in a sense; given an upper set point of
23 °C, `cool_deadband` set to 0.3 °C and `cool_overrun` set to 0.7 °C, `cool_action` would be called at 23.3 °C and
`idle_action` would not be called until the temperature is reduced to 22.3 °C.

## Important Terminology

Before we get into more configuration detail, let's take a step back and talk about the word "action"; we
need to carefully consider the context of the word in the upcoming section, as it has a double meaning and
will otherwise lead to some ambiguity.

- **ESPHome Action**: A task the ESPHome application performs as requested, such as
  turning on a switch. See [Action](/automations/actions#all-actions).

- **Climate Action**: What the climate device is actively doing
- **Climate Mode**: What the climate device should (or should not) do

We'll call out which definition "action" we are referring to as we describe them below -- read carefully!

With respect to climate control, it is important to understand the subtle difference between the terms
"action" and "mode" as they *are not the same thing*:

Examples:

- **Heat Mode**: The climate device may heat but may **not** cool.
- **Heat Action**: The climate device is *actively distributing heated air* into the dwelling.

Got all that? Great. Let's take a closer look at some configuration.

## Configuration Variables

The thermostat controller uses the sensor to determine whether it should heat or cool.

- **sensor** (**Required**, [ID](/guides/configuration-types#id)): The sensor that is used to measure the current temperature.
- **humidity_sensor** (*Optional*, [ID](/guides/configuration-types#id)): If specified, this sensor is used to measure
  the current humidity. This may be used for humidity control; see [Humidity Control Actions](#humidity-control-actions).

### Heating and Cooling Actions

These are triggered when the climate control **action** is changed by the thermostat controller. Here,
"action" takes on both meanings described above, as these are both climate actions *and* ESPHome
[actions](/automations/actions#all-actions). These should be used to activate heating, cooling, etc. devices.

- **idle_action** (**Required**, [Action](/automations/actions#all-actions)): The action to call when
  the climate device should enter its idle state (not cooling, not heating).

- **heat_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device should enter heating mode to increase the current temperature.

- **supplemental_heating_action** (*Optional*, [Action](/automations/actions#all-actions)): The action
  to call when the climate device should activate supplemental heating to (more aggressively)
  increase the current temperature. *This action is called repeatedly at an interval defined by*
  `max_heating_run_time` *(see below).*

- **cool_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device should enter cooling mode to decrease the current temperature.

- **supplemental_cooling_action** (*Optional*, [Action](/automations/actions#all-actions)): The action
  to call when the climate device should activate supplemental cooling to (more aggressively)
  decrease the current temperature. *This action is called repeatedly at an interval defined by*
  `max_cooling_run_time` *(see below).*

- **dry_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device should perform its drying (dehumidification) action. The thermostat
  controller does not trigger this action; it is invoked by `dry_mode` (see below).

- **fan_only_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device should activate its fan only (but does not heat or cool). When `fan_only_cooling`
  is set to `false`, the thermostat controller immediately triggers this action when set to
  `fan_only_mode`  ; however, when `fan_only_cooling` is set to `true`, this action is called
  based on the upper target temperature (similar to `cool_action` above).

- All other options from [Climate](/components/climate#config-climate).

**At least one of** `cool_action`, `fan_only_action`, `heat_action`, **and** `dry_action`
**must be specified.**

If only one of `cool_action`, `fan_only_action`, `heat_action`, and `dry_action` is specified,
the controller will configure itself to operate in single-point mode and, as such, Home Assistant will
display the single-point climate user interface for the device.

### Heating and Cooling Modes

These are triggered when the climate control **mode** is changed. Note the absence of "action" in the
parameter name here -- these are still ESPHome [actions](/automations/actions#all-actions), however they are *not*
climate actions. Instead, they are climate *modes*. These [actions](/automations/actions#all-actions) are useful
in that they could be used, for example, to toggle a group of LEDs on and/or off to provide a visual
indication of the current climate mode.

- **heat_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into heat mode (it may heat as required, but not cool).

- **cool_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into cool mode (it may cool as required, but not heat).

- **dry_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into dry mode (for dehumidification).

- **fan_only_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into fan-only mode (it may not heat or cool, but will activate
  its fan either immediately or, when `fan_only_cooling` is `true`, as needed based on the upper
  target temperature value).

- **heat_cool_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into "heat/cool" mode (it may both cool and heat as required).

- **auto_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into "auto" mode (it may both cool and heat as required). This mode is
  different from `heat_cool_mode` (above) in that it takes control of the set points away from the user;
  it's generally intended for exclusive use by automations.

- **off_mode** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the climate device is placed into "off" mode (it is completely disabled).

**The above actions are not to be used to activate cooling or heating devices!**
See the previous section for those.

### Fan Mode Actions

These are triggered when the climate control fan mode is changed. These are ESPHome [actions](/automations/actions#all-actions).
These should be used to control the fan only, if available.

- **fan_mode_auto_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should be set to "auto" mode (the fan is controlled by the climate control system as required).

- **fan_mode_on_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should run continuously.

- **fan_mode_off_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should never run.

- **fan_mode_low_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should run at its minimum speed.

- **fan_mode_medium_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should run at an intermediate speed.

- **fan_mode_high_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should run at its maximum speed.

- **fan_mode_middle_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should direct its airflow at an intermediate area.

- **fan_mode_focus_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should direct its airflow at a specific area.

- **fan_mode_diffuse_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should direct its airflow over a broad area.

- **fan_mode_quiet_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should run at quiet speed.

### Swing Mode Actions

These are triggered when the climate control swing mode is changed. These are ESPHome [actions](/automations/actions#all-actions).
These should be used to control the fan only, if available.

- **swing_off_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan should
  remain in a stationary position.

- **swing_horizontal_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should oscillate in a horizontal direction.

- **swing_vertical_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should oscillate in a vertical direction.

- **swing_both_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the fan
  should oscillate in horizontal and vertical directions.

### Humidity Control Actions

These are triggered when the humidity control action is changed by the thermostat controller. It can trigger actions
to activate humidification **or** dehumidification.

- **humidity_control_dehumidify_action** (*Optional*, [Action](/automations/actions#config-action)): The action to call when
  dehumidification is required.

- **humidity_control_humidify_action** (*Optional*, [Action](/automations/actions#config-action)): The action to call when
  humidification is required.

- **humidity_control_off_action** (*Optional*, [Action](/automations/actions#config-action)): The action to call when
  (de)humidification should stop. This action is **required** when either of the above actions are configured.

## Advanced Configuration/Behavior

### Set Point Options/Behavior

- **set_point_minimum_differential** (*Optional*, float): For dual-point/dual-function systems, the minimum
  required temperature difference between the heat and cool set points. Defaults to 0.5 °C.

- **supplemental_cooling_delta** (*Required with* `supplemental_cooling_action`, float): When the temperature
  difference between the upper set point and the current temperature exceeds this value,
  `supplemental_cooling_action` will be called immediately.

- **supplemental_heating_delta** (*Required with* `supplemental_heating_action`, float): When the temperature
  difference between the lower set point and the current temperature exceeds this value,
  `supplemental_heating_action` will be called immediately.

{{< anchor "thermostat-preset" >}}

### Presets

Presets allow you to define a combination of set points, climate, fan, and swing modes that can be recalled from
the front end (Home Assistant) as a single operation for quick and easy access. This can simplify the user
experience and automation.

- **preset**: (*Optional*, list)

  - **name** (**Required**, string): Name of the preset. If this is one of the *standard* presets (`eco`, `away`,
    `boost`, `comfort`, `home`, `sleep`, or `activity`  ) it is considered a *standard* preset. Any other
    string will make the preset a *custom* preset. *Standard* and *custom* presets are functionally equivalent,
    the only difference is that when switching the mode via [climate.control Action](/components/climate#climate-control_action)
    you will need to use the `preset` or `custom_preset` property as appropriate. The Home Assistant
    `climate.set_preset_mode` service treats them identically

  - **default_target_temperature_low** (*Optional*, float): The default low target temperature when switching to
    this preset

  - **default_target_temperature_high** (*Optional*, float): The default high target temperature when switching
    to this preset.

  - **mode** (*Optional*, climate mode): The mode the thermostat should switch to when this preset is activated.
    If not specified, the thermostat's mode will remain unchanged when the preset is activated. One of:

    - `heat_cool`
    - `cool`
    - `heat`
    - `dry`
    - `fan_only`
    - `auto`
  - **fan_mode** (*Optional*, climate fan mode): The fan mode the thermostat should switch to when this preset
    is activated. If not specified, the thermostat's fan mode will remain unchanged when the preset is activated.
    One of:

    - `on`
    - `off`
    - `auto`
    - `low`
    - `medium`
    - `high`
    - `middle`
    - `focus`
    - `diffuse`
    - `quiet`
  - **swing_mode** (*Optional*, climate swing mode): The fan swing mode the thermostat should switch to when this
    preset is activated. If not specified, the thermostat's fan swing mode will remain unchanged when the preset
    is activated. One of:

    - `off`
    - `both`
    - `horizontal`
    - `vertical`

```yaml
# Example climate controller with presets
climate:
  - platform: thermostat
    name: "Thermostat with Presets"
    preset:
      # Standard Preset
      - name: sleep
        default_target_temperature_low: 17
        default_target_temperature_high: 26
        fan_mode: LOW
        swing_mode: OFF
      # Custom preset
      - name: A custom preset
        default_target_temperature_low: 21
        default_target_temperature_high: 23
        fan_mode: HIGH
        mode: HEAT_COOL
```

- **preset_change**: (*Optional*, [Action](/automations/actions#all-actions)): The action to call when the preset is changed. This
  will be called either when a user changes the mode through the Home Assistant UI or through a call to `climate.control`

```yaml
# Example climate controller with preset and change action
climate:
  - platform: thermostat
    name: "Thermostat with Presets Actions"
    preset:
      - name: sleep
        default_target_temperature_low: 17
        default_target_temperature_high: 26
        fan_mode: LOW
        swing_mode: OFF
    preset_change:
      - logger.log: Preset has been changed!
```

### Default Preset

These configuration items determine default values the thermostat controller should use when it starts.

- **default_preset** (*Optional*, string): The name of the preset to use by default. Must match a preset
  as per [preset](#thermostat-preset).

- **on_boot_restore_from**: (*Optional*, on_boot_restore_from): Controls what the thermostat will do when
  it first boots. One of:

  - `memory` (default): The thermostat will restore any settings from last time it was running.
  - `default_preset`  : The thermostat will always switch to the preset specified by **default_preset**

> [!NOTE]
> You can specify a `default_preset` and set `on_boot_restore_from` to `memory`. In this mode when
> the settings from last boot cannot be retrieved, for any reason, then the specified `default_preset`
> will be applied.

```yaml
# This climate controller, on first boot, will switch to "My Startup Preset". Subsequent boots would
# restore to whatever mode it was in prior to the reboot
climate:
  - platform: thermostat
    name: "From Memory Thermostat"
    default_preset: My Startup Preset
    on_boot_restore_from: memory
    preset:
      - name: My Startup Preset
        default_target_temperature_low: 17
        default_target_temperature_high: 26
        fan_mode: OFF
        swing_mode: OFF
        mode: OFF
      # Custom preset
      - name: A custom preset
        default_target_temperature_low: 21
        default_target_temperature_high: 23
        fan_mode: HIGH
        mode: HEAT_COOL

# This climate controller will always switch to "Every Start Preset"
climate:
  - platform: thermostat
    name: "Default Preset Thermostat"
    default_preset: Every Start Preset
    on_boot_restore_from: default_preset
    preset:
      - name: Every Start Preset
        default_target_temperature_low: 17
        default_target_temperature_high: 26
        fan_mode: OFF
        swing_mode: OFF
        mode: OFF
      # Custom preset
      - name: A custom preset
        default_target_temperature_low: 21
        default_target_temperature_high: 23
        fan_mode: HIGH
        mode: HEAT_COOL
```

### Additional Actions/Behavior

- **target_humidity_change_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the thermostat's target humidity is changed.

- **target_temperature_change_action** (*Optional*, [Action](/automations/actions#all-actions)): The action to call when
  the thermostat's target temperature(s) is/are changed.

- **startup_delay** (*Optional*, boolean): If set to `true`, when ESPHome starts, `min_cooling_off_time`,
  `min_fanning_off_time`, and `min_heating_off_time` must elapse before each respective action may be invoked.
  This option provides a way to prevent damage to equipment (for example) disrupted by a power interruption.
  Defaults to `false`.

- **fan_only_action_uses_fan_mode_timer** (*Optional*, boolean): If set to `true`, the `fan_only_action` will
  share the same delay timer used for all `fan_mode` actions. The minimum fan switching delay is then determined
  by `min_fan_mode_switching_time` (see below). This is useful when `fan_only_action` controls the same physical
  fan as the `fan_mode` actions, common in forced-air HVAC systems.

- **fan_only_cooling** (*Optional*, boolean): If set to `true`, when in the `fan_only_mode` climate mode,
  the `fan_only_action` will only be called when the observed temperature exceeds the upper set point plus
  `cool_deadband`. When set to `false` (the default), `fan_only_action` is called immediately when
  `fan_only_mode` is activated, regardless of the current temperature or set points. Defaults to `false`.

- **fan_with_cooling** (*Optional*, boolean): If set to `true`, `fan_only_action` will be called whenever
  `cool_action` is called. This is useful for forced-air systems where the fan typically runs with cooling.
  Defaults to `false`.

- **fan_with_heating** (*Optional*, boolean): If set to `true`, `fan_only_action` will be called whenever
  `heat_action` is called. This is useful for forced-air systems where the fan typically runs with heating.
  Defaults to `false`.

- **max_cooling_run_time** (*Required with* `supplemental_cooling_action`, [Time](/guides/configuration-types#time)): Duration after
  which `supplemental_cooling_action` will be called when cooling is active. Note that
  `supplemental_cooling_action` will be called repeatedly at an interval defined by this parameter, as well,
  enabling multiple stages of supplemental (auxiliary/emergency) cooling.

- **max_heating_run_time** (*Required with* `supplemental_heating_action`, [Time](/guides/configuration-types#time)): Duration after
  which `supplemental_heating_action` will be called when heating is active. Note that
  `supplemental_heating_action` will be called repeatedly at an interval defined by this parameter, as well,
  enabling multiple stages of supplemental (auxiliary/emergency) heating.

- **min_cooling_off_time** (*Required with* `cool_action`, [Time](/guides/configuration-types#time)): Minimum duration the cooling action
  must be disengaged before it may be engaged.

- **min_cooling_run_time** (*Required with* `cool_action`, [Time](/guides/configuration-types#time)): Minimum duration the cooling action
  must be engaged before it may be disengaged.

- **min_fanning_off_time** (*Required with* `fan_only_action`, [Time](/guides/configuration-types#time)): Minimum duration the fanning
  action must be disengaged before it may be engaged.

- **min_fanning_run_time** (*Required with* `fan_only_action`, [Time](/guides/configuration-types#time)): Minimum duration the fanning
  action must be engaged before it may be disengaged.

- **min_heating_off_time** (*Required with* `heat_action`, [Time](/guides/configuration-types#time)): Minimum duration the heating action
  must be disengaged before it may be engaged.

- **min_heating_run_time** (*Required with* `heat_action`, [Time](/guides/configuration-types#time)): Minimum duration the heating action
  must be engaged before it may be disengaged.

- **min_idle_time** (**Required**, [Time](/guides/configuration-types#time)): Minimum duration the idle action must be active before calling
  another climate action.

- **min_fan_mode_switching_time** (*Required with any* `fan_mode` *action*, [Time](/guides/configuration-types#time)): Minimum duration
  any given fan mode must be active before it may be changed.

Note that `min_temperature` and `max_temperature` from the base climate component are used to define
the range of allowed temperature values in the thermostat component. See {{< docref "/components/climate" >}}.

### Hysteresis Values

- **cool_deadband** (*Optional*, float): The minimum temperature differential (temperature above the set point)
  before calling the cooling [action](/automations/actions#all-actions). Defaults to 0.5 °C.

- **cool_overrun** (*Optional*, float): The minimum temperature differential (cooling beyond the set point)
  before calling the idle [action](/automations/actions#all-actions). Defaults to 0.5 °C.

- **heat_deadband** (*Optional*, float): The minimum temperature differential (temperature below the set point)
  before calling the heating [action](/automations/actions#all-actions). Defaults to 0.5 °C.

- **heat_overrun** (*Optional*, float): The minimum temperature differential (heating beyond the set point)
  before calling the idle [action](/automations/actions#all-actions). Defaults to 0.5 °C.

- **humidity_hysteresis** (*Optional*, float): The maximum humidity differential (above/below the set point)
  before calling the respective humidity control [action](/automations/actions#config-action). Defaults to 1%.

> [!NOTE]
>
> - While this platform uses the term temperature everywhere, it can also be used to regulate other values.
>   For example, controlling humidity is also possible with this platform.
>
> - `min_temperature` and `max_temperature` from the base climate component are used the define the range of
>   adjustability and the defaults will probably not make sense for control of things like humidity. See
>   {{< docref "/components/climate" >}}.

## Bang-Bang vs. Thermostat

Please see the {{< docref "bang_bang" "Bang-Bang" >}} component's documentation for a detailed comparison of these two components.

## See Also

- {{< docref "/components/climate" >}}
- {{< docref "/components/sensor" >}}
- {{< docref "bang_bang" "Bang-Bang" >}}
- [All Actions](/automations/actions#all-actions)
