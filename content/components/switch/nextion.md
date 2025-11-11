---
description: "Instructions for setting up Nextion Switch."
title: "Nextion Switch Component"
params:
  seo:
    description: Instructions for setting up Nextion Switch.
    image: nextion.jpg
---

{{< anchor "nextion_switch" >}}

The `nextion` switch platform supports the many switched components in the Nextion as well as integer variables. It can be a component or variable in the Nextion display.
It is best to set the components vscope to global in the Nextion Editor. This way the component will be available if the page is shown or not.

See {{< docref "/components/display/nextion" >}} for setting up the display

```yaml
# Example configuration entry
display:
  - platform: nextion
    id: nextion1
    # ...

switch:
  - platform: nextion
    id: r0_switch
    name: "Radio 0 Switch"
    component_name: r0 # pageX.r0 for a global
    update_interval: 4s
  - platform: nextion
    id: darkmode
    name: "Is Darkmode Set"
    variable_name: darkmode
```

## Configuration variables

- **nextion_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the Nextion display.
- **component_name** (*Optional*, string): The name of the Nextion component.
- **variable_name** (*Optional*, string): The name of the Nextion variable. Any value over `0` is considered to be **on**
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The duration to update the sensor. If using a [Nextion Custom Switch Protocol](#nextion_custom_switch_protocol) this should not be used
- **background_color** (*Optional*, [Color](/components/display#config-color)): The background color
- **background_pressed_color** (*Optional*, [Color](/components/display#config-color)): The background color when pressed
- **foreground_color** (*Optional*, [Color](/components/display#config-color)): The foreground color
- **foreground_pressed_color** (*Optional*, [Color](/components/display#config-color)): The foreground color when pressed
- **visible** (*Optional*, boolean): Visible or not
- All other options from [Switch](/components/switch#config-switch).

**Only one** *component_name* **or** *variable_name* **can be set**

See [How things Update](#nextion_switch_how_things_update) for additional information

### Globals

The Nextion does not retain data on Nextion page changes. Additionally, if a page is changed and the **component_name** does not exist on that page then
nothing will be updated. To get around this, the Nextion components can be changed to have a vscope of `global`. If this is set then the **component_name**
should be prefixed with the page name (page0/page1 or whatever you have changed it to).

*Example:* `component_name: page0.r0`

{{< anchor "switch-nextion-publish_action" >}}

## `switch.nextion.publish` Action

You can also publish a state to a Nextion switch from elsewhere in your YAML file
with the `switch.nextion.publish` action.

```yaml
# Example configuration entry
sensor:
  - platform: nextion
    id: nextion_switch
    ...
# in some trigger
on_...:
  - switch.nextion.publish:
      id: nextion_switch
      state: true
      # These are optional. Defaults to true.
      publish_state: true
      send_to_nextion: true
  # Templated
  - switch.nextion.publish:
      id: nextion_switch
      state: !lambda 'return true;'
      # These are optional. Defaults to true.
      publish_state: true
      send_to_nextion: true
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the Nextion switch.
- **state** (**Required**, string, [templatable](/automations/templates)): The boolean state to publish.
- **publish_state** (*Optional*, bool, [templatable](/automations/templates)): Publish new state to Home Assistant.
  Default is true.

- **send_to_nextion** (*Optional*, bool, [templatable](/automations/templates)): Publish new state to Nextion
  display which will update component. Default is true.

> [!NOTE]
> This action can also be written in lambdas. See [Lambda Calls](#nextion_switch_lambda_calls)

{{< anchor "nextion_switch_lambda_calls" >}}

### Lambda Calls

From [lambdas](/automations/templates#config-lambda), you can call several methods to access
some more advanced functions (see the full {{< apiref "nextion/nextion_switch.h" "nextion/nextion_switch.h" >}} for more info).

{{< anchor "nextion_switch_set_state" >}}

- `set_state(bool value, bool publish, bool send_to_nextion)`  : Set the state to **value**. Publish the new state to HASS. Send_to_Nextion is to publish the state to the Nextion.

{{< anchor "nextion_switch_update" >}}

- `update()`  : Poll from the Nextion

{{< anchor "nextion_switch_settings" >}}

- `set_background_color(Color color)`  : Sets the background color to **Color**
- `set_background_pressed_color(Color color)`  : Sets the background color to **Color**
- `set_foreground_color(Color color)`  : Sets the background color to **Color**
- `set_foreground_pressed_color(Color color)`  : Sets the background color to **Color**
- `set_visible(bool visible)` : Sets visible or not. If set to false, no updates will be sent to the component

{{< anchor "nextion_switch_how_things_update" >}}

## How things Update

A Nextion component with an integer value (.val) or Nextion variable will be automatically polled if **update_interval** is set.
To have the Nextion send the data you can use the [Nextion Custom Switch Protocol](#nextion_custom_switch_protocol) for this. Add the [Nextion Custom Switch Protocol](#nextion_custom_switch_protocol) to the
component or function you want to trigger the send. Typically this is in *Touch Press Event* but some components, like a slider, should have it
set in the *Touch Release Event* to capture all the changes. Since this is a custom protocol it can be sent from anywhere (timers/functions/components)
in the Nextion.

> [!NOTE]
> There is no need to check the *Send Component ID* for the *Touch Press Event* or *Touch Release Event*
> since this will be sending the real value to esphome.

Using the above yaml example:

- "Radio 0 switch" will poll the Nextion for the `r0.val` value and set the state accordingly.
- "Is Darkmode Set" will NOT poll the Nextion. Either the Nextion will need to use the [Nextion Custom Switch Protocol](#nextion_custom_switch_protocol) or use a lambda:

- [Lambda Calls](#nextion_switch_lambda_calls).

> [!NOTE]
> No updates will be sent to the Nextion if it is sleeping. Once it wakes, the components will be updated. If a component is invisible, `visible(false)`, then it won't update until it is set to be visible.

{{< anchor "nextion_custom_switch_protocol" >}}

## Nextion Custom Switch Protocol

All lines are required

```c
printh 90
prints "r0",0
printh 00
prints r0.val,0
printh FF FF FF
```

### Explanation

- `printh 90` Tells the library this is a switch bool/integer data
- `prints "r0",0` Sends the name that matches **component_name** or **variable_name**
- `printh 00` Sends a NULL
- `prints r0.val,0` The actual value to send. For a variable use the Nextion variable name `r0` with out `.val`
- `printh FF FF FF` Nextion command ack

## See Also

- {{< docref "/components/display/nextion" >}}
- {{< docref "index/" >}}
- {{< apiref "nextion/switch/nextion_switch.h" "nextion/switch/nextion_switch.h" >}}
