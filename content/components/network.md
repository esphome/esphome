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
```

## Configuration variables

- **enable_ipv6** (*Optional*, boolean): Enables IPv6 support. Defaults to `false`.
- **min_ipv6_addr_count** (*Optional*, integer): ESPHome considers the network to be connected when it has one IPv4 address and this number of IPv6 addresses. Defaults to `0` so as to not hang on boot with networks where IPv6 is not enabled. `2` is typically a reasonable value for configurations requiring IPv6.

> [!NOTE]
> The [lwIP](https://savannah.nongnu.org/projects/lwip/) library used for the network component currently only implements IPv6 SLAAC according to [RFC4862](https://datatracker.ietf.org/doc/rfc4862/). The interface identifier (IID) is directly generated from the device MAC address.
> This has various security and privacy implications decribed in [RFC7721](https://datatracker.ietf.org/doc/rfc7721/), as this might leak outside of the smart home network and makes the device uniquely identifiable.
> Therefore, the address generation does not comply to [RFC7217](https://datatracker.ietf.org/doc/rfc7217/).

## See Also

- {{< docref "/components/wifi" >}}
- {{< docref "/components/ethernet" >}}
