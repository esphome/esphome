---
description: "Instructions for setting up LibreTiny text sensors."
title: "LibreTiny Text Sensor"
params:
  seo:
    description: Instructions for setting up LibreTiny text sensors.
    image: libretiny.svg
---

The `libretiny` text sensor platform exposes various LibreTiny core
information via text sensors.

```yaml
# Example configuration entry
text_sensor:
  - platform: libretiny
    version:
      name: LibreTiny Version
```

## Configuration variables

- **version** (*Optional*): Expose the version of LibreTiny core as a text sensor. All options from
  [Text Sensor](/components/text_sensor#config-text_sensor).

## See Also

- {{< docref "/components/libretiny" >}}
- {{< apiref "libretiny/lt_component.h" "libretiny/lt_component.h" >}}
