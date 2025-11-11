---
description: ""
headless: true
---

Pass forward a value with the first child filter that returns. Below example
will only pass forward values that are *either* at least 1s old or are if the absolute
difference is at least 5.0.

```yaml
# Example configuration entry
filters:
  - or:
    - throttle: 1s
    - delta: 5.0
```
