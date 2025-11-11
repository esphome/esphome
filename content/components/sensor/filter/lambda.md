---
description: ""
headless: true
---

Perform a simple mathematical operation over the sensor values. The input value is `x` and
the result of the lambda is used as the output (use `return`  ).

```yaml
# Example configuration entry
filters:
  - lambda: return x * (9.0/5.0) + 32.0;
```

Make sure to add `.0` to all values in the lambda, otherwise divisions of integers will
result in integers (not floating point values).

To prevent values from being published, return `{}`  :

```yaml
# Example configuration entry
filters:
  - lambda: |-
      if (x < 10) return {};
      return x-10;
```
