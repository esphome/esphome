---
description: "Instructions for setting up the Z-Wave Proxy in ESPHome."
title: "Z-Wave Proxy"
params:
  seo:
    description: Instructions for setting up the Z-Wave Proxy in ESPHome.
    image: z-wave.svg
---

The `zwave_proxy` component allows proxying of Z-Wave data frames between a
[Z-Wave Modem SoC](https://www.silabs.com/wireless/z-wave/800-series-modem-soc) and
[Z-Wave JS](https://zwave-js.github.io/zwave-js/) via ESPHome's {{< docref "/components/api" >}} over
the {{< docref "/components/wifi" >}} or {{< docref "/components/ethernet" >}}. This allows for more flexibility when
placing your Z-Wave hardware within your home.

As the Z-Wave modem SoC communicates via a serial connection, you need to have a [UART](/components/uart) defined in your ESPHome
device's configuration.

In addition, the `zwave_proxy` expects to proxy messages via ESPHome's {{< docref "/components/api" >}}; this is also
required in your configuration.

> [!NOTE]
> Number of connections
>
> While ESPHome supports multiple API connections/clients, only a single client may subscribe to and receive proxied
> Z-Wave data frames at any given time.

## Configuration

```yaml
# Example configuration entry
zwave_proxy:
```

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID for the `zwave_proxy`.

## Maximizing performance

Low latency is key to achieving an optimal experience with Z-Wave (or any) wireless devices.

It's important to understand that using the `zwave_proxy` *will* increase latency between your Z-Wave devices and
Z-Wave JS -- this is simply the consequence of passing messages from one medium to another.

Under near-ideal conditions:

- With a direct serial connection to the Z-Wave modem SoC, latency may be as low as approximately 20 milliseconds (ms).
- When introducing the Z-Wave proxy component, it's still possible to achieve low latency -- we've seen as low as 35
  ms! However, in practice, it's more realistic to expect 50-60 ms, assuming decent RF conditions.

In general, any duration less than 100 ms is quite acceptable in terms of latency; this value is generally a good
target to keep in mind.

## Maximizing reliability

In addition to low latency, reliability is also critical to an optimal experience.

**A wired Ethernet connection will provide the best reliability for the Z-Wave proxy.** Ethernet offers:

- More stable and predictable latency
- No connection drops or lag due to RF interference
- Immunity to environmental conditions

### Wi-Fi considerations

Wi-Fi is impacted by:

- Interference from other RF devices
- Environmental conditions, such as:
  - People/pets moving around a room
  - Building construction materials
  - Air density, quality, and even humidity

These factors can periodically cause momentary signal instability. While Wi-Fi devices typically recover automatically,
this comes at the expense of temporarily degraded performance (latency) and, in more extreme cases, brief loss of
connectivity.

If you choose to use Wi-Fi to bridge your Z-Wave modem to Z-Wave JS:

- Confirm that there is a strong, stable Wi-Fi signal available in the location you'll place your Z-Wave proxy.
- Do not attempt to place your Z-Wave proxy at or near the edge of the coverage area your Wi-Fi router/access point
  provides. If in doubt, move it closer to your Wi-Fi router/access point.
- If you find that your Z-Wave devices are not operating reliably, you might try:
  - moving your Z-Wave proxy closer to your Wi-Fi router/access point.
  - changing the Wi-Fi channel your Wi-Fi router/access point is using.
  - getting a better Wi-Fi router/access point. In particular, many ISP-provided Wi-Fi routers are designed to be
    cost-effective and are not optimized for performance.

## See Also

- {{< docref "/components/api" >}}
- {{< docref "/components/ethernet" >}}
- {{< docref "/components/wifi" >}}
- {{< apiref "zwave_proxy/zwave_proxy.h" "zwave_proxy/zwave_proxy.h" >}}
