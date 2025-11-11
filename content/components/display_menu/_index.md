---
description: "Instructions for setting up a simple hierarchical menu on displays."
title: "Display Menu"
params:
  seo:
    description: Instructions for setting up a simple hierarchical menu on displays.
    image: lcd_menu.png
---

{{< anchor "display_menu" >}}

The component provides a menu primarily intended to be controlled either by a rotary encoder
with a button or a five-button joystick controller. It allows to navigate a hierarchy of items
and submenus with the ability to change the values and execute commands. The menu can
be activated and deactivated on demand, allowing alternating between using the screen for
the menu and other information.

## Overview

This document describes the configuration and automations common for the components implementing
this component. At the moment the character based LCD displays are supported using
the [lcd_menu](/components/display_menu/lcd_menu#lcd_menu) component and an instance of this is used in the configuration
examples.

```yaml
# Example configuration entry
display:
  - platform: lcd_pcf8574
    id: my_lcd
    ...
    lambda: |-
      id(my_lcd_menu).draw();
      if (!id(my_lcd_menu).is_active())
        it.print("Menu is not active");

# Declare a LCD menu
lcd_menu:
  id: my_lcd_menu
  display_id: my_lcd
  active: true
  mode: rotary
  on_enter:
    then:
      lambda: 'ESP_LOGI("display_menu", "root enter");'
  on_leave:
    then:
      lambda: 'ESP_LOGI("display_menu", "root leave");'
  items:
    - type: back
      text: 'Back'
    - type: label
      text: 'Label 1'
    - type: label
      text: !lambda |-
        return "Templated label";

# Encoder to provide navigation
sensor:
  - platform: rotary_encoder
    ...
    on_anticlockwise:
      - display_menu.up:
    on_clockwise:
      - display_menu.down:

# A de-bounced GPIO is used to 'click'
binary_sensor:
  - platform: gpio
    ...
    filters:
      - delayed_on: 10ms
      - delayed_off: 10ms
    on_press:
      - display_menu.enter:
```

Configuration variables:

- **root_item_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the root menu item.
- **active** (*Optional*, boolean): Whether the menu should start as active, meaning accepting
  user interactions and displaying output. Defaults to `true`.

- **mode** (*Optional*, enum): Defines the navigation logic. Defaults to `rotary`.

  - `rotary`  : Rotary mode expects the clockwise movement wired to [display_menu.down](#display_menu-down_action),

      the anticlockwise one to [display_menu.up](#display_menu-up_action) and the switch
      to [display_menu.enter](#display_menu-enter_action) action.

  - `joystick`  : Joystick mode expects the up, down, left and right buttons wired to the [display_menu.up](#display_menu-up_action),

      [display_menu.down](#display_menu-down_action), [display_menu.left](#display_menu-left_action)
      and [display_menu.right](#display_menu-right_action) actions and the middle button
      to the [display_menu.enter](#display_menu-enter_action) action.

- **items** (**Required**): The first level of the menu.

Automations:

- **on_enter** (*Optional*, [Automation](/automations)): An automation to perform
  when the menu level (here the root one) is entered. See [`on_enter`](#display_menu-on_enter).

- **on_leave** (*Optional*, [Automation](/automations)): An automation to perform
  when the menu level is not displayed anymore.
  See [`on_leave`](#display_menu-on_leave).

## Menu Items

The component manages a hierarchy of menu items. The common configuration variables are:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **type** (**Required**, string): The type of the menu item (see below).
- **text** (*Optional*, string, [templatable](/automations/templates)): The text displayed
  for the menu item. If a lambda is specified it gets an `it` argument pointing to
  the `MenuItem` that is being drawn.

{{< anchor "display_menu-edit_mode" >}}

### Editing values

Some of the menu items provide a way to edit values either by selecting from a list of options
or changing a numeric one. Such items can be configured in two ways.

If the `immediate_edit` configuration is `false`, the editing mode has to be activated
first by activating the rotary encoder's switch or the joystick's center button.
On the activation the `on_enter` automation is called and the item is marked as editable
(the `>` selection marker changes to `*` as default). The value can be then
iterated through the rotary wheel (in the `rotary` mode) or the joystick left
and right buttons (in the `joystick` one). The editing mode is deactivated
by another clicking of the switch, the `on_leave` automation is called and the selection
marker changes back.

If the `immediate_edit` configuration is `true` the menu item is editable immediately
when it is selected. The `on_enter` and `on_leave` are not called. In the `joystick` mode
the left and right buttons iterate through the values; the items that are editable
show the editable marker to signal that the buttons can be used. In the `rotary` mode
activating the switch iterates to the next value. The selection marker does not change
(here it is used to signal whether rotating the knob navigates the menu or changes the value).
The menu item of the `number` type can be only immediately editable in the `joystick` mode.

### Label

```yaml
items:
  - id: my_label
    type: label
    text: 'My Label'
```

The menu item of the type `label` just displays a text. There is no configuration and
no interaction is possible.

### Menu

```yaml
items:
  - type: menu
    text: 'My Submenu'
    on_enter:
      then:
        lambda: 'ESP_LOGI("display_menu", "enter: %s", it->get_text().c_str());'
    on_leave:
      then:
        lambda: 'ESP_LOGI("display_menu", "leave: %s", it->get_text().c_str());'
    items:
      - type: label
        text: 'Label'
      - type: back
        text: 'Back'
```

The menu item of the type `menu` defines a list of child menu items. When the item
is clicked the display shows the new menu level.

Configuration variables:

- **items** (**Required**): Defines the child menu items.

Automations:

- **on_enter** (*Optional*, [Automation](/automations)): An automation to perform
  when the menu level is entered. See [`on_enter`](#display_menu-on_enter).

- **on_leave** (*Optional*, [Automation](/automations)): An automation to perform
  when the menu level is not displayed anymore.
  See [`on_leave`](#display_menu-on_leave).

### Back

```yaml
items:
  - type: back
    text: 'Back'
```

The menu item of the type `back` closes the current menu level and goes up in
the menu level hierarchy. The `on_leave` automation of the current level and
`on_enter` one of the higher one are invoked. There is no configuration.

### Select

```yaml
lcd_menu:
  items:
    - type: select
      immediate_edit: false
      text: 'My Color'
      select: my_color
      on_enter:
        then:
          lambda: 'ESP_LOGI("display_menu", "select enter: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
      on_leave:
        then:
          lambda: 'ESP_LOGI("display_menu", "select leave: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
      on_value:
        then:
          lambda: 'ESP_LOGI("display_menu", "select value: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'

select:
  - platform: template
    id: my_color
    optimistic: true
    options:
      - 'Red'
      - 'Green'
      - 'Blue'
```

The menu item of the type `select` allows cycling through a set of values defined by the
associated `select` component.

Configuration variables:

- **immediate_edit** (*Optional*, boolean): Whether the item can be immediately edited when
  selected. See [Editing Values](#display_menu-edit_mode). Defaults to `false`.

- **select** (**Required**, [ID](/guides/configuration-types#id)): A `select` component managing
  the edited value.

- **value_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda returning a string to be displayed as value. The lambda gets an `it` argument
  pointing to the `MenuItem`. If not specified the selected option name of the `select`
  component is used as the value.

Automations:

- **on_enter** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is activated. See [`on_enter`](#display_menu-on_enter).

- **on_leave** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is exited.
  See [`on_leave`](#display_menu-on_leave).

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when the value is changed.
  See [`on_value`](#display_menu-on_value).

### Number

```yaml
lcd_menu:
  items:
    - type: number
      text: 'My Number'
      format: '%.2f'
      number: my_number
      on_enter:
        then:
          lambda: 'ESP_LOGI("display_menu", "number enter: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
      on_leave:
        then:
          lambda: 'ESP_LOGI("display_menu", "number leave: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
      on_value:
        then:
          lambda: 'ESP_LOGI("display_menu", "number value: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'

number:
  - platform: template
    id: my_number
    optimistic: true
    min_value: 10.0
    max_value: 20.0
    step: 0.5
    on_value:
      then:
        lambda: 'ESP_LOGI("number", "value: %f", x);'
```

The menu item of the type `number` allows editing a floating point number.
On click the `on_enter` automation is called and the item is marked as editable
(the `>` selection marker changes to `*` as default). Up and down events
then increase and decrease the value by steps defined in the `number`,
respecting the `min_value` and `max_value`. The editing mode is exited
by another click.

Note that the fractional floating point values do not necessarily add nicely and
ten times `0.100000` is not necessarily `1.000000`. Use steps that are
powers of two (such as `0.125`  ) or take care of the rounding explicitly.

Configuration variables:

- **immediate_edit** (*Optional*, boolean): Whether the item can be immediately edited when
  selected. See [Editing Values](#display_menu-edit_mode). Ignored in the `rotary` mode.
  Defaults to `false`.

- **number** (**Required**, [ID](/guides/configuration-types#id)): A `number` component managing
  the edited value. If on entering the value is less than `min_value` or more than
  `max_value`, the value is capped to fall into the range.

- **format** (*Optional*, string): A `printf`  -like format string specifying
  exactly one `f` or `g`  -type conversion used to display the current value.
  Defaults to `%.1f`.

- **value_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda returning a string to be displayed as value. The lambda gets an `it` argument
  pointing to the `MenuItem`. If not specified the value of the `number` component
  formatted according to the `format` is used as the value.

Automations:

- **on_enter** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is activated. See [`on_enter`](#display_menu-on_enter).

- **on_leave** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is exited.
  See [`on_leave`](#display_menu-on_leave).

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when the value is changed.
  See [`on_value`](#display_menu-on_value).

### Switch

```yaml
lcd_menu:
  items:
    - type: switch
      immediate_edit: false
      text: 'My Switch'
      on_text: 'Bright'
      off_text: 'Dark'
      switch: my_switch
      on_enter:
        then:
          lambda: 'ESP_LOGI("display_menu", "switch enter: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
      on_leave:
        then:
          lambda: 'ESP_LOGI("display_menu", "switch leave: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
      on_value:
        then:
          lambda: 'ESP_LOGI("display_menu", "switch value: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'

switch:
  - platform: template
    id: my_switch
    optimistic: true
```

The menu item of the type `switch` allows toggling the associated `switch` component.

Configuration variables:

- **immediate_edit** (*Optional*, boolean): Whether the item can be immediately edited when
  selected. See [Editing Values](#display_menu-edit_mode). Defaults to `false`.

- **on_text** (*Optional*, string): The text for the `ON` state. Defaults to `On`.
- **off_text** (*Optional*, string): The text for the `OFF` state. Defaults to `Off`.
- **switch** (**Required**, [ID](/guides/configuration-types#id)): A `switch` component managing
  the edited value.

- **value_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda returning a string to be displayed as value. The lambda gets an `it` argument
  pointing to the `MenuItem`. If not specified the `on_text` / `off_text` is used.

Automations:

- **on_enter** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is activated. See [`on_enter`](#display_menu-on_enter).

- **on_leave** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is exited.
  See [`on_leave`](#display_menu-on_leave).

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when the value is changed.
  See [`on_value`](#display_menu-on_value).

### Command

```yaml
items:
  - type: command
    text: 'Hide'
    on_value:
      then:
        - display_menu.hide:
```

The menu item of the type `command` allows triggering commands. There is no
additional configuration.

Automations:

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when the menu item is clicked.
  See [`on_value`](#display_menu-on_value).

### Custom

```yaml
lcd_menu:
  items:
    - type: custom
      immediate_edit: false
      text: 'My Custom'
      value_lambda: 'return to_string(some_state);'
      on_next:
        then:
          lambda: 'some_state++;'
      on_prev:
        then:
          lambda: 'some_state--;'
```

The menu item of the type `custom` delegates navigating the values to the automations
and displaying the value to the `value_lambda`.

Configuration variables:

- **immediate_edit** (*Optional*, boolean): Whether the item can be immediately edited when
  selected. See [Editing Values](#display_menu-edit_mode). Defaults to `false`.

- **value_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda returning a string to be displayed as value. The lambda gets an `it` argument
  pointing to the `MenuItem`.

Automations:

- **on_enter** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is activated. See [`on_enter`](#display_menu-on_enter).

- **on_leave** (*Optional*, [Automation](/automations)): An automation to perform
  when the editing mode is exited.
  See [`on_leave`](#display_menu-on_leave).

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when the value is changed.
  See [`on_value`](#display_menu-on_value).

- **on_next** (*Optional*, [Automation](/automations)): An automation to perform
  when the user navigates to the next value.
  See [`on_next`](#display_menu-on_next).

- **on_prev** (*Optional*, [Automation](/automations)): An automation to perform
  when the user navigates to the previous value.
  See [`on_prev`](#display_menu-on_prev).

## Automations

{{< anchor "display_menu-on_enter" >}}

### `on_enter`

This automation will be triggered when the menu level is entered, i.e. the component
draws its items on the display. The `it` parameter points to a `MenuItem` class
with the information of the menu item describing the displayed child items.
If present at the top level it is an internally generated root menu item,
otherwise an user defined one.

```yaml
lcd_menu:
  ...
  items:
    - type: menu
      text: 'Submenu 1'
      on_enter:
        then:
          lambda: 'ESP_LOGI("display_menu", "enter: %s", it->get_text().c_str());'
```

{{< anchor "display_menu-on_leave" >}}

### `on_leave`

This automation will be triggered when the menu level is exited, i.e. the component
does not draw its items on the display anymore. The `it` parameter points to
a `MenuItem` class with the information of the menu item. If present at the
top level it is an internally generated root menu item, otherwise
an user defined one. It does not matter whether the level was left due to entering
the submenu or going back to the parent menu.

```yaml
lcd_menu:
  ...
  items:
    - type: menu
      text: 'Submenu 1'
      on_leave:
        then:
          lambda: 'ESP_LOGI("display_menu", "leave: %s", it->get_text().c_str());'
```

{{< anchor "display_menu-on_value" >}}

### `on_value`

This automation will be triggered when the value edited through the menu changed
or a command was triggered.

```yaml
lcd_menu:
  ...
  items:
    - type: select
      text: 'Select Item'
      select: my_select_1
      on_value:
        then:
          lambda: 'ESP_LOGI("display_menu", "select value: %s, %s", it->get_text().c_str(), it->get_value_text().c_str());'
```

{{< anchor "display_menu-on_next" >}}

### `on_next`

This automation will be triggered when the user requested to set the value to the next one.

```yaml
lcd_menu:
  ...
  items:
    - type: custom
      text: 'Custom Item'
      value_lambda: 'return to_string(some_state);'
      on_next:
        then:
          lambda: 'some_state++;'
```

{{< anchor "display_menu-on_prev" >}}

### `on_prev`

This automation will be triggered when the user requested to set the value to the previous one.

```yaml
lcd_menu:
  ...
  items:
    - type: custom
      text: 'Custom Item'
      value_lambda: 'return to_string(some_state);'
      on_prev:
        then:
          lambda: 'some_state--;'
```

{{< anchor "display_menu-up_action" >}}

### `display_menu.up` Action

This is an [Action](/automations/actions#all-actions) for navigating up in a menu. The action
is usually wired to an anticlockwise turn of a rotary encoder or to the upper
button of the joystick.

```yaml
sensor:
  - platform: rotary_encoder
    ...
    on_anticlockwise:
      - display_menu.up:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to navigate.

{{< anchor "display_menu-down_action" >}}

### `display_menu.down` Action

This is an [Action](/automations/actions#all-actions) for navigating down in a menu. The action
is usually wired to a clockwise turn of a rotary encoder or to the lower
button of the joystick.

```yaml
sensor:
  - platform: rotary_encoder
    ...
    on_clockwise:
      - display_menu.down:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to navigate.

{{< anchor "display_menu-left_action" >}}

### `display_menu.left` Action

This is an [Action](/automations/actions#all-actions) usually wired to the left button
of the joystick. In the `joystick` mode it is used to set the previous
value or to decrement the numeric one; depending on the `immediate_edit`
flag entering the edit mode is required or not. If used in the `rotary`
mode it exits the editing. In both modes it can be also used to navigate
back one level when used with the `back` menu item.

```yaml
binary_sensor:
  - platform: gpio
    ...
    on_press:
      - display_menu.left:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to navigate.

{{< anchor "display_menu-right_action" >}}

### `display_menu.right` Action

This is an [Action](/automations/actions#all-actions) usually wired to the right button
of the joystick. In the `joystick` mode it is used to set the next
value or to increment the numeric one; depending on the `immediate_edit`
flag entering the edit mode is required or not. In both modes it can
be also used to enter the submenu when used with the `menu` menu item.

```yaml
binary_sensor:
  - platform: gpio
    ...
    on_press:
      - display_menu.right:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to navigate.

{{< anchor "display_menu-enter_action" >}}

### `display_menu.enter` Action

This is an [Action](/automations/actions#all-actions) for triggering a selected menu item, resulting
in an action depending on the type of the item - entering a submenu, starting/stopping
editing or triggering a command. The action is usually wired to a press button
of a rotary encoder or to the center button of the joystick.

```yaml
binary_sensor:
  - platform: gpio
    ...
    filters:
      - delayed_on: 10ms
      - delayed_off: 10ms
    on_press:
      - display_menu.enter:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to navigate.

.. display_menu-show_action:

### `display_menu.show` Action

This is an [Action](/automations/actions#all-actions) for showing an inactive menu. The state
of the menu remains unchanged, i.e. the menu level shown at the moment it was hidden
is restored, as is the selected item. The following snippet shows the menu if it is
inactive, otherwise triggers the selected item.

```yaml
on_press:
  - if:
      condition:
        display_menu.is_active:
      then:
        - display_menu.enter:
      else:
        - display_menu.show:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to show.

.. display_menu-hide_action:

### `display_menu.hide` Action

This is an [Action](/automations/actions#all-actions) for hiding the menu. A hidden menu
does not react to `draw()` and does not process navigation actions.

```yaml
lcd_menu:
  ...
  items:
    - type: command
      text: 'Hide'
      on_value:
        then:
          - display_menu.hide:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to hide.

.. display_menu-show_main_action:

### `display_menu.show_main` Action

This is an [Action](/automations/actions#all-actions) for showing the root level of the menu.

```yaml
lcd_menu:
  ...
  items:
    - type: command
      text: 'Show Main'
      on_value:
        then:
          - display_menu.show_main:
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the menu to hide.

{{< anchor "display_menu-is_active" >}}

### `display_menu.is_active` Condition

This [Condition](/automations/actions#all-conditions) checks if the given menu is active, i.e.
shown on the display and processing navigation events.

```yaml
on_press:
  - if:
      condition:
        display_menu.is_active:
      ...
```

## See Also

- {{< apiref "display_menu_base/display_menu_base.h" "display_menu_base/display_menu_base.h" >}}
