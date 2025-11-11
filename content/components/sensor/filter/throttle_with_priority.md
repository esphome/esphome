---
description: ""
headless: true
---

Throttle the incoming values unless they match a prioritized value. When this filter gets an incoming value, it first
checks if it matches one of the prioritized values. If so, the value is passed through immediately. Otherwise, it
checks if the last incoming value is at least `specified time period` old. If it is not older than the configured
value, the value is not passed forward.

```yaml
# Example configuration entry
filters:
  - throttle_with_priority:
      timeout: 1s
      value:
        - nan
        - 0
```
