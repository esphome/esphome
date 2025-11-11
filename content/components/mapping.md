---
description: "Mapping Component"
title: "Mapping Component"
---

The `mapping` component allows you to create a map or dictionary that allows a one-to-one translation from keys to
values. This enables e.g. mapping a string to a number or vice versa, or mapping a string such as a weather condition to an image.

```yaml
# Example configuration entry
mapping:
  - id: weather_icon
    from: string
    to: image
    entries:
      clear-night: clear_night_img
      cloudy: cloudy_img

 # Using the mapping in an automation
text_sensor:
 - id: forecast_text
   platform: homeassistant
   entity_id: weather.forecast_home
   on_value:
     lvgl.image.update:
       id: weather_image
       src: !lambda return id(weather_icon)[x];
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): Give the mapping an ID so that you can refer
  to it later in [lambdas](/automations/templates#config-lambda).

- **from** (**Required**, string): The type of the keys in the mapping. Can be one of `string` or `int`.
- **to** (**Required**, string): The type of values in the map. May be one of `string` or `int` or a class specifier as discussed below.
- **entries** (**Required**, dict): A list of key-value pairs that define the mapping. The keys must be of the type specified in the `from` field, and the values must be of the type specified in the `to` field.

## Mapping to a class

You can also map to a class. This is useful when you want to map to a more complex type, such as an image or a color. There are several types of class specifiers you can use:

- `image`  : Maps to an image as defined in the {{< docref "/components/image" >}} component. The values should each be an image ID.
- `color`  : Maps to a predefined [Color](/components/display#config-color). The values should each be a color ID.
- The name of a C++ class defined by ESPHome, e.g. `Component`. The values should each be a ID of that class.

## Using a mapping

A mapping defined in this component can be used in lambdas in other components. The mapping can be accessed using
the `id` function, and the value can be looked up using the `[]` operator as per the above example, or the `get` function.
A map may be updated at run time using a lambda call, e.g. `map.set("key", value)`.

Maps are stored in RAM, but will use PSRAM if available.

A more complex example follows:

```yaml
mapping:
  - id: color_map
    from: int
    to: color
    entries:
      0: red
      1: green
      2: blue
  - id: string_map
    from: int
    to: string
    entries:
      0: red
      1: green
      2: blue

color:
  - id: red
    hex: FF0000
  - id: green
    hex: 00FF00
  - id: blue
    hex: 0000FF

font:
  - file: gfonts://Roboto
    id: roboto20
    size: 20
    bpp: 4

display:
  - platform: ...
    # update the display drawing random text in random colors
    lambda: |-
      auto color = color_map.get(random_uint32() % 3); # Uses get() to index the color_map
      it.printf(100, 100, id(roboto20), color, id(string_map)[random_uint32() % 3].c_str(), Color(0));

    on_...:
      then:
        - lambda: |-
              id(color_map).set(2, Color::random_color());

```

## See Also

- {{< docref "index/" >}}
- {{< docref "/automations/actions" >}}
