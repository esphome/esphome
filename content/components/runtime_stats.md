---
description: "Instructions for setting up the runtime statistics component to analyze component performance in ESPHome."
title: "Runtime Statistics"
params:
  seo:
    description: Instructions for setting up the runtime statistics component to analyze component performance in ESPHome.
    image: chart-line.svg
---

The `runtime_stats` component allows you to collect and analyze runtime performance statistics for all components in your ESPHome device. This is a powerful debugging and optimization tool that helps identify components that may be blocking the event loop or consuming excessive processing time.

> [!WARNING]
> This component is intended for **debugging and troubleshooting**. While it can be temporarily enabled in production to diagnose issues, it should not be left enabled long-term because:
>
> - The statistics collection adds overhead to every component execution
> - It increases memory usage to store statistics
> - The periodic logging can clutter your logs
>
> Enable it when needed to find problems, then disable it once your investigation is complete.

```yaml
# Example configuration entry
runtime_stats:
  log_interval: 60s
```

## Configuration variables

- **log_interval** (*Optional*, [Time](/guides/configuration-types#time)): How often to log the statistics. Defaults to `60s`.

  - Minimum value is `1s`
  - Setting this too low will increase log spam

## Understanding the Output

> [!NOTE]
> Runtime statistics use `millis()` for time measurement, which provides millisecond resolution. This means:
>
> - Components that execute in less than 1ms will show as 0ms
> - Very fast operations cannot be accurately measured
> - The statistics are best suited for finding components that take multiple milliseconds

The component logs two types of statistics:

**Period Statistics**
  Shows statistics for the most recent logging interval. Useful for identifying transient performance issues.

**Total Statistics**
  Shows cumulative statistics since boot. Useful for understanding overall system behavior.

For each component, the following metrics are reported:

- **count**: Number of times the component executed
- **avg**: Average execution time in milliseconds
- **max**: Maximum execution time observed
- **total**: Total cumulative execution time

Components are sorted by total execution time (descending) to highlight the most impactful components first.

## Example Output

```text
[09:55:52][I][runtime_stats:042]: Component Runtime Statistics
[09:55:52][I][runtime_stats:043]: Period stats (last 60000ms):
[09:55:52][I][runtime_stats:066]:   wifi: count=60, avg=0.50ms, max=5ms, total=30ms
[09:55:52][I][runtime_stats:066]:   api: count=120, avg=0.01ms, max=1ms, total=1ms
[09:55:52][I][runtime_stats:066]:   sensor: count=600, avg=0.00ms, max=1ms, total=2ms
[09:55:52][I][runtime_stats:070]: Total stats (since boot):
[09:55:52][I][runtime_stats:084]:   wifi: count=600, avg=0.45ms, max=5ms, total=270ms
[09:55:52][I][runtime_stats:084]:   api: count=1200, avg=0.01ms, max=1ms, total=12ms
[09:55:52][I][runtime_stats:084]:   sensor: count=6000, avg=0.00ms, max=1ms, total=20ms
```

## Use Cases

**Identifying Blocking Components**
  Look for components with high `max` times. These may be blocking the event loop and causing issues with other components.

**Optimization Targets**
  Components with high `total` times are good candidates for optimization, especially if they execute frequently.

**Performance Regression Testing**
  Compare statistics before and after changes to ensure performance hasn't degraded.

**Troubleshooting Timing Issues**
  If components are missing deadlines or behaving erratically, runtime statistics can help identify the cause.

## Tips for Effective Use

1. **Start with default interval**: The 60-second default provides a good balance between detail and log volume.

1. **Focus on outliers**: Components with significantly higher execution times than others are usually the best optimization targets.

1. **Consider execution frequency**: A component that takes 1ms but runs 1000 times per minute has more impact than one that takes 10ms but runs once per minute.

1. **Watch for patterns**: Execution times that increase over time may indicate memory leaks or resource exhaustion.

1. **Disable when done**: Always remove or comment out the runtime_stats component when you're finished debugging.

## See Also

- {{< docref "debug/" >}}
- {{< docref "logger/" >}}
- [Automation](/automations)
- {{< apiref "runtime_stats/runtime_stats.h" "runtime_stats/runtime_stats.h" >}}
