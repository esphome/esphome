---
description: ""
headless: true
---

This filter stores the last value passed through this filter and only passes incoming values through
if incoming value is sufficiently different from the previously passed one.
This difference can be calculated in two ways an absolute difference or a percentage difference.

If a number is specified, it will be used as the absolute difference required.
For example if the filter were configured with a value of 2 and the last value passed through was 10,
only values greater than or equal to 12 or less than or equal to 8 would be passed through.

```yaml
# Example configuration entry
filters:
  - delta: 2.0
```

If a percentage is specified a percentage of the last value will be used as the required difference.
For example if the filter were configured with a value of 20% and the last value passed through was 10,
only values greater than or equal to 12 or less than or equal to 8 would be passed through.
However, if the last value passed through was 100 only values greater than or equal to 120 or less than or
equal to 80 would be passed through.

```yaml
# Example configuration entry
filters:
  - delta: 20%
```
