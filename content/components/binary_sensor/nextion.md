---
description: "Instructions for setting up Nextion binary sensor."
title: "Nextion Binary Sensor Component"
params:
  seo:
    description: Instructions for setting up Nextion binary sensor.
    image: nextion.jpg
---

{{< anchor "nextion_binary_sensor" >}}

The `nextion`   binary sensor platform supports the many switched components in the Nextion as well as integer variables (>0 == true). It can be a component or variable in the Nextion display.
It is best to set the components vscope to global in the Nextion Editor. This way the component will be available if the page is shown or not.

See {{< docref "/components/display/nextion" >}} for setting up the display

```yaml
# Example configuration entry
display:
  - platform: nextion
    id: nextion1
    # ...

binary_sensor:
  - platform: nextion
    page_id: 0
    component_id: 8
    component_name: page0.mode # Only needed to support changing colors
    id: mode
  - platform: nextion
    id: r0_binary_sensor
    name: "Radio 0 Binary Sensor"
    component_name: page0.r0 # r0 is a global component in the Nextion Editor on page 0
    update_interval: 4s
  - platform: nextion
    id: darkmode
    name: "Is Darkmode Set"
    variable_name: darkmode
```

## Configuration variables

- **nextion_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the Nextion display.
- **component_name** (*Optional*, string): The name of the Nextion component.
- **variable_name** (*Optional*, string): The name of the Nextion variable. Any value over `0`   is considered to be **on**
- **page_id** (*Optional*, string): The ID of the page the component is on. Use `0`   for the default page.
- **component_id** (*Optional*, string): The ID (the number, not name!) of the component to track.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The duration to update the sensor. If using a [Nextion Custom Binary Sensor Protocol](#nextion_custom_binary_sensor_protocol) this should not be used
- **background_color** (*Optional*, [Color](/components/display#config-color)): The background color
- **foreground_color** (*Optional*, [Color](/components/display#config-color)): The foreground color
- **visible** (*Optional*, boolean): Visible or not
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

**Touch Sensor:**
The Nextion will send a **page_id** and **component_id** when the *Send Component ID* check box is selected for the component. To enable
this native event **page_id** and **component_id** are required. No [Nextion Custom Binary Sensor Protocol](#nextion_custom_binary_sensor_protocol) is required. If **page_id** and **component_id** are set then the component will only react to touch events from the Nextion. Setting **component_name** will allow setting options like foreground color.

> [!NOTE]
> `background_color(s)`, `foreground_color(s)`   and `visible`   do not retain their state on page change. [Binary Sensor Settings](#nextion_binary_sensor_settings).
> A [Nextion Sensor](/components/sensor/nextion#nextion_sensor) with a custom protocol sending the current page can be used to execute the API call [Update Components By Prefix](/components/display/nextion#update_components_by_prefix) to update all the components for that page

Example:

```yaml
- platform: nextion
  id: current_page
  nextion_id: nextion1
  name: current_page
  variable_name: current_page
  on_value:
    lambda: |-
      id(nextion1).update_components_by_page_prefix("page"+x+".");
```

See [How things Update](#nextion_binary_sensor_how_things_update) for additional information

### Globals

The Nextion does not retain data on Nextion page changes. Additionally if a page is changed and the **nextion_component_name** does not exist on that page then
nothing will be updated. To get around this the Nextion components can be changed to have a vscope of `global`. If this is set then the **nextion_component_name**
should be prefixed with the page name (page0/page1).

*Example:* `nextion_component_name: page0.r0`

{{< anchor "binary_sensor-nextion-publish_action" >}}

## `binary_sensor.nextion.publish` Action

You can also publish a state to a Nextion binary sensor from elsewhere in your YAML file
with the `binary_sensor.nextion.publish` action.

```yaml
# Example configuration entry
binary_sensor:
  - platform: nextion
    id: nextion_bsensor
    ...
# in some trigger
on_...:
  - binary_sensor.nextion.publish:
      id: nextion_bsensor
      state: true
      # These are optional. Defaults to true.
      publish_state: true
      send_to_nextion: true
  # Templated
  - binary_sensor.nextion.publish:
      id: nextion_bsensor
      state: !lambda 'return true;'
      # These are optional. Defaults to true.
      publish_state: true
      send_to_nextion: true
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the Nextion switch.
- **state** (**Required**, string, [templatable](/automations/templates)): The boolean state to publish.
- **publish_state** (*Optional*, bool, [templatable](/automations/templates)): Publish new state to Home Assistant.
  Default is true.

- **send_to_nextion** (*Optional*, bool, [templatable](/automations/templates)): Publish new state to Nextion
  display which will update component. Default is true.

> [!NOTE]
> This action can also be written in lambdas. See [Lambda Calls](#nextion_binary_sensor_lambda_calls)

{{< anchor "nextion_binary_sensor_lambda_calls" >}}

## Lambda Calls

From [lambdas](/automations/templates#config-lambda), you can call several methods to access some
more advanced functions (see the full {{< apiref "nextion/binary_sensor/nextion_binarysensor.h" "nextion/binary_sensor/nextion_binarysensor.h" >}} for more info).

{{< anchor "nextion_binary_sensor_set_state" >}}

- `set_state(bool value, bool publish, bool send_to_nextion)`  : Set the state to **value**. Publish the new state to HASS. Send_to_Nextion is to publish the state to the Nextion.

{{< anchor "nextion_binary_sensor_update" >}}

- `update()`  : Poll from the Nextion

{{< anchor "nextion_binary_sensor_settings" >}}

- `set_background_color(Color color)`  : Sets the background color to **Color**
- `set_background_pressed_color(Color color)`  : Sets the background color to **Color**
- `set_foreground_color(Color color)`  : Sets the background color to **Color**
- `set_foreground_pressed_color(Color color)`  : Sets the background color to **Color**
- `set_visible(bool visible)`   : Sets visible or not. If set to false, no updates will be sent to the component

{{< anchor "nextion_binary_sensor_how_things_update" >}}

## How things Update

A Nextion component with an integer value (.val) or Nextion variable will be automatically polled if **update_interval** is set.
To have the Nextion send the data you can use the [Nextion Custom Binary Sensor Protocol](#nextion_custom_binary_sensor_protocol) for this. Add the [Nextion Custom Binary Sensor Protocol](#nextion_custom_binary_sensor_protocol) to the
component or function you want to trigger the send. Typically this is in *Touch Press Event* but some components, like a slider, should have it
set in the *Touch Release Event* to capture all the changes. Since this is a custom protocol it can be sent from anywhere (timers/functions/components)
in the Nextion.

> [!NOTE]
> There is no need to check the *Send Component ID* for the *Touch Press Event* or *Touch Release Event*
> for an integer value component since this will be sending the real value to esphome,
> but make sure you have both checked for a touch sensor.

Using the above yaml example:

- "mode" is a touch sensor and will trigger when a user presess the component with ID `8`   in page `0`
- "Radio 0 Binary Sensor" will poll the Nextion for the `r0.val`   value and set the state accordingly.
- "Is Darkmode Set" will NOT poll the Nextion. Either the Nextion will need to use the [Nextion Custom Binary Sensor Protocol](#nextion_custom_binary_sensor_protocol) or use a lambda:

- [Lambda Calls](#nextion_binary_sensor_lambda_calls).

> [!NOTE]
> No updates will be sent to the Nextion if it is sleeping. Once it wakes the components will be updated. If a component is invisible, `visible(false)`, then it won't update until it is set to be visible.

{{< anchor "nextion_custom_binary_sensor_protocol" >}}

## Nextion Custom Binary Sensor Protocol

All lines are required

```c
printh 93
prints "r0",0
printh 00
prints r0.val,0
printh FF FF FF
```

### Explanation

- `printh 93`   Tells the library this is a binary sensor bool/integer data
- `prints "r0",0`   Sends the name that matches **nextion_component_name** or **nextion_variable_name**
- `printh 00`   Sends a NULL
- `prints r0.val,0`   The actual value to send. For a variable use the Nextion variable name `r0`   with out `.val`
- `printh FF FF FF`   Nextion command ack

## See Also

- {{< docref "/components/display/nextion" >}}
- {{< docref "index/" >}}
- {{< apiref "nextion/binary_sensor/nextion_binarysensor.h" "nextion/binary_sensor/nextion_binarysensor.h" >}}
