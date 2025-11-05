---
description: ""
headless: true
---

Limits the value to the range between `min_value` and `max_value`. By default, sensor values outside these bounds will be set to `min_value` or `max_value`, respectively. If `ignore_out_of_range` is true, then sensor values outside those bounds will be ignored. If `min_value` is not set, there is no lower bound; if `max_value` is not set there is no upper bound.

Configuration variables:

- **min_value** (*Optional*, float): The lower bound of the range.
- **max_value** (*Optional*, float): The upper bound of the range.
- **ignore_out_of_range** (*Optional*, bool): If true, ignores all sensor values out of the range. Defaults to `false`.

```yaml
# Example configuration entry
filters:
  - clamp:
      min_value: 10
      max_value: 75
      ignore_out_of_range: true
```
