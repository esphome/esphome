---
description: "Set up guide for configuring IR and RF devices in ESPHome."
title: "Setting up RMT Devices"
params:
  seo:
    description: Set up guide for configuring IR and RF devices in ESPHome.
    image: remote.svg
---

## Setting up IR Devices

{{< anchor "remote-setting-up-infrared" >}}

In this guide an infrared device will be set up with ESPHome. First, the remote code
will be captured with an IR receiver module (like [this one](https://www.sparkfun.com/products/10266)).
We will use ESPHome's dumping ability to output the decoded remote code directly.

Then we will set up a new remote transmitter with an infrared LED (like
[this one](https://learn.sparkfun.com/tutorials/ir-communication/all)) to transmit the
code when a button is pressed.

First, connect the infrared receiver module to a pin on your board and set up a
remote_receiver instance:

```yaml
remote_receiver:
  pin: GPIOXX
  dump: all
```

Compile and upload the code. While viewing the log output from the ESP,
press a button on an infrared remote you want to capture (one at a time).

You should see log output like below:

```text
# If the codec is known:
[D][remote.panasonic] Received Panasonic: address=0x4004 command=0x8140DFA2

# Or raw output if it's not known yet
# The values may fluctuate a bit, but as long as they're similar it's ok
[D][remote.raw] Received Raw: 4088, -1542, 1019, -510, 513, -1019, 510, -509, 511, -510, 1020,
[D][remote.raw]   -1020, 1022, -1019, 510, -509, 511, -510, 511, -509, 511, -510,
[D][remote.raw]   1020, -1019, 510, -511, 1020, -510, 512, -508, 510, -1020, 1022
```

If the codec is already implemented in ESPHome, you will see the decoded value directly -
otherwise you will see the raw data dump (which you can use just as well). You have
just successfully captured your first infrared code.

Now let's use this information to emulate a button press from the ESP. First, wire up the
IR diode to a new pin on the ESP and configure a global `remote_transmitter` instance:

```yaml
remote_transmitter:
  pin: GPIOXX
  # Infrared remotes use a 50% carrier signal
  carrier_duty_percent: 50%
```

This will allow us to send any data we want via the IR LED. To replicate the codes we decoded
earlier, create a new template button that sends the infrared code when triggered:

```yaml
button:
  - platform: template
    name: Panasonic Power Button
    on_press:
      - remote_transmitter.transmit_panasonic:
          address: 0x4004
          command: 0x8140DFA2

# Or for raw code
button:
  - platform: template
    name: Raw Code Power Button
    on_press:
      - remote_transmitter.transmit_raw:
          carrier_frequency: 38kHz
          code: [4088, -1542, 1019, -510, 513, -1019, 510, -509, 511, -510, 1020,
                 -1020, 1022, -1019, 510, -509, 511, -510, 511, -509, 511, -510,
                 1020, -1019, 510, -511, 1020, -510, 512, -508, 510, -1020, 1022]
```

Recompile again, when you power up the device the next time you will see a new button
in the frontend. Click on it and you should see the remote signal being transmitted. Done!

{{< anchor "remote-setting-up-rf" >}}

## Setting up RF Devices

The `remote_transmitter` and `remote_receiver` components can also be used to send
and receive 433MHz Radio Frequency (RF) signals. This guide will discuss setting up a 433MHz
receiver to capture a device's remote codes. After that we will set up a 433MHz transmitter
to replicate the remote code with the press of a button in the frontend.

First, connect the RF module to a pin on the ESP and set up a remote_receiver instance:

```yaml
remote_receiver:
  pin: GPIOXX
  dump: all
  # Settings to optimize recognition of RF devices
  tolerance: 50%
  filter: 250us
  idle: 4ms
  buffer_size: 2kb # only for ESP8266
```

Compile and upload the code. While viewing the log output from the ESP,
press a button on an RF remote you want to capture (one at a time).

You should see log output like below:

```text
# If the codec is known:
[D][remote.rc_switch] Received RCSwitch: protocol=2 data='100010000000000010111110'

# Or raw output if it's not known yet
# The values may fluctuate a bit, but as long as they're similar it's ok
[D][remote.raw] Received Raw: 4088, -1542, 1019, -510, 513, -1019, 510, -509, 511, -510, 1020,
[D][remote.raw]   -1020, 1022, -1019, 510, -509, 511, -510, 511, -509, 511, -510,
[D][remote.raw]   1020, -1019, 510, -511, 1020, -510, 512, -508, 510, -1020, 1022
```

> [!NOTE]
> If the log output is flooded with "Received Raw" messages, you can also disable raw
> remote code reporting and rely on rc_switch to decode the values.
>
> ```yaml
> remote_receiver:
>   pin: GPIOXX
>   dump:
>     - rc_switch
>   ...
> ```

If the codec is already implemented in ESPHome, you will see the decoded value directly -
otherwise you will see the raw data dump (which you can use just as well). You have
just successfully captured your first RF code.

Now let's use this information to emulate a button press from the ESP. First, wire up the
RF transmitter to a new pin on the ESP and configure a global `remote_transmitter` instance:

```yaml
remote_transmitter:
  pin: GPIOXX
  # RF uses a 100% carrier signal
  carrier_duty_percent: 100%
```

This will allow us to send any data we want via the RF transmitter. To replicate the codes we decoded
earlier, create a new template button that sends the RF code when triggered:

```yaml
button:
  - platform: template
    name: RF Power Button
    optimistic: true
    on_press:
      - remote_transmitter.transmit_rc_switch_raw:
          code: '100010000000000010111110'
          protocol: 2
          repeat:
            times: 10
            wait_time: 0s

# Or for raw code
button:
  - platform: template
    name: Raw Code Power Button
    on_press:
      - remote_transmitter.transmit_raw:
          code: [4088, -1542, 1019, -510, 513, -1019, 510, -509, 511, -510, 1020,
                 -1020, 1022, -1019, 510, -509, 511, -510, 511, -509, 511, -510,
                 1020, -1019, 510, -511, 1020, -510, 512, -508, 510, -1020, 1022]
```

Recompile again, when you power up the device the next time you will see a new button
in the frontend. Click on it and you should see the remote signal being transmitted. Done!

> [!NOTE]
> Some devices require that the transmitted code be repeated for the signal to be picked up
> as valid. Also the interval between repetitions can be important. Check that the pace of
> repetition logs are consistent between the remote controller and the transmitter node.
> You can adjust the `repeat:` settings accordingly.

## See Also

- {{< docref "/components/remote_receiver" >}}
- {{< docref "/components/remote_transmitter" >}}
