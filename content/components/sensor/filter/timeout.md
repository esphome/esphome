---
description: ""
headless: true
---

After the first value has been sent, if no subsequent value is published within the specified `timeout` period, send
a templatable value which defaults to `NaN`. The value may also be set to `last`, which will result in the last
value received by the filter being sent again.

This filter particularly is useful when:

- data is derived from some communication channel (a serial port, for example) which can potentially be interrupted.
- placed ahead of a throttle filter to ensure that the last value published will pass through the throttle.

```yaml
# Example configuration entry
filters:
  - timeout: 10s  # sent value will be NaN
  - timeout:
      timeout: 10s
      value: !lambda return 0;
  - timeout:
      timeout: 10s
      value: last  # sent value will be the last value received by the filter
```
