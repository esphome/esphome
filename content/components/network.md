---
description: "Network component"
title: "Network component"
params:
  seo:
    description:
    image: network-wifi.svg
---

The network component is a global configuration for all types of
networks (WiFi, Ethernet).

```yaml
# Example configuration
network:
    enable_ipv6: true
    min_ipv6_addr_count: 2
    enable_high_performance: true
```

## Configuration variables

- **enable_ipv6** (*Optional*, boolean): Enables IPv6 support. Defaults to `false`.
- **min_ipv6_addr_count** (*Optional*, integer): ESPHome considers the network to be connected when it has one IPv4 address and this number of IPv6 addresses. Defaults to `0` so as to not hang on boot with networks where IPv6 is not enabled. `2` is typically a reasonable value for configurations requiring IPv6.
- **enable_high_performance** (*Optional*, boolean): Explicitly enables or disables high-performance networking optimizations. Only supported on ESP32 devices. When not specified, this is automatically enabled by components that benefit from optimized network settings. Set to `false` to disable these optimizations if they cause memory issues on your device. Defaults to component-driven behavior.

## High-performance networking

The network component can automatically apply optimized settings for components that require high throughput or low latency, such as media streaming. When enabled, this feature configures both the lwIP TCP/IP stack and WiFi driver with settings optimized for performance.

### PSRAM-Aware Optimizations

The optimization level depends on whether PSRAM is guaranteed to be available (configured via the {{< docref "psram" >}} component with `ignore_not_found: false`):

**With PSRAM guaranteed:**

- TCP send/receive buffers: 512KB windows with window scaling enabled
- WiFi RX buffers: 512 dynamic buffers
- WiFi TX buffers: 32 static buffers
- AMPDU aggregation: Optimized block acknowledgment windows

**Without PSRAM (or when not guaranteed):**

- TCP send/receive buffers: 65KB windows
- WiFi RX buffers: 64 dynamic buffers
- WiFi TX buffers: 64 dynamic buffers
- AMPDU aggregation: Standard block acknowledgment windows

> [!NOTE]
> The [lwIP](https://savannah.nongnu.org/projects/lwip/) library used for the network component currently only implements IPv6 SLAAC according to [RFC4862](https://datatracker.ietf.org/doc/rfc4862/). The interface identifier (IID) is directly generated from the device MAC address.
> This has various security and privacy implications decribed in [RFC7721](https://datatracker.ietf.org/doc/rfc7721/), as this might leak outside of the smart home network and makes the device uniquely identifiable.
> Therefore, the address generation does not comply to [RFC7217](https://datatracker.ietf.org/doc/rfc7217/).

## See Also

- {{< docref "wifi/" >}}
- {{< docref "ethernet/" >}}
