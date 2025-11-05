---
description: ""
headless: true
---

Adds a value to each sensor value. The value may be a constant or a lambda returning a float.

```yaml
# Example configuration entry
filters:
  - offset: 2.0
  - offset: !lambda return id(some_sensor).state;
```
