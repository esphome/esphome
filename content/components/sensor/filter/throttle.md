---
description: ""
headless: true
---

Throttle the incoming values. When this filter gets an incoming value,
it checks if the last incoming value is at least `specified time period`   old.
If it is not older than the configured value, the value is not passed forward.

```yaml
# Example configuration entry
filters:
  - throttle: 1s
```
