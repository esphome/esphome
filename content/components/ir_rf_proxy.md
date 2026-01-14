---
description: "Instructions for setting up the IR/RF Proxy in ESPHome."
title: "IR/RF Proxy"
params:
  seo:
    description: Instructions for setting up the IR/RF Proxy in ESPHome.
    image: infrared.svg
---

> [!IMPORTANT]
> This component is EXPERIMENTAL. The API may change at any time
> without following the normal breaking changes policy. Use at your own risk.
> Once the API is considered stable, this warning will be removed.

ESPHome's IR/RF proxy component works with Home Assistant to expand its remote control capabilities. This component
provides a unified API-accessible interface for transmitting and receiving infrared and RF signals, acting as a bridge
between Home Assistant (or other API clients) and ESPHome's existing [remote_receiver](/components/remote_receiver) and
[remote_transmitter](/components/remote_transmitter) components. Note that at least one of these components is required
in your device's configuration if you wish to use the IR/RF proxy component.

The IR/RF proxy enables runtime signal transmission without recompiling and/or reinstalling (flashing) firmware,
making it ideal for learning and replaying IR/RF commands, creating universal remote controls, and integrating with
Home Assistant's remote control features.

```yaml
# Example configuration entry
infrared:
  # IR transmitter instance
  - platform: ir_rf_proxy
    name: IR Proxy Transmitter
    id: ir_proxy_tx
    remote_transmitter_id: ir_tx
  # IR receiver instance
  - platform: ir_rf_proxy
    name: IR Proxy Receiver
    id: ir_proxy_rx
    remote_receiver_id: ir_rx
```

## Configuration variables

- **remote_transmitter_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the
  [remote_transmitter](/components/remote_transmitter) component to use for sending signals. Exactly one of
  `remote_transmitter_id` or `remote_receiver_id` must be specified.
- **remote_receiver_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the
  [remote_receiver](/components/remote_receiver) component to use for receiving signals. Exactly one of
  `remote_transmitter_id` or `remote_receiver_id` must be specified.
- **frequency** (*Optional*, frequency): The operating frequency (for example, `433 MHz`, `315 MHz`, `900 MHz`). Set to
  a non-zero value for RF hardware. Defaults to `0` (infrared). This value is used to distinguish between IR and RF
  hardware types and is passed to Home Assistant, allowing it to identify integrations this IR/RF proxy instance can
  potentially support.

  All other configuration variables from [infrared](/components/infrared).

> [!NOTE]
> When configuring a transmitter for infrared (`frequency: 0` or not set), ensure the linked `remote_transmitter` has
> `carrier_duty_percent` set to an appropriate value, typically 30-50%. This is required for proper infrared
> transmission. If `carrier_duty_percent` is set to 0% or 100% and `frequency` is not set or is set to zero, a
> validation error will occur.

## How It Works

The IR/RF proxy component creates API-accessible entities that can be controlled from Home Assistant or other API
clients. Each instance can be configured as either a transmitter or receiver by specifying the appropriate hardware
component ID.

### Transmitting Signals

The IR/RF proxy transmits signals using raw timings, which provides full remote control over the exact waveform sent
to the hardware. When configured with a `remote_transmitter_id`, the component can transmit IR/RF signals by accepting
an [array of microsecond timing values](/components/remote_transmitter#remote_transmittertransmit_raw-action)
representing alternating mark (LED on) and space (LED off) periods.

**Transmission parameters:**

- **Raw timings array**: Alternating mark/space durations in microseconds (just as the
  [remote_transmitter.transmit_raw](/components/remote_transmitter#remote_transmittertransmit_raw-action) action uses)
- **Carrier frequency**: Optional carrier frequency in Hz (0 = no carrier modulation, typical for RF)
- **Repeat count**: Number of times to transmit the signal (defaults to 1)

The raw timings format allows for maximum flexibility and can support more or less any protocol, whether implemented in
ESPHome or not. Home Assistant integrations can encode protocol-specific commands into raw timings and transmit them
through the IR/RF proxy component.

### Receiving Signals

When configured with a `remote_receiver_id`, the IR/RF proxy captures raw timings and sends them to API clients for
decoding or analysis. This enables:

- Learning IR/RF commands from existing remotes
- Analyzing unknown protocols
- Creating universal remote controls

Reception is non-blocking, allowing other listeners to also process signals simultaneously.

## Hardware Support

The IR/RF proxy can work with both infrared and RF hardware:

- **Infrared**: Do not set `frequency` (or set `frequency: 0`, which is the default) and use standard IR LEDs/receivers
- **RF**: Set `frequency` to match your RF hardware (for example, `315 MHz` for 315 MHz or `433 MHz` for 433.92 MHz)

You can create separate instances for different purposes:

- Transmit-only (specify only `remote_transmitter_id`)
- Receive-only (specify only `remote_receiver_id`)
- Multiple instances of any of the above for different hardware or frequencies. For example, a single ESPHome device
  could have multiple transmitter LEDs, each in a different room/cabinet.

## See Also

- [Remote Transmitter](/components/remote_transmitter)
- [Remote Receiver](/components/remote_receiver)
- {{< apiref "ir_rf_proxy/ir_rf_proxy.h" "ir_rf_proxy/ir_rf_proxy.h" >}}
