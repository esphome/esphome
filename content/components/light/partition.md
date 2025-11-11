---
description: "Instructions for setting up light partitions."
title: "Light Partition"
params:
  seo:
    description: Instructions for setting up light partitions.
    image: color_lens.svg
---

The `partition` light platform allows you to combine multiple addressable light segments
(like {{< docref "fastled/" >}} or {{< docref "neopixelbus/" >}}) and/or individual lights (like {{< docref "rgb/" >}}) into a single addressable light.
This platform also allows splitting up an addressable light into multiple segments, so that
segments can be individually controlled.

## Splitting a single LED strip

If you want to split a strip, you may run into strange behavior like that the original light entity (e.g., `fastled_clockless`  )
may be conflicting with the partition. For better control over which segments of the strip will overlap each other,
mark the original `light` as `internal: true`.

```yaml
# Example configuration entry
light:
  - platform: partition
    name: "Partition Light 1"
    segments:
      # Use first 10 LEDs from the light with ID light1
      - id: light1
        from: 0
        to: 9

  - platform: partition
    name: "Partition Light 2"
    segments:
      # Use LEDs 11-20 from the light with ID light1
      - id: light1
        from: 10
        to: 19

  # Example for light segment source
  - platform: fastled_clockless
    id: light1
    # You may want (but don't need) this
    internal: true
    # Other settings
```

## Joining multiple LED lights into one

```yaml
# Example configuration entry
light:
  - platform: partition
    name: "Partition Light"
    segments:
      # Use first 10 LEDs from the light with ID light1
      - id: light1
        from: 0
        to: 9
      # Use first 10 LEDs from light with ID light2
      # they become LEDs 11-20 in the joined partition
      - id: light2
        from: 0
        to: 9
      # Use light3 as the 21st light in the partition
      - single_light_id: light3

  # Example for light segment source
  - platform: fastled_clockless
    id: light1
    # You may want (but don't need) this
    internal: true
    # Other settings

  # Example for light segment source
  - platform: fastled_clockless
    id: light2
    # You may want (but don't need) this
    internal: true
    # Other settings

  # Example for non-addressable light source
  - platform: rgb
    id: light3
    # You may want (but don't need) this
    internal: true
    # Other settings
```

## Configuration variables

- **segments** (**Required**, list): A list of segments included in this partition.

  *For addressable segments:*

  - **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the addressable light to be controlled by this segment.
  - **from** (**Required**, int): The index of the first LED to address in the segment. Counting starts with 0,
    so first LED is 0.

  - **to** (**Required**, int): The index of the last LED to address in this segment.
  - **reversed** (*Optional*, boolean): Whether to reverse the order of LEDs in this segment. Defaults to `false`.

  *For single light segments:*

  - **single_light_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of a single addressable or non-addressable light.
    If an addressable light is specified, it will be treated as a single light in the partition.

- All other options from [Light](/components/light#config-light).

> [!NOTE]
> Do *not* use this platform to control each LED on your addressable light - the light
> objects have a moderate overhead and if you try to create many lights you will run out
> of memory quickly.
>
> See [`light.addressable_set` Action](/components/light#light-addressable_set_action) for that.

## See Also

- {{< docref "/components/light" >}}
- {{< docref "/components/light/fastled" >}}
- {{< docref "/components/light/neopixelbus" >}}
- {{< apiref "partition/light_partition.h" "partition/light_partition.h" >}}
