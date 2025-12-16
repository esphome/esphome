---
description: "LVGL Widgets"
title: "LVGL Widgets"
---

At the next level of the LVGL object hierarchy are the widgets, which support styling directly. They can have sub-parts, which may be styled separately. Usually styles are inherited, but this depends on widget specifics or functionality. The widget and its parts have states, and different styling can be set for different states.

Widgets can have children, which can be any other widgets. Think of this as a nested structure. The child widgets move with the parent and, if the parent is hidden, its children will also be hidden.

By default, LVGL draws new widgets on top of old widgets, including their children. When widgets have children, property inheritance takes place. Some properties (typically those related to text and opacity) can be inherited from the parent widgets's styles. When the property is inheritable, the parent will be searched for an object which specifies a value for the property. The parents will use their own [state](#lvgl-widgetproperty-state) to determine the value. For example, if a button is pressed and the text color is defined by the "pressed" state, this "pressed" text color will be used.

## Common properties

The properties below are common to all widgets.

**Configuration variables:**

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **x** (*Optional*, int16 or percentage): Horizontal position of the widget.
- **y** (*Optional*, int16 or percentage): Vertical position of the widget.

> [!NOTE]
> By default, the `x` and `y` coordinates are measured from the *top left corner* of the parent's content area. [Important](/components/lvgl#lvgl-styling): content area starts *after the padding* thus if the parent has a non-zero padding value, position will be shifted with that. Percentage values are calculated from the parent's content area size.
>
> If specifying `align`, `x` and `y` can be used as an offset to the calculated position (can also be negative). They are ignored if [Layouts](/components/lvgl/layouts#lvgl-layouts) are used on the parent.

- **height** (*Optional*): Height of the widget in pixels or a percentage, or `SIZE_CONTENT`.
- **width** (*Optional*): Width of the widget in pixels or a percentage, or `SIZE_CONTENT`.

> [!NOTE]
> The size settings support a special value: `SIZE_CONTENT`. It means the widget's size in the respective direction will be set to the size of its children. Note that only children on the right and bottom sides will be considered and children on the top and left remain cropped. This limitation makes the behavior more predictable. Widgets with `hidden` or `floating` flags will be ignored by the `SIZE_CONTENT` calculation.
>
> Similarly to CSS, LVGL also supports `min_width`, `max_width`, `min_height` and `max_height`. These are limits preventing a widget's size from becoming smaller/larger than these values. They are especially useful if the size is set by percentage or `SIZE_CONTENT`.

- **min_width**, **max_width**, **min_height**, **max_height** (*Optional*, int16 or percentage): Sets a minimal/maximal width or a minimal/maximal height. Pixel and percentage values can be used. Percentage values are relative to the dimensions of the parent's content area. Defaults to `0%`.
- **scrollbar_mode** (*Optional*, string): If a child widget is outside its parent content area (the size without padding), the parent can become scrollable (see the `scrollable` [flag](#lvgl-widget-flags)). The widget can either be scrolled horizontally or vertically in one stroke. Scroll bars can appear depending on the setting:
  - `"OFF"`  : Never show the scroll bars (use the double quotes!).
  - `"ON"`  : Always show the scroll bars (use the double quotes!).
  - `"ACTIVE"`  : Show scroll bars while a widget is being scrolled.
  - `"AUTO"`  : Show scroll bars when the content is large enough to be scrolled (default).
- **scroll_dir** (*Optional*, string): Sets the permissible scroll directions for an object - one of `LEFT`, `RIGHT`,
  `BOTTOM`, `TOP`, `HOR`, `VER`, `ALL` (default).
- **scroll_snap_x** (*Optional*, string): For a child of a scrollable object, this property defines the snap position
  of the child in the X direction. One of `NONE` (default), `START`, `END`, `CENTER`.
- **scroll_snap_y** (*Optional*, string): For a child of a scrollable object, this property defines the snap position
  of the child in the Y direction. One of `NONE` (default), `START`, `END`, `CENTER`.
- **align** (*Optional*, enum): Alignment of the widget relative to the parent. A child widget is clipped to its parent boundaries. One of the values *not* starting with `OUT_` (see picture below).
- **align_to** (*Optional*, list): Alignment of the widget relative to another widget on the same level:
  - **id** (**Required**): The ID of a widget *to* which you want to align.
  - **align** (**Required**, string): Desired alignment (one of the values starting with `OUT_`  ).
  - **x** (*Optional*, int16 or percentage): Horizontal offset position. Default `0`.
  - **y** (*Optional*, int16 or percentage): Vertical offset position. Default `0`.

{{< img src="lvgl_align.png" alt="Image" class="align-center" >}}

- **group** (*Optional*, string): The name of the group of widgets which will interact with a {{< docref "/components/sensor/rotary_encoder" >}}. In every group there is always one focused widget which receives the encoder actions. You need to associate an input device with a group. An input device can send key events to only one group but a group can receive data from more than one input device. If no group is specified for a widget or an encoder, an unnamed default group will be assigned, so in most cases where only one encoder is used it will not be necessary to explicitly specify a group.
- **layout** (*Optional*): See [Layouts](/components/lvgl/layouts#lvgl-layouts) for details. Defaults to `NONE`.
- **styles** (*Optional*, [ID](/guides/configuration-types#id)): The ID of a *style definition* from the main component configuration to override the theme styles.
- **theme** (*Optional*, list): A list of styles to apply to the widget and children. Same configuration option as at the main component.
- **widgets** (*Optional*, list): A list of LVGL widgets to be drawn as children of this widget. Same configuration option as at the main component.

{{< anchor "lvgl-widgetproperty-state" >}}

- **state** (*Optional*, dict): Widgets or their (sub)parts can have states, which support separate styling. These state styles inherit from the theme, but can be locally set or overridden within style definitions. Can be one of:
  - **checked** (*Optional*, boolean): Toggled or checked state.
  - **disabled** (*Optional*, boolean): Disabled state (also usable with [shorthand](#lvgl-automation-shorthands) actions `lvgl.widget.enable` and `lvgl.widget.disable`  ).
  - **edited** (*Optional*, boolean): Edit by an encoder.
  - **focus_key** (*Optional*, boolean): Focused via keypad or encoder but *not* via touch screen.
  - **focused** (*Optional*, boolean): Focused via keypad or encoder or clicked via touch screen.
  - **pressed** (*Optional*, boolean): Being pressed.
  - **scrolled** (*Optional*, boolean): Being scrolled.
  - **user_1**, **user_2**, **user_3**, **user_4** (*Optional*, boolean): Custom states.

By default, states are all `false`, and they are templatable.
To apply styles to the states, you need to specify them one level above, for example:

```yaml
- button:
    checkable: true
    state:
      checked: true # here you activate the state to be used at boot
    checked:
      bg_color: 0x00FF00 # here you apply styles to be used when in the respective state
```

The state itself can be can be changed by interacting with the widget, or through [actions](#lvgl-automation-actions) with `lvgl.widget.update`.

See [Cover status and control](/cookbook/lvgl#lvgl-cookbook-cover) for a cookbook example which demonstrates how to use styling and properties to show different states of a Home Assistant entity.

{{< anchor "lvgl-widget-flags" >}}

In addition to visual styling, each widget supports some boolean **flags** to influence the behavior:

- **adv_hittest** (*Optional*, boolean): allow performing more accurate hit (click) test. For example, may help by accounting for rounded corners.
- **checkable** (*Optional*, boolean): toggle checked state when the widget is clicked.
- **click_focusable** (*Optional*, boolean): add focused state to the widget when clicked.
- **clickable** (*Optional*, boolean): make the widget clickable by input devices. Defaults to `true`. If `false`, it will pass the click to the widgets behind it (clicking through).
- **event_bubble** (*Optional*, boolean): propagate the events to the parent.
- **floating** (*Optional*, boolean): do not scroll the widget when the parent scrolls and ignore layout.
- **gesture_bubble** (*Optional*, boolean): propagate the gestures to the parent.
- **hidden** (*Optional*, boolean): make the widget hidden (like it wasn't there at all), also usable with [shorthand](#lvgl-automation-shorthands) actions `lvgl.widget.show` and `lvgl.widget.hide`. Hidden objects are ignored in layout calculations. Defaults to `false`.
- **ignore_layout** (*Optional*, boolean): the widget is simply ignored by the layouts. Its coordinates can be set as usual.
- **layout_1**, **layout_2** (*Optional*, boolean): custom flags, free to use by layouts.
- **overflow_visible** (*Optional*, boolean): do not clip the children's content to the parent's boundary.
- **press_lock** (*Optional*, boolean): keep the widget pressed even if the press slid from the widget.
- **scroll_chain simple** (*Optional*, boolean): packaging for (`scroll_chain_hor | scroll_chain_ver`  ).
- **scroll_chain_hor** (*Optional*, boolean): allow propagating the horizontal scroll to a parent.
- **scroll_chain_ver** (*Optional*, boolean): allow propagating the vertical scroll to a parent.
- **scroll_elastic** (*Optional*, boolean): allow scrolling inside but with slower speed.
- **scroll_momentum** (*Optional*, boolean): make the widget scroll further when "thrown".
- **scroll_on_focus** (*Optional*, boolean): automatically scroll widget to make it visible when focused.
- **scroll_one** (*Optional*, boolean): allow scrolling only on `snappable` children.
- **scroll_with_arrow** (*Optional*, boolean): allow scrolling the focused widget with arrow keys.
- **scrollable** (*Optional*, boolean): the widget can become scrollable. Defaults to `true` (also see the `scrollbar_mode` property).
- **snappable** (*Optional*, boolean): if scroll snap is enabled on the parent it can snap to this widget.
- **user_1**, **user_2**, **user_3**, **user_4** (*Optional*, boolean): custom flags, free to use by user.
- **widget_1**, **widget_2** (*Optional*, boolean): custom flags, free to use by widget.

> [!NOTE]
> LVGL only supports **integers** for numeric `value`. Visualizer widgets can't display floats directly, but they allow scaling by 10s. Some examples in the {{< docref "/cookbook/lvgl" "Cookbook" >}} cover how to do that.

{{< anchor "lvgl-widget-parts" >}}

## Widget parts

Widgets can have multiple parts, each of which can be styled independently. For example, a checkbox has a *main* part that styles the background and text label, and an *indicator* part that styles the tick box. All widgets have a *main* part, the available parts for other widgets are specified in the widget description.

The possible parts are:

- **main** (*Optional*, dict): The main part of the widget, i.e. the background. Any style properties applied at the top level of the widget are assumed to apply to this part, but may also be specified under the *main* config key.
- **scrollbar** (*Optional*, dict): The scrollbar styles.
- **indicator** (*Optional*, dict): The indicator part of the widget. The indicator part may be used to show tick boxes or other visual indicators in slider, bar or arc.
- **knob** (*Optional*, dict): The knob part of the widget e.g. a draggable item in slider, bar or arc.
- **selected** (*Optional*, dict): The currently selected part of the widget, e.g. text or the selected item in a roller.
- **items** (*Optional*, dict): The items part of the widget, e.g. the items in a roller.
- **ticks** (*Optional*, dict): Ticks on scales for a meter.
- **cursor** (*Optional*, dict): The cursor part of the widget, e.g. the cursor in a spinbox.

```yaml
# Example slider with knob and indicator styling
- slider:
    # main (background) styles
    bg_opa: cover
    bg_grad: color_bar
    radius: 0
    indicator:
      bg_opa: transp    # Makes the indicator part invisible
    knob:
      radius: 1
      width: 4
      height: 10%
      bg_color: 0x000000
```

## Widget-specific properties

Some widgets have properties that are specific to that widget. For example `label` has a `text` property. The properties
specific to a widget are described in each section below. Some of these properties may also be updated with a corresponding
`lvgl.<widget_type>.update` action, e.g. `lvgl.label.update` allows updating not only the common style properties,
but also the `text` property of a label.

{{< anchor "text-property" >}}

## Text property

Several widgets have a `text` property, and the possible ways this can be specified are common to all `text` properties.
The text may be a Unicode string or other constant convertible to a string; if the string is enclosed in double quotes `""` then
standard escape sequences sucn as newline `\n` and Unicode codepoints will be translated.
The text value may also be a lambda returning a `std::string` or may be
specified with a `format` property utilising `printf` style formatting. There is also a `time_format` option
which allows use of [strftime](http://www.cplusplus.com/reference/ctime/strftime/) formats.

When formatting a single floating point value, it is possible to provide a substitute string to be used when the
value is `nan` or `inf`. The substitute string is specified with the `if_nan` option.

**Examples:**

```yaml
# Example label
- label:
    id: label_id
    text: "Text string"
- checkbox:
    id: checkbox_id
    text: "\uF00C"  # Unicode check-mark

on_...:
  - lvgl.label.update:
      id: label_id
      text:
        format: "%d bells, %d things"
        args: [x, 10]  # x is a value from the enclosing trigger.
  - lvgl.label.update:
      id: label_id
      text: !lambda return id(text_sensor).state;
  - lvgl.label.update:
      id: label_id
      text:
        time_format: "%c"   # uses default time component
  - lvgl.checkbox.update:
      id: checkbox_id
      text:
        time_format: "%c"
        time: sntp_id
  - lvgl.label.update:
      id: label_id
      text:
        time_format: "%c"
        time: !lambda return id(sntp_id).utcnow();
  - lvgl.label.update:
      id: value_id
      text:
        format: "%.1f"
        args: [id(sensor_id).state]
        if_nan: "N/A"
```

{{< anchor "lvgl-widget-animimg" >}}

## `animimg`

The animation image is similar to the normal `image` widget. The main difference is that instead of one source image, you set a list of multiple source images. You can also specify a duration and a repeat count.

{{< img src="lvgl_animimg.gif" alt="Image" class="align-center" >}}

**Configuration variables:**

- **src** (**Required**, list of [images](/components/image#display-image)): A list of IDs of existing image configurations to be loaded as frames of the animation.
- **auto_start** (*Optional*, boolean): Start the animation playback automatically at boot. Defaults to `true`.
- **duration** (**Required**, [Time](/guides/configuration-types#time)): Total duration of a playback cycle (each frame is displayed for an equal amount of time).
- **repeat_count** (*Optional*, int16 or *forever*): The number of times playback should be repeated. Defaults to `forever`.
- Some style options from [Style properties](/components/lvgl#lvgl-styling) for the background rectangle that uses the typical background style properties and the image itself using the image style properties.

**Actions:**

- `lvgl.animimg.start` [action](/automations/actions#actions-action) starts the animation playback if it was displayed with `auto_start` false or after `repeat_count` expired.
  - **id** (**Required**): The ID or a list of IDs of animimg widgets which you want start.

- `lvgl.animimg.stop` [action](/automations/actions#actions-action) stops the animation playback.
  - **id** (**Required**): The ID or a list of IDs of animimg widgets which you want stop.

- `lvgl.animimg.update` [action](/automations/actions#actions-action) can be used to change `repeat_count` and `duration`, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of animimg widgets to be updated.
  - **src** (*Optional*, list of [images](/components/image#display-image)): A list of IDs of existing image configurations to be loaded as frames of the animation.
  - **auto_start** (*Optional*, boolean): Start the animation playback automatically after update. Defaults to `true`.
  - **duration** (*Optional*, [Time](/guides/configuration-types#time)): Total duration of a playback cycle (each frame is displayed for an equal amount of time). This will apply to the next playback loop.
  - **repeat_count** (*Optional*, int16 or *forever*): The number of times playback should be repeated. Defaults to `forever`.
  - Some style options from [Style properties](/components/lvgl#lvgl-styling) for the background rectangle that uses the typical background style properties and the image itself using the image style properties.

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- animimg:
    align: CENTER
    id: anim_id
    src: [ cat_image, cat_image_bowtie ]
    duration: 1000ms

# Example actions:
on_...:
  then:
    - lvgl.animimg.update:
        id: anim_id
        repeat_count: 100
        duration: 300ms
```

See [Battery charging animation](/cookbook/lvgl#lvgl-cookbook-animbatt) in the Cookbook for a more detailed example.

{{< anchor "lvgl-widget-arc" >}}

## `arc`

The arc consists of a background and a foreground arc. The indicator foreground can be touch-adjusted with a knob.

{{< img src="lvgl_arc.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **adjustable** (*Optional*, boolean): Add a knob that the user can move to change the value. Defaults to `false`.
- **arc_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color used to draw the arc.
- **arc_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the arc.
- **arc_rounded** (*Optional*, boolean): Make the end points of the arcs rounded. `true` rounded, `false` perpendicular line ending.
- **arc_width** (*Optional*, int16): Set the width of the arcs in pixels.
- **change_rate** (*Optional*, uint16): Limits the speed at which the arc value changes when touched or dragged. The change rate is defined in degree/second. Defaults to `720`.
- **end_angle** (*Optional*, 0-360): end angle of the arc background (see note). Defaults to `45`.
- **indicator** (*Optional*, list): Settings for the indicator *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. Draws *another arc using the arc style* properties. Its padding values are interpreted relative to the background arc.
- **knob** (*Optional*, list): Settings for the knob *part* to control the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. Draws a handle on the end of the indicator using all background properties and padding values. With zero padding the knob size is the same as the indicator's width. Larger padding makes it larger, smaller padding makes it smaller.
- **max_value** (*Optional*, int16): Maximum value of the indicator. Defaults to `100`.
- **min_value** (*Optional*, int16): Minimum value of the indicator. Defaults to `0`.
- **mode** (*Optional*, string): `NORMAL`  : the indicator is drawn from the minimum value to the current. `REVERSE`  : the indicator is drawn counter-clockwise from the maximum value to the current. `SYMMETRICAL`  : the indicator is drawn from the middle point to the current value. Defaults to `NORMAL`.
- **rotation** (*Optional*, 0-360): Offset to the 0 degree position. Defaults to `0.0`.
- **start_angle** (*Optional*, 0-360): start angle of the arc background (see note). Defaults to `135`.
- **value** (*Optional*, int16): Actual value of the indicator at start, in `0`  -`100` range. Defaults to `0`.
- Any [Styling](/components/lvgl#lvgl-styling) and state-based option to override styles inherited from parent. The arc's size and position will respect the padding style properties.

If the `adv_hittest` [flag](#lvgl-widget-flags) is enabled the arc can be clicked through in the middle. Clicks are recognized only on the ring of the background arc.

> [!NOTE]
> The zero degree position is at the middle right (3 o'clock) of the widget and the degrees increase in a clockwise direction from there. Angles are specified in the `0`  -`360` range.

**Actions:**

- `lvgl.arc.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of arc widgets to be updated.
  - **change_rate** (*Optional*, uint16): New change rate in degree/second.
  - **end_angle** (*Optional*, 0-360): New end angle of the arc background.
  - **max_value** (*Optional*, int16): New maximum value of the indicator.
  - **min_value** (*Optional*, int16): New minimum value of the indicator.
  - **mode** (*Optional*, string): New indicator mode.
  - **rotation** (*Optional*, 0-360): New offset to the 0 degree position.
  - **start_angle** (*Optional*, 0-360): New start angle of the arc background.
  - **value** (*Optional*, int16): New value of the indicator.
  - Any [Styling](/components/lvgl#lvgl-styling) and state-based option to override styles inherited from parent. The arc's size and position will respect the padding style properties.

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the arc value changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- `on_change` [trigger](/automations/actions#actions-trigger) is activated when the arc value is changed by user interaction. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the value in `x`.

**Example:**

```yaml
# Example widget:
- arc:
    x: 10
    y: 10
    id: arc_id
    value: 75
    min_value: 0
    max_value: 100
    adjustable: true

# Example action:
on_...:
  then:
    - lvgl.arc.update:
        id: arc_id
        knob:
          bg_color: 0x00FF00
        value: 55

# Example trigger:
- arc:
    ...
    on_value:
      - logger.log:
          format: "Arc value is: %.0f"
          args: [ 'x' ]
```

> [!NOTE]
> The `on_value` and `on_change` triggers are sent as the arc knob is dragged or changed with keys. The event is sent *continuously* while the arc knob is being dragged; this generally has a negative effect on performance. To mitigate this, consider using a [universal interaction trigger](#lvgl-automation-triggers) like `on_release`, to get the `x` variable once after the interaction has completed.

The `arc` can be also integrated as a {{< docref "/components/number/lvgl" "Number" >}} or {{< docref "/components/sensor/lvgl" "Sensor" >}} component.

See [Light brightness slider](/cookbook/lvgl#lvgl-cookbook-bright) and [Media player volume slider](/cookbook/lvgl#lvgl-cookbook-volume) for examples which demonstrate how to use a slider (or an arc) to control entities in Home Assistant.

{{< anchor "lvgl-widget-bar" >}}

## `bar`

The bar widget has a background and an indicator foreground on it. The size of the indicator is set according to the current `value` of the bar.

{{< img src="lvgl_bar.png" alt="Image" class="align-center" >}}

Vertical bars can be created if the width is smaller than the height.

Not only the end, but also the start value of the bar can be set, which changes the start position of the indicator.

**Configuration variables:**

- **animated** (*Optional*, boolean): Animate the indicator on boot to the starting value. Defaults to `true`.
- **indicator** (*Optional*, int): Settings for the indicator *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize, all the typical background properties.
- **max_value** (*Optional*, int): Maximum value of the indicator. Defaults to `100`.
- **min_value** (*Optional*, int): Minimum value of the indicator. Defaults to `0`.
- **mode** (*Optional*, string): The mode of the bar. Defaults to `NORMAL`.
  - `NORMAL`  : the indicator is drawn from the minimum value to the current value.
  - `RANGE`  : the indicator is drawn from the start value to the current value.
  - `SYMMETRICAL`  : the indicator is symmetrically drawn from the middle point to the current value.
- **start_value** (*Optional*, int): For `RANGE` mode, the start value of the indicator. Defaults to `0`.
- **value** (*Optional*, int): Value for the indicator end, in `min_value`  -`max_value` range. Defaults to `0`.
- Style options from [Style properties](/components/lvgl#lvgl-styling). The background of the bar and it uses the typical background style properties. Adding padding will make the indicator smaller or larger.

**Actions:**

- `lvgl.bar.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of bar widgets to be updated.
  - Any of the bar options as listed above.
  - Style options from [Style properties](/components/lvgl#lvgl-styling). The background of the bar and it uses the typical background style properties. Adding padding will make the indicator smaller or larger.

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- bar:
    x: 10
    y: 100
    id: bar_id
    value: 75
    min_value: 1
    max_value: 100

# Example action:
on_...:
  then:
    - lvgl.bar.update:
        id: bar_id
        value: 55
```

The `bar` can be also integrated as {{< docref "/components/number/lvgl" "Number" >}} or {{< docref "/components/sensor/lvgl" "Sensor" >}} component.

{{< anchor "lvgl-widget-button" >}}

## `button`

Simple push (momentary) or toggle (two-states) button.

{{< img src="lvgl_button.png" alt="Image" class="align-center" >}}

A button has no inherent content so requires child widgets to be added. As a shorthand for a button with a single text label,
the `text:` option may be used to add a single `label` child, otherwise the `widgets:` key must be used to add other
widgets inside the button.

A button is momentary by default, which has a `pressed` state. If the `checkable` flag is set, it becomes a toggle button, which also has a `checked` state.

**Configuration variables:**

- **checkable** (*Optional*, boolean): A significant [flag](#lvgl-widget-flags) to make a toggle button (which reports its `checked` state). Defaults to `false`.
- **text** (*Optional*, string): Text to be displayed on the button. This will create and add a single label widget to the button. May not be used
  with the `widgets:` key.
- Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the button. Uses the typical background style properties.

A notable state is `checked` (boolean) which can have different styles applied.

**Triggers:**

- `on_change` [trigger](/automations/actions#actions-trigger) is activated after clicking. If `checkable` is `true`, the boolean variable `x`, representing the checked state, may be used by lambdas within this trigger.
- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the checked value changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget with text:
- button:
    id: btn_id
    text: "Click me!"
```

To create an image button, add a child [`image`](#lvgl-widget-image) widget to it:

```yaml
# Example toggle button with image:
- button:
    id: btn_id
    checkable: true
    widgets:
      - image:
          src: my_image_id

# Example trigger:
- button:
    ...
    on_value:
      then:
        - logger.log:
            format: "Button checked state: %d"
            args: [ x ]

```

The `button` can be also integrated as a {{< docref "/components/binary_sensor/lvgl" "Binary Sensor" >}} or as a {{< docref "/components/switch/lvgl" "Switch" >}} component.
> [!NOTE]
> A binary sensor linked to a button reports its `pressed` state, while a switch linked to a button reports its `checked` state.

See [Remote light button](/cookbook/lvgl#lvgl-cookbook-binent) for an example which demonstrates how to use a checkable button to act on a Home Assistant service.

**Actions:**

- `lvgl.button.update` [action](/automations/actions#actions-action) may be used to update the button styles at runtime. If
  the button has a `text:` option then it may also be updated with this action.
  - **id** (**Required**): The ID or a list of IDs of button widgets to be updated.
  - **text** (*Optional*, string): Update the button's text (only if the button was configured with the `text:` option).
  - Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the button.

  > [!NOTE]
  > Where other widgets are added as children, they must be updated directly.

```yaml
# Text update example
- button:
    id: btn_id
    text: "Click me!"
    on_click:
      lvgl.button.update:
        id: btn_id
        text: "Clicked"
```

{{< anchor "lvgl-widget-buttonmatrix" >}}

## `buttonmatrix`

The button matrix widget is a lightweight way to display multiple buttons in rows and columns. It's lightweight because the buttons are not actually created but instead simply drawn on the fly. This reduces the memory footprint of each button from approximately 200 bytes (for both the button and its label widget) down to only eight bytes.

{{< img src="lvgl_buttonmatrix.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **rows** (**Required**, list): A list for the button rows:
  - **buttons** (**Required**, list): A list of buttons in a row:
    - **id** (*Optional*): An ID for the button in the matrix.
    - **key_code** (*Optional*, string): One character be sent as the key code to a [Key collector component](/components/key_collector#key_collector) instead of `text` when the button is pressed.
    - **selected** (*Optional*, boolean): Set the button as the most recently released or focused. Defaults to `false`.
    - **text** (*Optional*, [Text property](#text-property)): Text to display on the button.
    - **width** (*Optional*): Width relative to the other buttons in the same row. Must be a value between `1` and `15`  ; the default is `1` (for example, given a line with two buttons, one with `width: 1` and another one with `width: 2`, the first will be `33%` wide while the second will be `66%` wide).
    - **control** (*Optional*): Binary flags to control behavior of the buttons (all `false` by default):
      - **checkable** (*Optional*, boolean): Enable toggling of a button, `checked` state will be added/removed as the button is clicked.
      - **checked** (*Optional*, boolean): Make the button checked. Apply `checked` styles to the button.
      - **click_trig** (*Optional*, boolean): Control how to [trigger](#lvgl-automation-triggers) `on_value` : if `true` on *click*, if `false` on *press*.
      - **custom_1** and **custom_2** (*Optional*, boolean): Custom, free to use flags.
      - **disabled** (*Optional*, boolean): Apply `disabled` styles to the button.
      - **hidden** (*Optional*, boolean): Make a button hidden (hidden buttons still take up space in the layout, they are just not visible or clickable).
      - **no_repeat** (*Optional*, boolean): Disable repeating when the button is long pressed.
      - **popover** (*Optional*, boolean): Show the button label in a popover when pressing this button.
      - **recolor** (*Optional*, boolean): Enable recoloring of button text with `#`. For example: `It's #FF0000 red#`

- **items** (*Optional*, list): Settings for the items *part*, the buttons all use the text and typical background style properties except translations and transformations.
- **one_checked** (*Optional*, boolean): Allow only one button to be checked at a time (aka. radio buttons). Defaults to `false`.
- Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the button matrix, uses the typical background style properties. `pad_row` and `pad_column` set the space between the buttons.

**Actions:**

- `lvgl.buttonmatrix.update` [action](/automations/actions#actions-action) updates the item styles and properties specified in the specific `state`, `items` options.
  - **id** (**Required**): The ID or a list of IDs of buttonmatrix widgets to be updated.
  - Widget styles or properties from `state`, `items` options above, to be updated.

- `lvgl.matrix.button.update` [action](/automations/actions#actions-action) updates the button styles and properties specified in the specific `control`, `width` and `selected` options.
  - **id** (**Required**): The ID or a list of IDs of matrix buttons to be updated.
  - Widget styles or properties from `control`, `width` and `selected` options above, to be updated.

**Triggers:**

- `on_value` and [interaction](#lvgl-automation-triggers) triggers can be configured for each button, is activated after clicking. If `checkable` is `true`, the boolean variable `x`, representing the checked state, may be used by lambdas within this trigger.
- The [interaction](#lvgl-automation-triggers) LVGL event triggers can be configured for the main widget, they pass the ID of the pressed button (or null if nothing pressed) as variable `x` (a pointer to a `uint16_t` which holds the index number of the button).

**Example:**

```yaml
# Example widget:
- buttonmatrix:
    x: 10
    y: 40
    width: 220
    items:
      pressed:
        bg_color: 0xFFFF00
    id: matrix_id
    rows:
      - buttons:
        - id: button_1
          text: "\uF04B"
          control:
            checkable: true
        - id: button_2
          text: "\uF04C"
          control:
            checkable: true
      - buttons:
        - id: button_3
          text: "A"
          control:
            popover: true
        - id: button_4
          text: "B"
          control:
            disabled: true
      - buttons:
        - id: button_5
          text: "It's #ff0000 red#"
          width: 2
          control:
            recolor: true

# Example action:
on_...:
  then:
    - lvgl.matrix.button.update:
        id: button_1
        width: 1
        selected: true
        control:
          checkable: false
    - lvgl.buttonmatrix.update:
        id: matrix_id
        state:
          disabled: true
        items:
          bg_color: 0xf0f0f0

# Example trigger:
- buttonmatrix:
    ...
    rows:
      - buttons:
        ...
        - id: button_2
          ...
          control:
            checkable: true
          on_value: # Trigger for the individual button, returning the checked state
            then:
              - logger.log:
                  format: "Button 2 checked: %d"
                  args: [ x ]
    on_press: # Triggers for the matrix, to determine which button was pressed.
      logger.log:
        format: "Matrix button pressed: %d"
        args: ["x"]  # If x is 65535, it was the container, (or through a disabled button).
    on_click:
      logger.log:
        format: "Matrix button clicked: %d, is button_2 = %u"
        args: ["x", "id(button_2) == x"]
```

> [!TIP]
> The Button Matrix widget supports the [Key collector component](/components/key_collector#key_collector) to collect the button presses as key press sequences for further automations. Check out [A numeric input keypad](/cookbook/lvgl#lvgl-cookbook-keypad) for an example.

## `canvas`

The canvas widget provides a surface for custom drawing operations. It allows you to draw shapes, text, images and perform pixel-level manipulations.
All options are templatable.
Where a list of points is required, this can be provided in the form of a list of dictionaries, each with templatable `x` and `y` keys, or as a shorthand `x,y` pair (constant integers only.)

{{< img src="canvas.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **width** (**Required**, int): Width of the canvas in pixels.
- **height** (**Required**, int): Height of the canvas in pixels.
- **transparent** (*Optional*, boolean): Enable alpha channel support. Defaults to `false`.

**Actions:**

- `lvgl.canvas.fill` fills the entire canvas with a color:
  - **id** (**Required**): The ID of the canvas widget.
  - **color** (**Required**, [color](/components/lvgl#lvgl-color)): Fill color.
  - **opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the fill. Defaults to `COVER`.

- `lvgl.canvas.set_pixels` sets individual pixels:
  - **id** (**Required**): The ID of the canvas widget.
  - **color** (**Required**, [color](/components/lvgl#lvgl-color)): Pixel color.
  - **opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the pixels. Defaults to `COVER`.
  - **points** (**Required**, list): List of points to set, each with:
    - **x** (**Required**, int): X coordinate.
    - **y** (**Required**, int): Y coordinate.

- `lvgl.canvas.draw_rectangle` draws a rectangle:
  - **id** (**Required**): The ID of the canvas widget.
  - **x** (**Required**, int): X coordinate.
  - **y** (**Required**, int): Y coordinate.
  - **width** (**Required**, int): Width in pixels
  - **height** (**Required**, int): Height in pixels
  - **radius** (*Optional*, int): Corner radius.
  - **bg_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Background color.
  - **bg_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Background opacity. Defaults to `COVER`.
  - **border_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Border color.
  - **border_width** (*Optional*, int): Border width.
  - **border_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Border opacity. Defaults to `COVER`.
  - **outline_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Outline color.
  - **outline_width** (*Optional*, int): Outline width.
  - **outline_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the outline. Defaults to `COVER`.
  - **outline_pad** (*Optional*, int): Padding of the outline. Defaults to `0`.
  - **shadow_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Shadow color.
  - **shadow_width** (*Optional*, int): Shadow width.
  - **shadow_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the shadow. Defaults to `COVER`.
  - **shadow_offset_x** (*Optional*, int): Shadow offset X.
  - **shadow_offset_y** (*Optional*, int): Shadow offset Y.
  - **shadow_spread** (*Optional*, int): Shadow spread.

- `lvgl.canvas.draw_polygon` draws a polygon:
  - **id** (**Required**): The ID of the canvas widget.
  - **points** (**Required**, list): List of points forming the polygon vertices.
  - Other options as for `lvgl.canvas.draw_rectangle`.

- `lvgl.canvas.draw_text` draws text:
  - **id** (**Required**): The ID of the canvas widget.
  - **x** (**Required**, int): X coordinate.
  - **y** (**Required**, int): Y coordinate.
  - **text** (**Required**, string): Text to draw.
  - **max_width** (**Required**, int): Max width in pixels.
  - **align** (*Optional*, enum): Alignment of the text relative to `x` and `max_width`. One of `LEFT`, `CENTER`, `RIGHT`, `AUTO`.
  - **color** (*Optional*, [color](/components/lvgl#lvgl-color)): Text color.
  - **opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Text opacity. Defaults to `COVER`.
  - **font** (*Optional*, string): Font to use.
  - **decor** (*Optional*, list): Choose decorations for the text: `NONE`, `UNDERLINE`, `STRIKETHROUGH` (multiple can be specified as YAML list). Defaults to `NONE`.
  - **letter_space** (*Optional*, int16): Extra character spacing of the text. Defaults to `0`.
  - **line_space** (*Optional*, int16): Line spacing of the text. Defaults to `0`.

- `lvgl.canvas.draw_line` draws a line:
  - **id** (**Required**): The ID of the canvas widget.
  - **points** (**Required**, list): List of points forming the line, each with:
    - **x** (**Required**, int): X coordinate.
    - **y** (**Required**, int): Y coordinate.
  - **color** (*Optional*, [color](/components/lvgl#lvgl-color)): Line color.
  - **width** (*Optional*, int): Line width.
  - **opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Line opacity. Defaults to `COVER`.
  - **round_start** (*Optional*, boolean): Round the start of the line. Defaults to `false`.
  - **round_end** (*Optional*, boolean): Round the end of the line. Defaults to `false`.

- `lvgl.canvas.draw_arc` draws an arc:
  - **id** (**Required**): The ID of the canvas widget.
  - **x** (**Required**, int): Center X coordinate.
  - **y** (**Required**, int): Center Y coordinate.
  - **radius** (**Required**, int): Arc radius.
  - **start_angle** (**Required**, 0-360): Start angle.
  - **end_angle** (**Required**, 0-360): End angle.
  - **color** (*Optional*, [color](/components/lvgl#lvgl-color)): Arc color.
  - **width** (*Optional*, int): Arc line width.
  - **opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Arc opacity. Defaults to `COVER`.
  - **rounded** (*Optional*, boolean): Round the start/end of the arc.

- `lvgl.canvas.draw_image` draws an image:
  - **id** (**Required**): The ID of the canvas widget.
  - **x** (**Required**, int): X coordinate.
  - **y** (**Required**, int): Y coordinate.
  - **src** (**Required**, string): Image source.
  - **angle** (*Optional*, 0-360): Rotation angle.
  - **zoom** (*Optional*, float): Zoom factor 0.1-10.0 (default 1.0)
  - **pivot_x** (*Optional*, int): X pivot point for rotation.
  - **pivot_y** (*Optional*, int): Y pivot point for rotation.

```yaml
# Example widget:
- canvas:
    id: my_canvas
    width: 240
    height: 240
    transparent: false

# Example drawing actions, executed at startup:
    on_boot:
      then:
        # Fill canvas with white
        - lvgl.canvas.fill:
            id: my_canvas
            color: white
        # Draw a blue rectangle
        - lvgl.canvas.draw_rectangle:
            id: my_canvas
            x: 10
            y: 10
            width: 100
            height: 50
            bg_color: blue
        # Draw some red text
        - lvgl.canvas.draw_text:
            id: my_canvas
            x: 20
            y: 20
            text: "Hello World"
            max_width: 100
            color: red
```

{{< anchor "lvgl-widget-checkbox" >}}

## `checkbox`

The checkbox widget is made internally from a *tick box* and a label. When the checkbox is clicked the tick box's `checked` state will be toggled.

{{< img src="lvgl_checkbox.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **indicator** (*Optional*, list): Settings for the indicator *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. The "tick box" is a square that uses all the typical background style properties. By default, its size is equal to the height of the main part's font. Padding properties make the tick box larger in the respective directions.
- Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the widget and it uses the text and all the typical background style properties. `pad_column` adjusts the spacing between the tick box and the label.

**Actions:**

- `lvgl.checkbox.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of checkbox widgets to be updated.
  - **text** (*Optional*, [Text property](#text-property)): Text to display beside the checkbox.
  - Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the widget and it uses the text and all the typical background style properties. `pad_column` adjusts the spacing between the tick box and the label.

**Triggers:**

- `on_change` [trigger](/automations/actions#actions-trigger) is activated when interactively toggling the checkbox. The boolean variable `x`, representing the checkbox's state, may be used by lambdas within this trigger.
- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the checkbox is toggled, either by user interaction or programmatically. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the value in `x`.

**Example:**

```yaml
# Example widget:
- checkbox:
    x: 10
    y: 10
    id: checkbox_id
    text: Checkbox

# Example action:
on_...:
  then:
    - lvgl.checkbox.update:
        id: checkbox_id
        state:
          checked: true
        text: Checked

# Example trigger:
- checkbox:
    ...
    on_value:
      then:
        - logger.log:
            format: "Checkbox state: %d"
            args: [ x ]
```

> [!NOTE]
> In case you configure `default_font` in the main section to a custom font, the checkmark will not be shown correctly when the checkbox is in the checked state. See [Restore checkbox mark](/cookbook/lvgl#lvgl-cookbook-ckboxmark) for how to easily resolve this.

The `checkbox` can be also integrated as a {{< docref "/components/switch/lvgl" "Switch" >}} component.

{{< anchor "lvgl-widget-container" >}}

## `container`

A `container` is an unstyled widget that is, as the name suggests, intended to act as a container for other widgets.
It is functionally identical to an [`obj`](#obj) but has no styles applied to it, so without any styling or widgets
it is invisible. It has a default width and height of 100%.

**Configuration variables:**

- Style options from [Style properties](/components/lvgl#lvgl-styling).

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- container:
    x: 10
    y: 10
    width: 220
    height: 300
    widgets:
      - ...
```

{{< anchor "lvgl-widget-dropdown" >}}

## `dropdown`

The dropdown widget allows the user to select one value from a list.

The dropdown list is closed by default and displays a single value. When activated (by clicking on the drop-down list), a list is drawn from which the user may select one option. When the user selects a new value, the list is deleted from the screen.

{{< img src="lvgl_dropdown.png" alt="Image" class="align-center" >}}

The Dropdown widget is built internally from a *button* part and a *list* part (both not related to the actual widgets with the same name).

**Configuration variables:**

- **dir** (*Optional*, str): Where the list part of the dropdown gets created relative to the button part. `LEFT`, `RIGHT`, `BOTTOM`, `TOP`, defaults to `BOTTOM`.
- **dropdown_list** (*Optional*, dict): Settings for the the list with items. Supports a list of [styles](/components/lvgl#lvgl-styling) to customize. Notable are `text_line_space` and `pad_all` for spacing of list items, and `text_font` to separately change the font in the list. The parts `main`, `scrollbar` and `selected` may be customised. Note that changing styles on the selected item should be done in the `selected` part with `checked` state. `max_height` can be used to limit the height of the list.
- **indicator** (*Optional*, dict): Styles for the dropdown symbol.
- **options** (**Required**, list): The list of available options in the drop-down.
- **selected_index** (*Optional*, int8): The index of the item you wish to be selected.
- **selected_text** (*Optional*, string): The text of the item you wish to be selected.
- **symbol** (*Optional*, dict): A symbol (typically an chevron) is shown in dropdown list. If `dir` of the drop-down list is `LEFT` the symbol will be shown on the left, otherwise on the right. Choose a different [symbol](/components/lvgl#lvgl-fonts) from those built-in or from your own customized font.
- Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the button. Uses the typical background properties and [`label`](#lvgl-widget-label) text properties for the text on it. `text_font` can be used to set the font of the button part, including the symbol.

**Actions:**

- `lvgl.dropdown.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of dropdown widgets to update.
  - **dir** (*Optional*, str): Where the list part of the dropdown gets created relative to the button part. `LEFT`, `RIGHT`, `BOTTOM`, `TOP`.
  - **options** (*Optional*, list): The list of available options in the drop-down.
  - All other configuration variables from above are optional and have the same function as previously described.

**Triggers:**

- `on_change` [trigger](/automations/actions#actions-trigger) is activated only when the user selects an item from the list. The new selected index is returned in the variable `x`. The [interaction](#lvgl-automation-triggers) LVGL event triggers also apply, and they also return the selected index in `x`.
- `on_value` [trigger](/automations/actions#actions-trigger) is activated the selection changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- `on_cancel` [trigger](/automations/actions#actions-trigger) is also activated when you close the dropdown without selecting an item from the list. The currently selected index is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the value in `x`.

**Example:**

```yaml
# Example widget:
- dropdown:
    id: dropdown_id
    width: 90
    align: CENTER
    options:
      - Violin
      - Piano
      - Bassoon
      - Chello
      - Drums
    selected_index: 2
    dropdown_list:
      selected:
        checked:
          text_color: 0xFF0000

# Example action:
on_...:
  then:
    - lvgl.dropdown.update:
        id: dropdown_id
        selected_index: 4

# Example trigger:
- dropdown:
    ...
    on_value:
      - logger.log:
          format: "Selected index is: %d"
          args: [ x ]
    on_cancel:
      - logger.log:
          format: "Dropdown closed. Selected index is: %d"
          args: [ x ]
```

The `dropdown` can be also integrated as {{< docref "/components/select/lvgl" "Select" >}} component.

{{< anchor "lvgl-widget-image" >}}

## `image`

Images are the basic widgets used to display images.

{{< img src="lvgl_image.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **src** (**Required**, [image](/components/image#display-image)): The ID of an existing image configuration.
- **angle** (*Optional*, 0-360): Rotation of the image. Defaults to `0.0`.
- **antialias** (*Optional*): The quality of the angle or scale transformation. When anti-aliasing is enabled, the transformations are higher quality but slower. Defaults to `false`.
- **mode** (*Optional*): Either `REAL` or `VIRTUAL`. With `VIRTUAL`, when the image is scaled or rotated, the real coordinates of the image object are not changed. The larger content simply overflows the object's boundaries. It also means the layouts are not affected the by the transformations. With `REAL`, if the width/height of the object is set to `SIZE_CONTENT`, the object's size will be set to the scaled and rotated size. If an explicit size is set, the overflowing content will be cropped. Defaults to `VIRTUAL`.
- **offset_x** (*Optional*): Add a horrizontal offset to the image position.
- **offset_y** (*Optional*): Add a vertical offset to the image position.
- **pivot_x** (*Optional*): Horizontal position of the pivot point of rotation, in pixels, relative to the top left corner of the image. Defaults to the center of the image. Must be specified along with `pivot_y`.
- **pivot_y** (*Optional*): Vertical position of the pivot point of rotation, in pixels, relative to the top left corner of the image. Defaults to the center of the image. Must be specified along with `pivot_x`
- **zoom** (*Optional*, 0.1-10): Zoom of the image.
- **image_recolor** (*Optional*, [color](/components/lvgl#lvgl-color)): Color to mix with every pixel of an image Note that `image_recolor_opa` defaults to TRANSP, so it must also be set.
- **image_recolor_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the image recoloring.

- Some style options from [Style properties](/components/lvgl#lvgl-styling) for the background rectangle that uses the typical background style properties and the image itself using the image style properties.

**Actions:**

- `lvgl.image.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of image widgets to be updated.
  - All other configuration variables from above are optional and have the same function as previously described. Updating the `src` option changes the image at runtime.

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- image:
    align: CENTER
    src: cat_image
    id: img_id
    radius: 11
    clip_corner: true

# Example action:
on_...:
  then:
    - lvgl.image.update:
        id: img_id
        src: cat_image_bowtie
```

> [!NOTE]
> Currently `RGB565` type images are supported, with transparency using the optional parameter `transparency` set. See [Images](/components/image#display-image) for how to load an image for rendering in ESPHome.
> [!TIP]
> `offset_x` and `offset_y` can be useful when the widget size is set to be smaller than the image source size. A "running image" effect can be created by animating these values.

{{< anchor "lvgl-widget-keyboard" >}}

## `keyboard`

The keyboard widget is a special Button matrix with predefined keymaps and other features to show an on-screen keyboard usable to type text into a [`textarea`](#lvgl-widget-textarea).

{{< img src="lvgl_keyboard.png" alt="Image" class="align-center" >}}

For styling, the `keyboard` widget uses the same settings as [`buttonmatrix`](#lvgl-widget-buttonmatrix).

**Configuration variables:**

- **textarea** (*Optional*): The ID of a `textarea` to associate with the keyboard. If provided, all key entries are recorded in the `textarea`.
- **mode** (*Optional*, enum): Keyboard layout to use. Each `TEXT_` layout contains a button to allow the user to iterate through the `TEXT_` layouts.
  - `TEXT_LOWER`  : Display lower case letters (default).
  - `TEXT_UPPER`  : Display upper case letters.
  - `TEXT_SPECIAL`  : Display special characters.
  - `NUMBER`  : Display numbers, +/- sign, and decimal dot.

**Actions:**

- `lvgl.keyboard.update` [action](/automations/actions#actions-action) updates the properties from the specific options above, plus any from [lvgl.widget.update](#lvgl-automation-actions).
  - **id** (**Required**): The ID or a list of IDs of keyboard widgets which you want to update.
  - **mode** (*Optional*, enum): Keyboard layout to use.
  - All other configuration variables from above are optional and have the same function as previously described.

**Triggers:**

- `on_ready` [trigger](/automations/actions#actions-trigger) is activated when the checkmark key is pressed.
- `on_cancel` [trigger](/automations/actions#actions-trigger) is activated when the key containing the keyboard icon is pressed.

**Example:**

```yaml
# Example widget:
- keyboard:
    id: keyboard_id
    textarea: textarea_1
    mode: TEXT_UPPER

# Example actions:
on_focus:
  then:
    - lvgl.keyboard.update:
        id: keyboard_id
        mode: number
        textarea: textarea_2

# Example trigger:
- keyboard:
    ...
    on_ready:
      then:
        - logger.log: Keyboard is ready
    on_cancel:
      then:
        - logger.log: Keyboard cancelled
```

> [!TIP]
> The Keyboard widget supports the [Key collector component](/components/key_collector#key_collector) to collect the button presses as key press sequences for further automations.

> [!NOTE]
> The Keyboard widget in ESPHome doesn't support popovers or custom layouts.

{{< anchor "lvgl-widget-label" >}}

## `label`

A label is the basic widget type that is used to display text.

{{< img src="lvgl_label.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **long_mode** (*Optional*, list): By default, the width and height of the label is set to `SIZE_CONTENT`. Therefore, the size of the label is automatically expanded to the text size. Otherwise, if the `width` or `height` are explicitly set (or set by [Layouts](/components/lvgl/layouts#lvgl-layouts)), the lines wider than the label's width can be manipulated according to the long mode policies below. These policies can be applied if the height of the text is greater than the height of the label.
  - `WRAP`  : Wrap lines which are too long. If the height is `SIZE_CONTENT`, the label's height will be expanded, otherwise the text will be clipped (default).
  - `DOT`  : Replaces the last 3 characters from bottom right corner of the label with dots.
  - `SCROLL`  : If the text is wider than the label, scroll the text horizontally back and forth. If it's higher, scroll vertically. Text will scroll in only one direction; horizontal scrolling has higher precedence.
  - `SCROLL_CIRCULAR`  : If the text is wider than the label, continuously scroll the text horizontally. If it's higher, scroll vertically. Text will scroll in only one direction; horizontal scrolling has higher precedence.
  - `CLIP`  : Simply clip the parts of the text outside the label.
- **recolor** (*Optional*, boolean): Enable recoloring of button text with `#`. This makes it possible to set the color of characters in the text individually by prefixing the text to be re-colored with a `#RRGGBB` hexadecimal color code followed by a *space*, and finally closed with a single hash `#` tag. For example: `Write a #FF0000 red# word`.
- **scrollbar** (*Optional*, list): Settings for the indicator *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. The scroll bar that is shown when the text is larger than the widget's size.
- **selected** (*Optional*, list): Settings for the style of the selected text. Only `text_color` and `bg_color` style properties can be used.
- **text_align** (*Optional*, enum): Alignment of the text in the widget - it doesn't align the object itself, only the lines inside the object. One of `LEFT`, `CENTER`, `RIGHT`, `AUTO`. Inherited from parent. Defaults to `AUTO`, which detects the text base direction and uses left or right alignment accordingly.
- **text_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color to render the text in. Inherited from parent. Defaults to `0` (black).
- **text_decor** (*Optional*, list): Choose decorations for the text: `NONE`, `UNDERLINE`, `STRIKETHROUGH` (multiple can be specified as YAML list). Inherited from parent. Defaults to `NONE`.
- **text_font**: (*Optional*, [font](/components/lvgl#lvgl-fonts)): The ID of the font used to render the text or symbol. Inherited from parent.
- **text_letter_space** (*Optional*, int16): Extra character spacing of the text. Inherited from parent. Defaults to `0`.
- **text_line_space** (*Optional*, int16): Line spacing of the text. Inherited from parent. Defaults to `0`.
- **text_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the text. Inherited from parent. Defaults to `COVER`.
- **text** (*Optional*, [Text property](#text-property)): Text to display on the label.
- Style options from [Style properties](/components/lvgl#lvgl-styling). Uses all the typical background properties and the text properties. The padding values can be used to add space between the text and the background.

> [!NOTE]
> Newline escape sequences are handled automatically by the label widget. You can use `\n` to make a line break. For example: `"line1\nline2\n\nline4"`. For escape sequences like newline to be translated, *enclose the string in double quotes*.

**Actions:**

- `lvgl.label.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of label widgets to be updated.
  - **text** (*Optional*, [Text property](#text-property)): New text to display on the label.
  - All other configuration variables from above are optional and have the same function as previously described.

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- label:
    align: CENTER
    id: lbl_id
    recolor: true
    text: "#FF0000 write# #00FF00 colored# #0000FF text#"

- label:
    align: TOP_MID
    id: lbl_symbol
    text_font: montserrat_28
    text: "\uF013"

# Example action (update label with a value from a sensor):
on_...:
  then:
    - lvgl.label.update:
        id: lbl_id
        text:
          format: "%.0fdBm"
          args: [ 'id(wifi_signal_db).get_state()' ]
```

The `label` can be also integrated as {{< docref "/components/text/lvgl" "Text" >}} or {{< docref "/components/text_sensor/lvgl" "Text Sensor" >}} component.

{{< anchor "lvgl-widget-led" >}}

## `led`

The LED widgets are either circular or rectangular widgets whose brightness can be adjusted. As their brightness decreases, the colors become darker.

{{< img src="lvgl_led.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **brightness** (*Optional*, percentage): The brightness of the LED color, where `0%` corresponds to black, and `100%` corresponds to the full brightness of the color specified above.
- **color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color for the background, border, and shadow of the widget.
- Style options from [Style properties](/components/lvgl#lvgl-styling), using all the typical background style properties.

**Actions:**

- `lvgl.led.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of led widgets to be updated.
  - All other configuration variables from above are optional and have the same function as previously described.

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- led:
    id: led_id
    align: CENTER
    color: 0xFF0000
    brightness: 70%

# Example action:
on_...:
  then:
    - lvgl.led.update:
        id: led_id
        color: 0x00FF00
```

The `led` can be also integrated as {{< docref "/components/light/lvgl" "Light" >}} component.

> [!NOTE]
> If configured as a light component, `color` and `brightness` are overridden by the light at startup, according to its `restore_mode` setting.

Check out [A numeric input keypad](/cookbook/lvgl#lvgl-cookbook-keypad) in the Cookbook for an example which demonstrates how to change the `led` styling properties from an automation.

{{< anchor "lvgl-widget-line" >}}

## `line`

The line widget is capable of drawing straight lines between a set of points.

{{< img src="lvgl_line.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **line_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color for the line.
- **line_dash_gap** (*Optional*, int16): Set the width of the gap between the dashes in the line (in pixels).
- **line_dash_width** (*Optional*, int16): Set the width of the dashes in the line (in pixels).
- **line_rounded** (*Optional*, boolean): Make the end points of the line rounded. `true` rounded, `false` perpendicular line ending.
- **line_width** (*Optional*, int16): Set the width of the line in pixels.
- **points** (**Required**, list): A list of `x, y` integer pairs for point coordinates (origin from top left of parent)
- Style options from [Style properties](/components/lvgl#lvgl-styling), all the typical background properties and line style properties.

By default, the Line widget width and height dimensions are set to `SIZE_CONTENT`. This means it will automatically set its size to fit all the points. If the size is set explicitly, parts of the line may not be visible.

The points list may be defined with constants in the form `x, y` or as a list of dictionaries with `x` and `y` keys. The latter allows for more complex point definitions, such as using a lambda function to calculate the coordinates.

**Actions:**

- `lvgl.line.update` [action](/automations/actions#actions-action) updates the points and any style properties.
  - **id** (**Required**): The ID or a list of IDs of lines to update.
  - **points** (**Required**): A point list as described above.

**Example:**

```yaml
# Example widget:
- line:
    points:
      - 5, 5
      - 70, 70
      - 120, 10
      - x: !lambda return random_uint32() % 100;
        y: !lambda return random_uint32() % 100;
      - 230, 15
    line_width: 8
    line_color: 0x0000FF
    line_rounded: true
```

{{< anchor "lvgl-widget-meter" >}}

## `meter`

The meter widget can visualize data in very flexible ways. It can use arcs, needles, ticks, lines and/or labels.

{{< img src="lvgl_meter.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **scales** (**Required**, list): A list with (any number of) scales to be added to the meter.
  - **angle_range** (**Required**): The angle between start and end of the tick scale. Defaults to `270`.
  - **indicators** (**Required**, list): A list with indicators to be added to the scale. Multiple of each can be added. Their values are interpreted in the range of the scale:
    - **arc** (*Optional*): Add a background arc the scale:
      - **color**: [Color](/components/lvgl#lvgl-color) to draw the arc. Defaults to `0` (black).
      - **end_value**: The value in the scale range to end drawing the arc to.
      - **r_mod**: Adjust the position of the arc from the scale radius with this amount (can be negative). Defaults to `0`.
      - **start_value**: The value in the scale range to start drawing the arc from.
      - **width**: Arc width in pixels. Defaults to `4`.
      - **opa**: Opacity of the arc. Defaults to 100%.
    - **image** (*Optional*): Add a rotating needle image to the scale:
      - **id**: Manually specify the [ID](/guides/configuration-types#id) used for updating the indicator value at runtime.
      - **pivot_x**: Horizontal position of the pivot point of rotation, in pixels, relative to the top left corner of the image.
      - **pivot_y**: Vertical position of the pivot point of rotation, in pixels, relative to the top left corner of the image.
      - **src**: The ID of an existing image configuration, representing a needle pointing to the right like `-o--->`.
      - **value**: The value in the scale range to show at start.
      - **opa**: Opacity of the image. Defaults to 100%.
    - **line** (*Optional*): Add a needle line to the scale. By default, the length of the line is the same as the scale's radius:
      - **color**: [Color](/components/lvgl#lvgl-color) for the needle line. Defaults to `0` (black).
      - **id**: Manually specify the [ID](/guides/configuration-types#id) used for updating the indicator value at runtime.
      - **r_mod**: Adjust the length of the needle from the scale radius with this amount (can be negative). Defaults to `0`.
      - **value**: The value in the scale range to show at start.
      - **width**: Needle line width in pixels. Defaults to `4`.
      - **opa**: Opacity of the needle. Defaults to 100%.
    - **tick_style** (*Optional*): Add tick style modifications:
      - **color_end**: [Color](/components/lvgl#lvgl-color) for the gradient end of the ticks.
      - **color_start**: [Color](/components/lvgl#lvgl-color) for the gradient start of the ticks.
      - **end_value**: The value in the scale range to modify the ticks to.
      - **local**: If `true` the ticks' color will be faded from `color_start` to `color_end` in the start and end values specified above. If `false`, `color_start` and `color_end` will be mapped to the entire scale range (and only a *slice* of that color gradient will be visible in the indicator's start and end value range). Defaults to `false`.
      - **start_value**: The value in the scale range to modify the ticks from.
      - **width**: Modifies the `width` of the tick lines.
  - **range_from** (**Required**): The minimum value of the tick scale. Defaults to `0`.
  - **range_to** (**Required**): The maximum value of the tick scale. Defaults to `100`.
  - **rotation** (*Optional*): The rotation angle offset of the tick scale.
  - **ticks** (**Required**, list): A scale can have minor and major ticks and labels on the major ticks. To add the minor ticks:
    - **color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color to draw the ticks. Required if `count` is greater than `0`. Defaults to `0x808080`.
    - **count** (**Required**): How many ticks to be on the scale. Defaults to `12`.
    - **length** (*Optional*): Tick line length in pixels. Required if `count` is greater than `0`. Defaults to `10`.
    - **width** (*Optional*): Tick line width in pixels. Required if `count` is greater than `0`. Defaults to `2`.
    - **major** (*Optional*, list): If you want major ticks and value labels displayed:
      - **color**: [Color](/components/lvgl#lvgl-color) to draw the major ticks. Defaults to `0` (black).
      - **label_gap**: Label distance from the ticks with text proportional to the values of the tick line. Defaults to `4`.
      - **length**: Tick line length in pixels or percentage. Defaults to `15%`.
      - **stride**: How many minor ticks to skip when adding major ticks. Defaults to `3`.
      - **width**: Tick line width in pixels. Defaults to `5`.
    - Style options from [Style properties](/components/lvgl#lvgl-styling) for the tick *lines* and *labels* using the [`line`](#lvgl-widget-line) and [`label`](#lvgl-widget-label) text style properties.
- Style options from [Style properties](/components/lvgl#lvgl-styling) for the background of the meter, using the typical background properties.
- **ticks** (*Optional*, dict): Styling options for the ticks *part*, which will be applied to the tick lines and labels using standard *line* and *label* styles.
- **indicator** (*Optional*, dict): Styling options for the indicator *part*, which will be applied to the needle line or image using standard *line* and *image* styles. Background properties applied here will style the pivot (the dot in the middle of the meter) for example to hide the pivot use `bg_opa: transp` in the `indicator` style.
- **items** (*Optional*, dict): Settings for the items *part*, which will be applied to arcs.

> [!NOTE]
> The zero degree position is at the middle right (3 o'clock) of the widget and the degrees increase in a clockwise direction from there. Angles are specified in the `0`  -`360` range.

**Actions:**

- `lvgl.indicator.update` [action](/automations/actions#actions-action) updates indicator options as below. [lvgl.widget.update](#lvgl-automation-actions) action can be used for the common styles, states or flags of the meter widget.
  - **id** (**Required**): The ID or a list of IDs of indicators to update.
  - **end_value** (*Optional*): The value in the scale range to end drawing the arc to.
  - **start_value** (*Optional*): The value in the scale range to start drawing the arc from.
  - **opa** (*Optional*): Opacity of the indicator.

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- meter:
    align: center
    scales:
      range_from: -10
      range_to: 40
      angle_range: 240
      rotation: 150
      ticks:
        count: 51
        length: 3
        major:
          stride: 5
          length: 13
          label_gap: 13
      indicators:
        - line:
            id: temperature_needle
            width: 2
            color: 0xFF0000
            r_mod: -4
        - tick_style:
            start_value: -10
            end_value: 40
            color_start: 0x0000bd #FF0000
            color_end: 0xbd0000 #0000FF

# Example action:
on_...:
  then:
    - lvgl.indicator.update:
        id: temperature_needle
        value: 3
```

See [Semicircle gauge](/cookbook/lvgl#lvgl-cookbook-gauge), [Thermometer](/cookbook/lvgl#lvgl-cookbook-thermometer) and [An analog clock](/cookbook/lvgl#lvgl-cookbook-clock) in the Cookbook for examples which demonstrate how to effectively use this widget.

{{< anchor "lvgl-widget-msgbox" >}}

## `msgboxes`

The message boxes act as pop-ups. They are built from a background container, a title, an optional close button, a text and optional buttons.

{{< img src="lvgl_msgbox.png" alt="Image" class="align-center" >}}

The text will be broken into multiple lines automatically and the height will be set automatically to include the text and the buttons. The message box is modal (blocks clicks on the rest of the screen until closed).

**Configuration variables:**

- **msgboxes** (*Optional*, list): A list of message boxes to use. This option is available only at the top level of the LVGL component configuration. Each list entry may have the following options:
  - **title** (**Required**, string): A string to display at the top of the message box.
  - **body** (*Optional*, dict): The content of the body of the message box:
    - **text** (*Optional*, [Text property](#text-property)): The text to display in the body of the message box.
    - Style options from [Style properties](/components/lvgl#lvgl-styling). Uses all the typical background properties and the text properties.
  - **buttons** (*Optional*, list): A list of buttons to show at the bottom of the message box:
    - **text** (*Optional*, [Text property](#text-property)): Text to display on the button.
    - See [`buttonmatrix`](#lvgl-widget-buttonmatrix) for other options for the buttons.
  - **button_style** (*Optional*, dict): A style to apply to the buttons. Uses all the typical style properties. Buttons cannot be individually styled since they are part of a `buttonmatrix`.
  - **close_button** (*Optional*, boolean): Controls the presence of the close button to the top right of the message box. Defaults to true

**Actions:**

The configured message boxes are hidden by default. They can be shown and hidden using `lvgl.widget.show` and `lvgl.widget.hide` respectively [actions](#lvgl-automation-shorthands).

**Example:**

```yaml
# Example widget:
lvgl:
  ...
  msgboxes:
    - id: message_box
      close_button: true
      title: Message box
      body:
        text: "This is a sample message box."
        bg_color: 0x808080
      buttons:
        - id: msgbox_apply
          text: "Apply"
        - id: msgbox_close
          text: "\uF00D"
          on_click:
            then:
              - lvgl.widget.hide: message_box
```

> [!TIP]
> You can create your own more complex dialogs with a full-screen sized, half-opaque `obj` with any child widgets on it, and the `hidden` flag set to `true` by default. For non-modal dialogs, simply set the `clickable` flag to `false` on it.

{{< anchor "lvgl-widget-obj" >}}

## `obj`

The base object is just a simple, empty widget. By default, it's nothing more than a rounded rectangle:

{{< img src="lvgl_baseobj.png" alt="Image" class="align-center" >}}

You can use it as a parent container for other widgets. By default, it catches touches. Since `obj` has default
styling applied, you may choose to use [`container`](#container) instead if the role is simply to group other
widgets.

**Configuration variables:**

- Style options from [Style properties](/components/lvgl#lvgl-styling).

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- obj:
    x: 10
    y: 10
    width: 220
    height: 300
    widgets:
      - ...
```

{{< anchor "lvgl-widget-qrcode" >}}

## `qrcode`

Use this widget to generate and display a QR-code containing a string at run time.

**Configuration variables:**

- **text** (*Optional*, [Text property](#text-property)): Text to be encoded in the QR.
- **size** (**Required**, int16): Set the desired size of the QR-code (in pixels). QR-codes with less data are smaller, but they scaled by an integer number to best fit to the given size.
- **light_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color for the light areas of the QR. Defaults to white.
- **dark_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color for the dark areas of the QR. Defaults to black.
- Style options from [Style properties](/components/lvgl#lvgl-styling).

**Actions:**

- `lvgl.qrcode.update` [action](/automations/actions#actions-action) updates the widget's `text` property to display a new QR-code.
  - **id** (**Required**): The ID of the qrcode widget to be updated.
  - **text** (*Optional*, [Text property](#text-property)): New text to be encoded in the QR.
  - Style options from [Style properties](/components/lvgl#lvgl-styling).

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- qrcode:
    id: lv_qr
    size: 100
    light_color: whitesmoke
    dark_color: steelblue
    text: esphome.io

# Example action:
on_...:
  then:
    - lvgl.qrcode.update:
        id: lv_qr
        text: home-assistant.io
```

{{< anchor "lvgl-widget-roller" >}}

## `roller`

Roller allows you to simply select one option from a list by scrolling.

{{< img src="lvgl_roller.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **anim_time** (*Optional*, [Time](/guides/configuration-types#time)): When the Roller is scrolled and doesn't stop exactly on an option it will scroll to the nearest valid option automatically in this amount of time.
- **mode** (*Optional*, enum): Option to make the roller circular. `NORMAL` or `INFINITE`, defaults to `NORMAL`.
- **options** (**Required**, list): The list of available options in the roller.
- **selected_index** (*Optional*, int8): The index of the item you wish to be selected.
- **selected_text** (*Optional*, string): The text of the item you wish to be selected.
- **selected** (*Optional*, list): Settings for the selected *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. The selected option in the middle. Besides the typical background properties it uses the [`label`](#lvgl-widget-label) text style properties to change the appearance of the text in the selected area.
- **visible_row_count** (*Optional*, int8): The number of visible rows.
- Style options from [Style properties](/components/lvgl#lvgl-styling). The background of the roller uses all the typical background properties and [`label`](#lvgl-widget-label) style properties. `text_line_space` adjusts the space between the options.

**Actions:**

- `lvgl.roller.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
- **id** (**Required**): The ID or a list of IDs of roller widgets to be updated.
- **animated** (*Optional*, boolean): Animate the indicator to the new selected value. Defaults to `true`.
- All the other roller options as listed above.

**Triggers:**

- `on_change` [trigger](/automations/actions#actions-trigger) is activated only when the user selects an item from the list. The new selected index is returned in the variable `x`. The [interaction](#lvgl-automation-triggers) LVGL event triggers also apply, and they also return the selected index in `x`.
- `on_value` [trigger](/automations/actions#actions-trigger) is activated the selection changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the selected index in `x`.

**Example:**

```yaml
# Example widget:
- roller:
    align: CENTER
    id: roller_id
    options:
      - Violin
      - Piano
      - Bassoon
      - Chello
      - Drums

# Example action:
on_...:
  then:
    - lvgl.roller.update:
        id: roller_id
        selected_index: 4

# Example trigger:
- roller:
    ...
    on_value:
      - logger.log:
          format: "Selected index is: %d, text is: %s"
          args: [x, text.c_str()]
```

The `roller` can be also integrated as {{< docref "/components/select/lvgl" "Select" >}} component.

{{< anchor "lvgl-widget-slider" >}}

## `slider`

The slider widget looks like a bar supplemented with a knob. The user can drag the knob to set a value. Just like bar, slider can be vertical or horizontal. The size of the indicator foreground and the knob position is set according to the current `value` of the slider.

{{< img src="lvgl_slider.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **anim_time** (*Optional*, [Time](/guides/configuration-types#time)): Sets the animation time if the value is set with `animated: true`.
- **animated** (*Optional*, boolean): Animate the indicator on boot to the starting value. Defaults to `true`.
- **indicator** (*Optional*, list): Settings for the indicator *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. The indicator shows the current state of the slider. Also uses all the typical background style properties.
- **knob** (*Optional*, list): Settings for the knob *part* to control the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. A rectangle (or circle) is drawn at the current value. Also uses all the typical background properties to describe the knob. By default, the knob is square (with an optional corner radius) with side length equal to the smaller side of the slider. The knob can be made larger with the padding values. Padding values can be asymmetric.
- **max_value** (*Optional*, int8): Maximum value of the indicator. Defaults to `100`.
- **min_value** (*Optional*, int8): Minimum value of the indicator. Defaults to `0`.
- **value** (*Optional*, int8): Actual value of the indicator at start, in `min_value`  -`max_value` range. Defaults to `0`.
- Any [Styling](/components/lvgl#lvgl-styling) and state-based option for the background of the slider. Uses all the typical background style properties. Padding makes the indicator smaller in the respective direction.

Normally, the slider can be adjusted either by dragging the knob, or by clicking on the slider bar. In the latter case the knob moves to the point clicked and slider value changes accordingly. In some cases it is desirable to set the slider to react on dragging the knob only. This feature is enabled by enabling the `adv_hittest` flag.

**Actions:**

- `lvgl.slider.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of slider widgets to be updated.
  - **animated** (*Optional*, boolean): Animate the indicator to the new value. Defaults to `true`.
  - **value** (*Optional*, int8): New value of the indicator.
  - Any [Styling](/components/lvgl#lvgl-styling) and state-based option for the background of the slider. Uses all the typical background style properties. Padding makes the indicator smaller in the respective direction.

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the slider value changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- `on_change` [trigger](/automations/actions#actions-trigger) is activated when the slider value is changed by user interaction. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the value in `x`.

**Example:**

```yaml
# Example widget:
- slider:
    x: 10
    y: 10
    width: 220
    id: slider_id
    value: 75
    min_value: 0
    max_value: 100

# Example action:
on_...:
  then:
    - lvgl.slider.update:
        id: slider_id
        knob:
          bg_color: 0x00FF00
        value: 55

# Example trigger:
- slider:
    ...
    on_value:
      - logger.log:
          format: "Slider value is: %.0f"
          args: [ 'x' ]
```

> [!NOTE]
> The `on_value` trigger is sent as the slider is dragged or changed with keys. The event is sent *continuously* while the slider is being dragged; this generally has a negative effect on performance. To mitigate this, consider using a [universal interaction trigger](#lvgl-automation-triggers) like `on_release`, to get the `x` variable once after the interaction has completed.

The `slider` can be also integrated as {{< docref "/components/number/lvgl" "Number" >}} or {{< docref "/components/sensor/lvgl" "Sensor" >}} component.

See [Light brightness slider](/cookbook/lvgl#lvgl-cookbook-bright) and [Media player volume slider](/cookbook/lvgl#lvgl-cookbook-volume) for examples which demonstrate how to use a slider to control entities in Home Assistant.

{{< anchor "lvgl-widget-spinbox" >}}

## `spinbox`

The spinbox contains a numeric value (as text) which can be increased or decreased through actions. You can, for example, use buttons labeled with plus and minus to call actions which increase or decrease the value as required.

{{< img src="lvgl_spinbox.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **anim_time** (*Optional*, [Time](/guides/configuration-types#time)): Sets the cursor's blink time.
- **decimal_places** (*Optional*, 0..6): The number of digits after the decimal point. If `0`, no decimal point is displayed. Defaults to `0`.
- **digits** (*Optional*, 1..10): The number of digits (excluding the decimal separator and the sign characters). Defaults to `4`.
- **range_from** (*Optional*, float): The minimum value allowed to set the spinbox to. Defaults to `0`.
- **range_to** (*Optional*, float): The maximum value allowed to set the spinbox to. Defaults to `100`.
- **rollover** (*Optional*, boolean): While increasing or decreasing the value, if either the minimum or maximum value is reached with this option enabled, the value will change to the other limit. If disabled, the value will remain at the minimum or maximum value. Defaults to `false`.
- **selected_digit** (*Optional*, int): The ordinal number of the digit to be initially focused. Defaults to `0` which
  represents the least significant digit. This digit will
  be incremented or decremented by one when `increment` or `decrement` actions are called.
- **value** (*Optional*, float): Actual value to be shown by the spinbox at start. Defaults to `0`.

> [!NOTE]
> The sign character will only be shown if the set range contains negatives.

**Actions:**

- `lvgl.spinbox.update` [action](/automations/actions#actions-action) updates the widget styles and properties from the specific options above, just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of spinbox widgets to be updated.
  - **value** (**Required**, float): New value of the spinbox.

- `lvgl.spinbox.increment` [action](/automations/actions#actions-action) increases the value by one `step` configured above.
  - **id** (**Required**): The ID of the spinbox widget which you want to increment.

- `lvgl.spinbox.decrement` [action](/automations/actions#actions-action) decreases the value by one `step` configured above.
  - **id** (**Required**): The ID of the spinbox widget which you want to decrement.

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the spinbox value changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the value in `x`.

**Example:**

```yaml
# Example widget:
- spinbox:
    id: spinbox_id
    text_align: center
    range_from: -10
    range_to: 40
    selected_digit: 2
    digits: 3
    decimal_places: 1

# Example actions:
on_...:
  then:
    - lvgl.spinbox.decrement: spinbox_id
on_...:
  then:
    - lvgl.spinbox.update:
        id: spinbox_id
        value: 25.5

# Example trigger:
- spinbox:
    ...
    on_value:
      then:
        - logger.log:
            format: "Spinbox value is %f"
            args: [ x ]
```

The `spinbox` can be also integrated as a {{< docref "/components/number/lvgl" "Number" >}} or {{< docref "/components/sensor/lvgl" "Sensor" >}} component.

See [Climate control](/cookbook/lvgl#lvgl-cookbook-climate) for an example which demonstrates how to implement a thermostat control using the spinbox.

{{< anchor "lvgl-widget-spinner" >}}

## `spinner`

The Spinner widget is a spinning arc over a ring.

{{< img src="lvgl_spinner.gif" alt="Image" class="align-center" >}}

**Configuration variables:**

- **arc_color** (*Optional*, [color](/components/lvgl#lvgl-color)): Color to draw the arcs.
- **arc_length** (**Required**, 0-360): Length of the spinning arc in degrees.
- **arc_opa** (*Optional*, [opacity](/components/lvgl#lvgl-opacity)): Opacity of the arc.
- **arc_rounded** (*Optional*, boolean): Make the end points of the arcs rounded. `true` rounded, `false` perpendicular line ending.
- **arc_width** (*Optional*, int16): Set the width of the arcs in pixels.
- **indicator** (*Optional*, list): Settings for the indicator *part* to show the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize. Draws *another arc using the arc style* properties. Its padding values are interpreted relative to the background arc.
- **spin_time** (**Required**, [Time](/guides/configuration-types#time)): Duration of one cycle of the spin.
- Style options from [Style properties](/components/lvgl#lvgl-styling).

**Actions:**

- `lvgl.spinner.update` [action](/automations/actions#actions-action), just like the [lvgl.widget.update](#lvgl-automation-actions) action is used for the common styles, states or flags.
  - **id** (**Required**): The ID or a list of IDs of spinner widgets to be updated.
  - Style options from [Style properties](/components/lvgl#lvgl-styling).

**Triggers:**

- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- spinner:
    align: center
    spin_time: 2s
    arc_length: 60deg
    id: spinner_id
    indicator:
      arc_color: 0xd4d4d4

# Example action:
on_...:
  then:
    - lvgl.spinner.update:
        id: spinner_id
        arc_color: 0x31de70
```

{{< anchor "lvgl-widget-switch" >}}

## `switch`

The switch looks like a little slider and can be used to turn something on and off.

{{< img src="lvgl_switch.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **indicator** (*Optional*, list): Settings for the indicator *part*, the foreground area underneath the knob shown when the switch is in `checked` state. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize.
- **knob** (*Optional*, list): Settings for the knob *part* to control the value. Supports a list of [styles](/components/lvgl#lvgl-styling) and state-based styles to customize.
- Style options from [Style properties](/components/lvgl#lvgl-styling).

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the switch value changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- `on_change` [trigger](/automations/actions#actions-trigger) is activated when the switch value is changed by user interaction. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers which also return the value in `x`.

**Example:**

```yaml
# Example widget:
- switch:
    x: 10
    y: 10
    id: switch_id

# Example trigger:
- switch:
    ...
    on_value:
      then:
        - logger.log:
            format: "Switch state: %d"
            args: [ x ]
```

The `switch` can be also integrated as a {{< docref "/components/switch/lvgl" "Switch" >}} component.

See [Local light switch](/cookbook/lvgl#lvgl-cookbook-relay) for an example which demonstrates how to use a switch to act on a local component.

{{< anchor "lvgl-widget-tabview" >}}

## `tabview`

The tab view object can be used to organize content in tabs. The tab buttons are internally generated with a [`buttonmatrix`](#lvgl-widget-buttonmatrix).

{{< img src="lvgl_tabview.png" alt="Image" class="align-center" >}}

The tabs are indexed (zero-based) in the order they appear in the configuration file. A new tab can be selected either by clicking on a tab button, by sliding horizontally on the content or via the `lvgl.tabview.select` [action](/automations/actions#actions-action), specifying the tab's index.

**Configuration variables:**

- **position** (*Optional*, enum): Position of the tab selector buttons. One of `TOP`, `BOTTOM`, `LEFT`, `RIGHT`. Defaults to `TOP`.
- **size** (*Optional*, percentage): The height (in case of `TOP`, `BOTTOM`  ) or width (in case of `LEFT`, `RIGHT`  ) tab buttons. Defaults to `10%`.
- **tabs** (**Required**, list): A list with (any number of) tabs to be added to tabview.
  - **name** (**Required**): The text to be shown on the button corresponding to the tab.
  - **id** (*Optional*): An ID for the tab itself.
  - **widgets** (**Required**, list): A list of {{< docref "/components/lvgl/widgets" >}} to be drawn on the tab, as children.
- **tab_style** (*Optional*): Style settings for the tabs.
  - **items** (*Optional*, dict): Settings for the items *part*, the buttons all use the text and typical background style properties except translations and transformations.
- **content_style** (*Optional*): Style settings for the content area.

**Actions:**

- `lvgl.tabview.select` [action](/automations/actions#actions-action) jumps the view to the desired tab:
  - **id** (**Required**): The ID of the tabview which receives this action.
  - **index** (**Required**): The (zero-based) index of the tab to which to jump.
  - **animated** (*Optional*, boolean): To animate the movement. Defaults to `false`.

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated when the tab value changes, either by user interaction or programmatically. The new value is returned in the variable `x`.
- `on_change` [trigger](/automations/actions#actions-trigger) is activated when the tab value is changed by user interaction. The new value is returned in the variable `x`.
- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- tabview:
    id: tabview_id
    position: top
    tab_style:
      border_color: 0x00FF00
      border_width: 6
      items:
        text_color: 0x0000FF
    tabs:
      - name: Dog
        id: tabview_tab_1
        widgets:
          - image:
              src: dog_img
          ...
      ...

# Example action:
on_...:
  then:
    - lvgl.tabview.select:
        id: tabview_id
        index: 1
        animated: true

# Example trigger:
- tabview:
    ...
    on_value:
      then:
        - if:
            condition:
              lambda: return tab == id(tabview_tab_1);
            then:
              - logger.log: "Dog tab is now showing"
```

{{< anchor "lvgl-widget-textarea" >}}

## `textarea`

The textarea is an extended label widget which displays a cursor and allows the user to input text. Long lines are wrapped and when the text becomes long enough the text area can be scrolled. It supports one line mode and password mode, where typed characters are replaced visually with bullets or asterisks.

{{< img src="lvgl_textarea.png" alt="Image" class="align-center" >}}

**Configuration variables:**

- **accepted_chars** (*Optional*, string): You can set a list of accepted characters, so other characters will be ignored.
- **max_length** (*Optional*, int): Limit the maximum number of characters to this value.
- **one_line** (*Optional*, boolean): The text area can be limited to only allow a single line of text. In this case the height will set automatically to fit only one line, line break characters will be ignored, and word wrap will be disabled.
- **password_mode** (*Optional*, boolean): The text area supports password mode. By default, if the `•` (bullet, `0x2022`  ) glyph exists in the font, the entered characters are converted to it after some time or when a new character is entered. If `•` is missing from the font, `*` (asterisk) will be used.
- **text** (*Optional*, [Text property](#text-property)): Initial contents of the textarea.
- **placeholder_text** (*Optional*, [Text property](#text-property)): A placeholder text can be specified, which is displayed when the Text area is empty.
- any [Styling](/components/lvgl#lvgl-styling) and state-based option for the background of the textarea. Uses all the typical background style properties and the text/label related style properties for the text.

**Actions:**

- `lvgl.textarea.update` [action](/automations/actions#actions-action) updates the widget's `text` property, to replace the entire text content.
  - **id** (**Required**): The ID or a list of IDs of textarea widgets to be updated.
  - **text** (*Optional*, [Text property](#text-property)): The text to replace the textarea content.
  - All other configuration variables from above are optional and have the same function as previously described.

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated on every keystroke.
- `on_ready` [trigger](/automations/actions#actions-trigger) is activated when `one_line` is configured as `true` and the newline character is received (Enter/Ready key on the keyboard).
- [interaction](#lvgl-automation-triggers) LVGL event triggers.

For both triggers above, when triggered, the variable `text` (`std::string` type) is available for use in lambdas within these triggers and it will contain the entire contents of the textarea.

**Example:**

```yaml
# Example widget:
- textarea:
    id: textarea_id
    one_line: true
    placeholder_text: "Enter text here"

# Example action:
on_...:
  then:
    - lvgl.textarea.update:
        id: textarea_id
        text: "Hello World!"

# Example trigger:
- textarea:
    ...
    on_value:
      then:
        - logger.log:
            format: "Textarea changed to: %s"
            args: [ text.c_str() ]
    on_ready:
      then:
        - logger.log:
            format: "Textarea ready: %s"
            args: [ text.c_str() ]
```

The `textarea` can be also integrated as {{< docref "/components/text/lvgl" "Text" >}} or {{< docref "/components/text_sensor/lvgl" "Text Sensor" >}} component.

{{< anchor "lvgl-widget-tileview" >}}

## `tileview`

The tileview is a container object whose elements, called tiles, can be arranged in grid form. A user can navigate between the tiles by dragging or swiping. Any direction can be disabled on the tiles individually to not allow moving from one tile to another.

If the tileview is screen sized, the user interface resembles what you may have seen on smartwatches. The tileview has parts `main` and `scrollbar`.

**Configuration variables:**

- **tiles** (**Required**, list): A list with (any number of) tiles to be added to tileview.
  - **column** (**Required**): Vertical position of the tile in the tileview grid.
  - **row** (**Required**): Horizontal position of the tile in the tileview grid.
  - **dir** (*Optional*): Enable moving to adjacent tiles in the given direction by swiping/dragging. One (or multiple as YAML list) of `LEFT`, `RIGHT`, `TOP`, `BOTTOM`, `HOR`, `VER`, `ALL`. Defaults to `ALL`.
  - **id** (*Optional*): A tile ID to be used with the `lvgl.tileview.select` action.
  - **widgets** (*Optional*, list): A list of {{< docref "/components/lvgl/widgets" >}} to be drawn on the tile, as children.

**Actions:**

- `lvgl.tileview.select` [action](/automations/actions#actions-action) jumps the `tileview` to the desired tile:
  - **animated** (*Optional*, boolean): To animate the movement. Defaults to `false`.
  - **column** (*Optional*): Vertical position of the tile to which to jump. Required if not specifying `tile_id`.
  - **row** (*Optional*): Horizontal position of the tile to which to jump. Required if not specifying `tile_id`.
  - **id** (**Required**): The ID of the tileview which receives this action.
  - **tile_id** (*Optional*): The ID of the tile (from within the tileview) to which to jump. Required if not specifying `row` and `column`.

**Triggers:**

- `on_value` [trigger](/automations/actions#actions-trigger) is activated when displayed tile changes. The new value is returned in the variable `tile` as the ID of the now-visible tile.
- [interaction](#lvgl-automation-triggers) LVGL event triggers.

**Example:**

```yaml
# Example widget:
- tileview:
    id: tiv_id
    tiles:
      - id: cat_tile
        row: 0
        column: 0
        dir: VER
        widgets:
          - image:
              src: cat_image
          - ...
      - ...

# Example action:
on_...:
  then:
    - lvgl.tileview.select:
        id: tiv_id
        tile_id: cat_tile
        animated: true

# Example trigger:
- tileview:
    ...
    on_value:
      - if:
          condition:
            lambda: return tile == id(cat_tile);
          then:
            - logger.log: "Cat tile is now showing"
```

{{< anchor "lvgl-automations" >}}

## Automations

{{< anchor "lvgl-automation-actions" >}}

### Actions

As outlined in the sections above, each widget type supports several of its own, unique actions.
Several universal actions are also available for all widgets, these are outlined below.

#### `lvgl.widget.update`

This powerful [action](/automations/actions#actions-action) allows changing/updating any widget's common [style property](/components/lvgl#lvgl-styling), state (templatable) or [flag](#lvgl-widget-flags) on the fly.

- **id** (**Required**): The ID or a list of IDs of widgets configured in LVGL to be updated.
- The widget's common [style property](/components/lvgl#lvgl-styling), state (templatable) or [flag](#lvgl-widget-flags).

```yaml
# Example for updating styles (in states):
on_...:
  then:
    - lvgl.widget.update:
        id: my_button_id
        bg_color: 0xFF0000
        state:
          disabled: true

# Example for updating flag:
on_...:
  then:
    - lvgl.widget.update:
        id: my_label_id
        hidden: true
```

Check out in the Cookbook [Remote light button](/cookbook/lvgl#lvgl-cookbook-binent) for an example which demonstrates how to use a template to update the state.

{{< anchor "lvgl-automation-shorthands" >}}

#### `lvgl.widget.hide`, `lvgl.widget.show`

These [actions](/automations/actions#actions-action) are shorthands for toggling the `hidden` [flag](#lvgl-widget-flags) of any widget.

- **id** (**Required**): The ID or a list of IDs of widgets configured in LVGL which you want to hide or show.

```yaml
on_...:
  then:
    - lvgl.widget.hide: my_label_id                # a single widget
    - lvgl.widget.show: [my_button_1, my_button_2] # a list of widgets
    - delay: 0.5s
    - lvgl.widget.show:
        -id: my_label_id
    - lvgl.widget.hide:
        - id: [my_button_1, my_button_2]
```

#### `lvgl.widget.disable`, `lvgl.widget.enable`

These [actions](/automations/actions#actions-action) are shorthands for toggling the `disabled` state of any widget (which controls the appearance of the corresponding *disabled* style set of the theme):

- **id** (**Required**): The ID or a list of IDs of widgets configured in LVGL which you want to disable or enable.

```yaml
- on_...:
    then:
      - lvgl.widget.disable:
          - my_button_1
          - my_button_2

- on_...:
    then:
      - lvgl.widget.enable:
          - id: my_button_1
          - id: my_button_2
```

{{< anchor "lvgl-automation-triggers" >}}

### Triggers

Specific triggers like `on_value` and `on_change`  ` are available for certain widgets; they are described above in their respective section.
Some universal triggers are also available for all of the widgets:

ESPHome implements as universal triggers the following interaction events generated by LVGL:

- `on_press`  : The widget has been pressed.
- `on_long_press`  : The widget has been pressed for at least the `long_press_time` specified in the input device configuration. Not called if scrolled.
- `on_long_press_repeat`  : Called after `long_press_time` in every `long_press_repeat_time` ms. Not called if scrolled.
- `on_short_click`  : The widget was pressed for a short period of time, then released. Not called if scrolled or long pressed.
- `on_click`  : Called on release if a widget did not scroll (regardless of long press).
- `on_release`  : Called in every case when a widget has been released.
- `on_scroll_begin`  : Scrolling of the widget begins.
- `on_scroll_end`  : Scrolling of the widget ends.
- `on_scroll`  : The widget was scrolled.
- `on_focus`  : The widget is focused.
- `on_defocus`  : The widget is unfocused.
- `on_gesture`  : A swipe gesture has been detected
- `on_swipe_left`  : A left swipe gesture has been detected
- `on_swipe_right`  : A right swipe gesture has been detected
- `on_swipe_up`  : An upwards swipe gesture has been detected
- `on_swipe_down`  : A downwards swipe gesture has been detected
- `on_swipe_top`  : Synonym for `on_swipe_up`
- `on_swipe_bottom`  : Synonym for `on_swipe_down`
- `on_all_events`  : Will be triggered on any event sent to the widget - this is useful for debugging.

These triggers can be applied directly to any widget in the LVGL configuration, *given that the widget itself supports generating such events*. For the widgets having a value, the triggers return the current value in variable `x`  ; this variable may be used in lambdas defined within those triggers.

Each trigger also deliver an `event` parameter, which is a pointer to the LVGL C type `lv_event_t`. This may be used in lambdas defined within those triggers. Refer to the [LVGL documentation](https://docs.lvgl.io/8.4/overview/event.html) for more information.

There are additional triggers for pages - each page may have an `on_load` and `on_unload` trigger. These will be called
when the page becomes active or inactive respectively.

```yaml
# Example triggers:
- button:
    ...
    on_short_click:
      then:
        lvgl.page.show: main_page
    on_long_press:
      then:
        light.toggle: display_backlight

- slider:
    ...
    on_release:
      then:
        - light.turn_on:
            id: display_backlight
            transition_length: 0ms
            brightness: !lambda return x / 100;
```

### `on_boot`

This [trigger](#lvgl-automation-triggers) is triggered after LVGL has been setup. It is available on the `lvgl` component and any widget and can be used to perform any LVGL related setup that is not possible with static configuration.
When used on a widget, it does not act specifically on that widget but can be used to keep actions related to that widget together with its configuration.

## See Also

- {{< docref "/components/lvgl/index" "LVGL Main component" >}}
- {{< docref "/components/binary_sensor/lvgl" >}}
- {{< docref "/components/sensor/lvgl" >}}
- {{< docref "/components/number/lvgl" >}}
- {{< docref "/components/switch/lvgl" >}}
- {{< docref "/components/select/lvgl" >}}
- {{< docref "/components/light/lvgl" >}}
- {{< docref "/components/text/lvgl" >}}
- {{< docref "/components/text_sensor/lvgl" >}}
- [LVGL docs](https://docs.lvgl.io/)
