---
description: ""
headless: true
---

(**Required**, number): Filter out specific values to be displayed, e.g., filtering out the value `85.0`

```yaml
# Example configuration entry
filters:
  - filter_out: 85.0
```

A list of values may be supplied, and values are templatable:

```yaml
# Example configuration entry
filters:
  - filter_out:
      - 85.0
      - !lambda return id(some_sensor).state;
```
