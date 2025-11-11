---
description: "Instruction for using touchscreen components."
title: "Touchscreen Components"
params:
  seo:
    description: Instruction for using touchscreen components.
    image: folder-open.svg
---

The `touchscreen` component contains the base code for most touchscreen driver components
available in ESPHome and is responsible for passing the touch events to
`binary_sensors` with the `touchscreen` platform. It is also used by the LVGL component.

{{< anchor "config-touchscreen" >}}

## Base Touchscreen Configuration

```yaml
# Example touchscreen
touchscreen:
  - platform: ...
    display: display1
    transform:
      mirror_x: false
      mirror_y: false
      swap_xy: false

    on_touch:
      then:
        ...
    on_update:
      then:
        ...
    on_release:
      then:
        ...
```

### Configuration variables

- **display** (**Required**, [ID](/guides/configuration-types#id)): The display to use this touchscreen with.
- **transform** (*Optional*): Transform the touchscreen presentation using hardware. All defaults are `false`.

  - **swap_xy** (*Optional*, boolean): If true, exchange the x and y axes.
  - **mirror_x** (*Optional*, boolean): If true, mirror the x axis.
  - **mirror_y** (*Optional*, boolean): If true, mirror the y axis.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The touchscreen polling interval - used only if an interrupt pin is not in use. Defaults to `50ms`.
- **touch_timeout** (*Optional*, [Time](/guides/configuration-types#time)): A timeout for touchscreens that do not report the end of touch. The default varies depending on the touchscreen.
- **calibration** (*Optional*): Some touchscreens require calibration on a per-device basis.

  - **x_min** (**Required**, int): The raw value corresponding to the left

      (or top if `swap_xy` is specified) edge of the touchscreen. See [Calibration](#touchscreen-calibration)
      for the process to calibrate the touchscreen.

  - **x_max** (**Required**, int): The raw value corresponding to the right

      (or bottom if `swap_xy` is specified) edge of the touchscreen.

  - **y_min** (**Required**, int): The raw value corresponding to the top

      (or left if `swap_xy` is specified) edge of the touchscreen.

  - **y_max** (**Required**, int): The raw value corresponding to the bottom

      (or right if `swap_xy` is specified) edge of the touchscreen.

- **on_touch** (*Optional*, [Automation](/automations)): An automation to perform
  when the touchscreen is touched. See [`on_touch` Trigger](#touchscreen-on_touch).

- **on_update** (*Optional*, [Automation](/automations)): An automation to perform
  when the touchscreen is touched. See [`on_update` Trigger](#touchscreen-on_update).

- **on_release** (*Optional*, [Automation](/automations)): An automation to perform
  when the touchscreen is no longer touched. See [`on_release` Trigger](#touchscreen-on_release).

{{< anchor "touchscreen-touchpoint" >}}

## `TouchPoint` Argument Type

Both the [`on_touch` Trigger](#touchscreen-on_touch) and [`on_update` Trigger](#touchscreen-on_update) have an argument of the type {{< apistruct "touchscreen::TouchPoint" "touchscreen::TouchPoint" >}} in a
list (`on_update`  ) or as an `optional` (`on_touch`  ).

The integer members for the touch positions below are in relation to the display width and height:

- `id` is a number provided by the touchscreen to uniquely identify the touch on a multi-touch screen.
- `state` indicates the state of the touch. This can be **1**, indicating it is an initial touch, or **2** indicating the touch position has changed/moved.

- `x` and `y` are the current position.
- `x_prev` and `y_prev` are the previous position.
- `x_org` and `y_org` are the position of the touch when it was first detected.
- `x_raw` and `y_raw` are for calibrating the touchscreen in relation of the display. This replaces the properties with the same name in the touchscreen classes.

{{< anchor "touchscreen-calibration" >}}

## Calibration

For most touchscreen drivers the dimensions of the touchscreen are automatically set from the display driver to match the screen size.
In some cases such as the [XPT2046](/components/touchscreen/xpt2046#xpt2046-component) (a resistive touch screen) the reported values bear no relation to the actual screen size.
The `calibration` configuration can be used to manually calibrate the touchscreen.

To match the point of the touch to the display coordinates the touch screen has to be calibrated.
The touchscreen component returns raw values in the calibration range. Those raw values are available
as the `x_raw` and `y_raw` member variables. The goal of the calibration is to identify the raw values corresponding
to the edges of the screen.

The calibration assumes a display oriented in a way that you will be using it, i.e. your
[Display Rendering Engine](/components/display#display-engine) component has to have the [0,0] logical coordinate at the top left.

```yaml
# Touchscreen
touchscreen:
  platform: xpt2046
  id: my_touchscreen
  cs_pin: GPIOXX
  on_touch:
    - lambda: |-
          ESP_LOGI("cal", "x=%d, y=%d, x_raw=%d, y_raw=%0d",
              touch.x,
              touch.y,
              touch.x_raw,
              touch.y_raw
              );
```

Get a stylus or a similar object, run the project and touch the corners of the screen at
the edge pixels. Repeat several times and note minimum and maximum x and y raw values.

```none
... top left ...
[21:07:48][I][cal:071]: x=217, y=34, x_raw=3718, y_raw=445
[21:07:49][I][cal:071]: x=222, y=32, x_raw=3804, y_raw=419
... top right ...
[21:07:52][I][cal:071]: x=19, y=36, x_raw=334, y_raw=370
[21:07:52][I][cal:071]: x=22, y=35, x_raw=386, y_raw=347
... bottom left ...
[21:08:00][I][cal:071]: x=224, y=299, x_raw=3836, y_raw=3835
[21:08:00][I][cal:071]: x=225, y=303, x_raw=3848, y_raw=3878
[21:08:01][I][cal:071]: x=223, y=299, x_raw=3807, y_raw=3829
... bottom right ...
[21:08:11][I][cal:071]: x=16, y=299, x_raw=281, y_raw=3839
[21:08:12][I][cal:071]: x=19, y=302, x_raw=328, y_raw=3866
[21:08:13][I][cal:071]: x=20, y=296, x_raw=358, y_raw=3799
```

That means that the minimum raw x is 281, maximum 3848, minimum y 347 and maximum 3878.

Identify which raw value is the display's x direction and what the y one. In our case
moving right decreases the x raw value and going down increases the y one so the axes
match and we *don't* need to use `swap_xy`. If the raw x is the display's y,
use `swap_xy = true`.

If one of the coordinates goes in the "wrong" direction it needs to be inverted.
The inversion is performed by setting the `mirror_x` and/or `mirror_y` to true.

```yaml
touchscreen:
  platform: xpt2046
  calibration:
    x_min: 281
    x_max: 3848
    y_min: 347
    y_max: 3878
  transform:
    mirror_x: false
    mirror_y: false
    swap_xy: false
```

Compile, run and click on the edges again. The x and y should now match the coordinates
of the display.

```none
[21:32:34][I][cal:071]: x=7, y=6, x_raw=3755, y_raw=407
[21:32:37][I][cal:071]: x=237, y=4, x_raw=313, y_raw=385
[21:32:43][I][cal:071]: x=239, y=318, x_raw=284, y_raw=3845
[21:33:05][I][cal:071]: x=2, y=313, x_raw=3821, y_raw=3793
```

Note that the touch screen is not extremely precise and there might be nonlinearities
or similar errors so don't expect a pixel-perfect precision. You can verify the touchpoint
using a display lambda similar to the following.

```yaml
display:
  - platform: ili9341
    lambda: |-
      auto touch = id(my_touchscreen)->get_touch();
      if (touch) // or touch.has_value()
        it.filled_circle(touch.value().x, touch.value().y, 10, RED);
```

To be exact, the component does the following

- reads the raw x and y and normalizes it using (non-inverted) min and max values
- swaps x and y if needed
- inverts if needed
- scales to the display dimensions

{{< anchor "touchscreen-on_touch" >}}

## `on_touch` Trigger

This automation will be triggered when the touchscreen initially detects a touch on the touchscreen; it will not be fired again until
all touches (for multi-touch supported drivers) are released.

NOTE: This has changed (from ESPHome 2023.11.6.) To receive continuous updates from touch drags, use [`on_update` Trigger](#touchscreen-on_update).

This trigger provides two arguments named `touch` of type *touchpoint* and `touches` with a list of all touches.

{{< anchor "touchscreen-on_update" >}}

## `on_update` Trigger

This new automation will be triggered when the touchscreen detects an extra touch or that a touch has moved around on the screen.

This trigger provides one argument named `touches` of type {{< apiref "touchscreen::TouchPoints_t" "touchscreen::TouchPoints_t" >}} which has a list of
 [`TouchPoint` Argument Type](#touchscreen-touchpoint).

This trigger may be useful to detect gestures such as swiping across the display.

For example you could do:

```yaml
on_update:
    - lambda: |-
          for (auto touch: touches)  {
              if (touch.state <= 2) {
                 ESP_LOGI("Touch points:", "id=%d x=%d, y=%d", touch.id, touch.x, touch.y);
              }
          }
```

Be aware that you need to check the state flag every time to see if the touch is still valid.

- state value 0. means the touch is invalid as the touch is no longer detected.
- state value 1 means is being the first time detected.
- state value 2 means the touch is still being detected but is moved on the screen.
- state value 4 and higher means a touch release is detected.

{{< anchor "touchscreen-on_release" >}}

## `on_release` Trigger

This automation will be triggered when all touches are released from the touchscreen.

At this point of time it has no extra arguments.

{{< anchor "touchscreen_binary_sensor" >}}

## Binary Sensor

The `touchscreen` binary sensor allows you to setup areas on the touch screen as virtual
buttons.

```yaml
binary_sensor:
  - platform: touchscreen
    name: Top Left Touch Button
    x_min: 0
    x_max: 100
    y_min: 0
    y_max: 100
    page_id: home_page_id
    use_raw: true
```

### Configuration variables

- **touchscreen_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the touchscreen.
- **x_min** (**Required**, int): Left coordinate of the screen area to be detected as the virtual button.
- **x_max** (**Required**, int): Right coordinate of the screen area to be detected as the virtual button.
- **y_min** (**Required**, int): Top coordinate of the screen area to be detected as the virtual button.
- **y_max** (**Required**, int): Bottom coordinate of the screen area to be detected as the virtual button.
- **page_id** (*Optional*, [ID](/guides/configuration-types#id)): Only trigger this binary sensor if the display is showing this page.
  Cannot be used with `pages`.

- **pages** (*Optional*, list of [ID](/guides/configuration-types#id)): Only trigger this binary sensor if the display is showing one of these pages.
  Cannot be used with `page_id`.

- **use_raw** (*Optional*, boolean): Some touch screens are larger than the underlying screen, and use this extra
  area for touch buttons. To allow the sensor to register touches outside the display area set this to `true`.
  The calibration values as above should be set to the display bounds.

- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- [Binary Sensor Filters](/components/binary_sensor#binary_sensor-filters)
- {{< docref "/components/display/inkplate" "Inkplate 6 Plus" >}}
- {{< docref "/components/touchscreen/ektf2232" "EKTF2232" >}}
- {{< docref "/components/touchscreen/xpt2046" "XPT2046" >}}
- {{< docref "/components/touchscreen/tt21100" "TT21100" >}}
- {{< docref "/components/touchscreen/gt911" "GT911" >}}
- {{< apiref "touchscreen/touchscreen.h" "touchscreen/touchscreen.h" >}}
- {{< apiref "touchscreen/binary_sensor/touchscreen_binary_sensor.h" "touchscreen/binary_sensor/touchscreen_binary_sensor.h" >}}
