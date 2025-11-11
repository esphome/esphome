---
description: "Instructions for setting up additional buttons for Haier climate devices."
title: "Haier Climate Buttons"
params:
  seo:
    description: Instructions for setting up additional buttons for Haier climate devices.
    image: haier.svg
---

Additional buttons for Haier AC cleaning. **These buttons are supported only by the hOn protocol**.

```yaml
# Example configuration entry
button:
  - platform: haier
    haier_id: haier_ac
    self_cleaning:
      name: Haier start self cleaning
    steri_cleaning:
      name: Haier start 56°C steri-cleaning
```

## Configuration variables

- **haier_id** (**Required**, [ID](/guides/configuration-types#id)): The id of Haier climate component
- **self_cleaning** (*Optional*): A button that starts Haier climate self cleaning.
  All options from [Button](/components/button#config-button).

- **steri_cleaning** (*Optional*): A button that starts Haier climate 56°C Steri-Clean.
  All options from [Button](/components/button#config-button).

## See Also

- {{< docref "/components/climate/haier" "Haier Climate" >}}
