---
description: "Instructions for setting up generic output buttons in ESPHome that control an output component."
title: "Generic Output Button"
params:
  seo:
    description: Instructions for setting up generic output buttons in ESPHome that control an output component.
    image: upload.svg
---

The `output` button platform allows you to use any output component as a button. This can for example be used to
momentarily set a GPIO pin using a button.

{{< img src="generic-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
output:
  - platform: gpio
    pin: GPIOXX
    id: output1

button:
  - platform: output
    name: "Generic Output"
    output: output1
    duration: 500ms
```

## Configuration variables

- **output** (**Required**, [ID](/guides/configuration-types#id)): The ID of the output component to use.
- **duration** (**Required**, [Time](/guides/configuration-types#time)): How long the output should be set when the button is pressed.
- All other options from [Button](/components/button#config-button).

> [!NOTE]
> When used with a {{< docref "/components/output/gpio" >}}, the pin will be low by default and pulled high when the button is
> pressed. To invert this behaviour and have the pin pulled low when the button is pressed, set the `inverted` option
> in the [Pin Schema](/guides/configuration-types#pin-schema).

## See Also

- {{< docref "/components/output" >}}
- {{< apiref "output/button/output_button.h" "output/button/output_button.h" >}}
