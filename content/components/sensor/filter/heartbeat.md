---
description: ""
headless: true
---

Send the sensor value periodically at the specified time interval. If the sensor value changes
during this interval, the timer will not reset — the last known value of the sensor will still
be sent when the interval elapses.

For example, a value of `10s` will cause the filter to output the last known value every 10 seconds,
regardless of how often the input value changes.

When using `optimistic` mode, the filter will still repeat the last known value at the configured interval,
but in addition, every new incoming value is published immediately as it arrives. This ensures that the
sensor output updates instantly on change, while maintaining a steady periodic "heartbeat" of the last value
between updates. This mode is useful for sensors where immediate responsiveness is desired.

Configuration variables:

- **period** (Required, time): The interval at which the last known value is republished.
- **optimistic** (*Optional*, boolean): When enabled, every new incoming value is published immediately as it
  arrives, regardless of the configured time interval.

```yaml
# Example filters
filters:
  - heartbeat: 5s
  - heartbeat:
      period: 5s
      optimistic: true
```
