---
description: "Frequently asked questions in ESPHome."
title: "Frequently Asked Questions"
params:
  seo:
    description: Frequently asked questions in ESPHome.
    image: question_answer.svg
---

## Which ESP should I use for my project?

We're asked this *all the time.* As with all things engineering, "it depends". Based on the current state of hardware
support within ESPHome, here's what we suggest:

### Recommended

- **ESP32**

  - Best supported/most mature
  - Includes a great set of built-in hardware peripherals, so it's very capable and very flexible.

- **ESP32-S3**

  - An update to the original ESP32 with a slightly modified set of hardware peripherals.
  - Has a built-in USB peripheral/interface (as opposed to relying on an external USB-to-serial chip)
  - Has instruction set extensions which make it a better fit for applications which require some form of machine
    learning ({{< docref "/components/micro_wake_word" >}}, for example).

- **ESP32-C3**

  - Generally intended ([per Espressif](https://www.espressif.com/en/news/ESP32_C3)) to replace the well-known ESP8266.
  - Use if:

    - You're worried that the ESP32(-Sx) is "too powerful".
    - You need a lower-power device than the ESP32(-Sx) family offers.

### Not Recommended

- **ESP8266**

  - It's over ten years old and is *quite lacking* in terms of built-in hardware peripherals.
  - Use an ESP32-C3 when you're thinking you need to use an ESP8266 because the ESP32(-Sx) is "too powerful" or
    "overkill".

  - Does not meet the requirements of {{< docref "/guides/made_for_esphome" >}}.
  - The original NodeMCU, D1-Mini and ESP-01 are examples of boards which utilize an ESP8266; note that there are
    (pin-compatible) versions of these boards available which instead utilize a more modern ESP32 or variant.

### Additional Considerations

- These recommendations are primarily for people who are starting from scratch and/or are new to ESPHome.
- A lot of people already have a drawer full of ESP8266 boards -- we're not trying to stop you from using them! That
  said, *don't buy any more of them* and *consider our recommendations above* as you buy new devices. 😉

- *"...But the [ESP8266 board] is cheaper!!!"*...well, you get what you pay for. Compared with the ESP8266, all ESP32s
  and variants have:

  - a better, more complete set of hardware peripherals, keeping the processor core(s) free to maximize performance.
  - more GPIO pins.
  - roughly 5x the amount of RAM.

    - Some components require more RAM than is available on the ESP8266 -- (large) displays and
      {{< docref "/components/sensor/bme68x_bsec2" "some sensors" >}} are known to regularly provoke issues/crashes
      on ESP8266s.

    - Workarounds are often available, but it's not reasonable to assume that a given workaround will work forever,
      especially if you *want* to update your devices regularly but depend on
      {{< docref "/components/sensor/bme68x_bsec2" "vendor-provided/maintained libraries for some functionality" >}}.

  - significantly more flash memory.

    - Most ESP8266 boards have just 1 or 2 MBs; meanwhile...
    - Most ESP32s and variants have at least 4 MBs, but some have 8, 16 or even 32 MBs!

    More RAM and/or flash memory means you can have bigger/more complex ESPHome configurations.

  If saving a dollar or so on a cheaper microcontroller is tempting, keep in mind that *you'll just have to buy yet
  another, different/"better" board when you realize that the cheaper one doesn't meet the needs of your project(s).*
  This approach ultimately ends up costing **more.** If you *can*, spend that little bit extra to get a board which
  will have better longevity and work for more of your projects!

- *What about the ESP32-C6/ESP32-H2/[latest Espressif chip]/RP2040/RP2350?*

  - Support for these is less mature so you're more likely to run into problems with these devices.
  - We recommend sticking to the microcontrollers we've recommended above for the best ESPHome experience.

- We'll update our recommendations here as support is added/matures for newer microcontrollers.

{{< anchor "faq-usb_installation" >}}

## How do I install ESPHome onto my device?

You can use the [ESPHome Device Builder](/guides/getting_started_hassio#installing-esphome-device-builder) directly; after editing your device's
configuration to your liking, click "INSTALL" and follow the prompts. Note that the first time you install ESPHome onto
a (new) device, you need to connect it with a (USB) cable; this installation method requires a browser that supports
WebSerial, like Google Chrome or Microsoft Edge.

If you prefer the more manual way:

1. You need to obtain the firmware file to install:

   - **Using ESPHome CLI**

     The file is available in the `<CONFIG_DIR>/<NODE_NAME>/.pioenvs/<NODE_NAME>/firmware.bin` directory after the
     build completes.

   - **Using ESPHome Dashboard**

     1. Click on the three-dot menu for your device
     1. Select "Install"
     1. Choose "Manual download"

1. On some boards, you may need to force the microcontroller into its [programming mode](/guides/physical_device_connection#esphome-phy-con-prg).
   This often isn't necessary on most modern boards/devices, but it's worth trying if you're experiencing difficulties.

1. Finally, to install a firmware file, you can use:

   - [ESPHome Web](https://web.esphome.io/), our web-based installer. This is the easiest approach but requires a

       browser that supports WebSerial, like Google Chrome or Microsoft Edge.

       1. Connect the board to your computer, make sure it's detected as a [serial port](/guides/physical_device_connection#esphome-phy-con-drv) and
          click **Connect**.

       1. If prompted, allow your browser the requested permission in the pop-up box that appears.
       1. Select the serial device associated with your board
       1. Click **Install** and browse for/select the binary file you downloaded earlier (as above).

       Note that the file is processed locally and is **not** uploaded to any cloud service.

   - `esptool` [from the GitHub repository](https://github.com/espressif/esptool/releases). It's likely available

       as package for your OS or you can try installing it with `pip install esptool` (in case of Linux).

{{< anchor "esphome-esptool" >}}

## What is `esptool`  ?

`esptool` is a command-line/terminal application which can be used to perform a variety of tasks on Espressif
microcontrollers. It's not the most user-friendly approach, but it's quite powerful and can be useful if you get stuck.

> [!NOTE]
> Before using `esptool`, make sure you know which serial port your board/serial adapter is connected to!
>
> - In Linux, you can use the `dmesg` command after you plug the device into the USB port to see the name of the
>   (new) serial port.
>
> - In Windows, look in the Device Manager to see if a new serial port appears when you plug it in and note the (new)
>   port's COM number.

### Erase flash

This erases your microcontroller's flash memory -- nothing (settings, data, etc.) will remain!

```bash
esptool --port /dev/ttyUSB0 erase_flash
```

### Write flash

This will install ("flash") your binary (ESPHome) onto your microcontroller.

```bash
esptool --port /dev/ttyUSB0 write_flash 0x0 your_node_firmware.bin
```

{{< anchor "faq-usb_troubleshooting" >}}

## I can't get installation over USB to work

There are a number of reasons this may happen.

### Common Issues

- You are **not using a USB data cable.** To reduce cost, many USB cables are designed for *battery charging only* and
  they are not capable of establishing the data connection required to communicate with your board.

- ESPHome depends on your computer's operating system (OS) to enable the programming tool (`esptool.py`, for example)
  to communicate with your microcontroller board; you may need to [install appropriate drivers](/guides/physical_device_connection#esphome-phy-con-drv).

- If you're trying to install ESPHome onto your device from within a Docker container, be sure you are mounting the
  device into your container using `--device=/dev/ttyUSB0`.

### `esptool` Troubleshooting

If you're just seeing `Connecting....____....` on the screen and installation ("flashing") fails:

- Verify that the name of the device's port has not changed; this can happen if you disconnect and then reconnect it
  too quickly (for example, it might change from `/dev/ttyUSB0` to `/dev/ttyUSB1`  ).

- If you're using an external USB-to-serial adapter, confirm that the wires are connected correctly. The receive (RX)
  line from the adapter should be connected to the transmit (TX) line of your board (and vice-versa for the other
  wire).

- Some devices may require you to keep `GPIO0` and `GND` connected at least until flashing has begun.
- Some devices may require you to power-cycle them to restart programming mode after erasing flash; they won't
  auto-reset.

- Last but not least, this could be a sign that your microcontroller is defective, damaged or otherwise cannot be
  programmed. :(

If you're in a noisy electrical/RF environment or are using unusually long cables/wires, installation can fail during
transfer. Don't worry -- just try again, perhaps with a reduced baud rate for safer transfers:

```bash
esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash 0x0 your_node_firmware.bin
```

If you *still* can't get it to work, you might want to revisit
[I can't get installation over USB to work](#faq-usb_troubleshooting) above.

## Tips for using ESPHome

- ESPHome supports (most of) [Home Assistant's YAML configuration directives][ha-docs-split] like `!include`
  and `!secret`.
  This allows you to store your secrets (for example, Wi-Fi passwords and API keys) in a file called `secrets.yaml`,
  as long as this file is in the same directory as your ESPHome configuration file.

  We've enhanced ESPHome's `!include` directive such that it accepts a list of variables that can be substituted
  within the included file. For example:

[ha-docs-split]: https://www.home-assistant.io/docs/configuration/splitting_configuration/

```yaml
    binary_sensor:
      - platform: gpio
        id: button1
        pin: GPIOXX
        on_multi_click: !include { file: on-multi-click.yaml, vars: { id: 1 } } # inline syntax
      - platform: gpio
        id: button2
        pin: GPIOXX
        on_multi_click: !include
          # multi-line syntax
          file: on-multi-click.yaml
          vars:
            id: 2
```

  `on-multi-click.yaml`  :

```yaml
    - timing: !include click-single.yaml
      then:
        - mqtt.publish:
            topic: ${device_name}/button${id}/status
            payload: single
    - timing: !include click-double.yaml
      then:
        - mqtt.publish:
            topic: ${device_name}/button${id}/status
            payload: double
```

- You can use {{< docref "/components/substitutions" >}} to build on the examples above and reduce repetition in your
  configuration files.

- If you want to see how ESPHome interprets your configuration, run:

```bash
    esphome config livingroom.yaml
```

- To view the logs from your ESPHome node without uploading, run:

```bash
    esphome logs livingroom.yaml
```

- You can always find the source ESPHome generates in the `<NODE_NAME>/src/` directory.

- You can view the full list of command line interface options here: {{< docref "/guides/cli" >}}

## Help! Something's not working

That's no good. Here are some steps that resolve some problems:

- **If you're having Wi-Fi problems**: See [My node keeps reconnecting randomly](#wifi-problems).
- [Enable verbose logs](/components/logger#logger-log_levels) in your ESPHome device's `logger:` section.
- **If your device is crashing**: See the {{< docref "/guides/troubleshooting" >}} guide for how to get a backtrace.
- **Still seeing an error?** Check if there is a known issue in the
  [ESPHome issue tracker](https://github.com/esphome/esphome/issues). If not, you can create a new issue to describe
  your problem there. We will take a look at it as soon as we can. Thanks!

{{< anchor "faq-bug_report" >}}

## How do I report an issue?

ESPHome is a big project and many aspects are in general use and known to work well. That said, some parts are less
frequently used and, as such, less tested. We try our best to test as much as we can, but we simply don't have every
single piece of hardware that ESPHome supports/implements. We rely heavily on testing done by the community and our
contributors. As we make changes, it can happen that something somewhere breaks. Issue reports are a great way for us
to track and (hopefully/eventually) fix issues.

When filing an issue, it's important to be as descriptive as possible -- but do avoid excessive extraneous information.
If you want the issue you're experiencing to be fixed quickly:

- **Just writing "X doesn't work" or "X gives bug" is not helpful!!!** Seriously, how do you expect help given just
  that information?

- Provide a snippet of the code/configuration which triggers the issue; we'll likely want to try to reproduce it.
  Please read [How to create a Minimal, Complete, and Verifiable example](https://stackoverflow.com/help/mcve).

- If it's a hardware communication issue (such as with an I²C or SPI device), try setting the
  [log level](/components/logger#logger-log_levels) to `VERY_VERBOSE` as it may provide better insight into what is going on.

- Please describe what troubleshooting steps you've already tried as that may also help us track down the issue.

You can find our issue tracker [on GitHub](https://github.com/esphome/esphome/issues).

## How do I update to the latest version?

In Home Assistant, an update notification will appear when there's an update available (as with all add-ons).

If you're running the Docker container independently of Home Assistant, run:

```bash
pip3 install -U esphome
# From docker:
docker pull ghcr.io/esphome/esphome:stable
```

{{< anchor "faq-beta" >}}

## How do I update to the latest beta release?

ESPHome has a beta release cycle so that new releases can easily be tested before the changes are deployed to the
stable channel. You can help test ESPHome (and use new features) by installing the beta:

- For Home Assistant supervised installs, search for "ESPHome" in the Add-on Store. Note that the add-ons are named
  accordingly; for the beta version, you'll want "ESPHome Device Builder (beta)".

- If you're running the container in Docker independently of Home Assistant:

```bash
    # For pip-based installs
    pip3 install --pre -U esphome

    # For docker-based installs
    docker run [...] -it ghcr.io/esphome/esphome:beta run livingroom.yaml
```

The beta documentation is available at [beta.esphome.io](https://beta.esphome.io).

## How do I use the latest bleeding-edge version?

First, a fair warning that the latest bleeding-edge version is not always stable. You may encounter unusual problems
and/or undocumented/unexpected changes. We do not generally *support* running ESPHome `dev` -- it's usually something
only developers use.

That aside, if you want to install the `dev` version of ESPHome:

- For Home Assistant supervised installs, search for "ESPHome" in the Add-on Store. Note that the add-ons are named
  accordingly; for the dev version, you'll want "ESPHome Device Builder (dev)".

- From `pip`  :

```bash
    pip3 install https://github.com/esphome/esphome/archive/dev.zip
```

- From docker, use the [ghcr.io/esphome/esphome:dev](https://github.com/esphome/esphome/pkgs/container/esphome/)
  image.

```bash
    docker run [...] -it ghcr.io/esphome/esphome:dev livingroom.yaml compile
```

The dev documentation is available at [next.esphome.io](https://next.esphome.io/).

## How do I use my Home Assistant `secrets.yaml` file?

If you want to keep all your secrets in one place, make a `secrets.yaml` file in your `esphome` directory with
the following contents:

```yaml
<<: !include ../secrets.yaml
```

This "pulls in" the contents of your Home Assistant `secrets.yaml` file from the parent directory.

## Does ESPHome support [this device/feature]?

If it's not in {{< docref "/index" "the docs" >}}, it's not (officially) supported. However, we are always adding
support for new features.

In some cases, community-provided {{< docref "/components/external_components" >}} are available; keep in mind that
these are not officially supported by ESPHome, so, if you run into problems, you'll need to contact the developer of
the external component you're using for help.

You can also create a feature request in our
[ESPHome feature request tracker](https://github.com/orgs/esphome/discussions).

## I have a question... How can I contact you?

Sure! We are happy to help :) You can contact us here:

- [Discord](https://discord.gg/KhAMKrd)
- [Home Assistant Community Forums](https://community.home-assistant.io/c/esphome)
- ESPHome [issue](https://github.com/esphome/esphome/issues) and
   [feature request](https://github.com/orgs/esphome/discussions) trackers. Preferably only for issues and
   feature requests.

- **If your inquiry is not regarding support**, you can [e-mail us](mailto:esphome@openhomefoundation.org).

{{< anchor "wifi-problems" >}}

## My node keeps reconnecting randomly

This is a known issue but seems to be very low-level (in other words, not specifically an ESPHome problem) and we don't
really know how to solve it. We're trying to identify workarounds for the issue, but there isn't a single, specific
solution right now.

Here are some steps that may help mitigate the issue:

- If you're using a hidden Wi-Fi network, make sure to enable `fast_connect` mode in your device's Wi-Fi
  configuration. Note that this may help with non-hidden networks, as well.

- Give your ESPHome device a [static IP](/components/wifi#wifi-manual_ip).
- Set the `power_save_mode` to `light` in your `wifi:` configuration. Note, however, that this may exacerbate the
  problem in some situations. See [Power Save Mode](/components/wifi#wifi-power_save_mode).

- The issue seems to happen with "cheap" boards more frequently -- especially the "cheap" NodeMCU boards from eBay
  which sometimes have bad antennas.

- ESPHome intentionally reboots after a configured duration in specific situations, such as when the
  {{< docref "/components/wifi" "Wi-Fi connection cannot be made" >}},
  {{< docref "/components/api" "API connection is lost" >}} or
  {{< docref "/components/mqtt" "MQTT connection is lost" >}}. To disable this behavior, you'll need to explicitly
  set the `reboot_timeout` option to `0s` on the relevant components.

- If you see `Error: Disconnecting <NODE_NAME>` in your logs, ESPHome is actively closing the native API client
  connection. You'll need to establish a serial connection with your device to determine the reason. If you see
  `ack timeout 4` immediately before the disconnect, this might be because of a bug in the AsyncTCP library, for
  which a fix was included in ESPHome version 1.18.0. If you are running an ESPHome version, prior to 1.18.0, you
  should upgrade ESPHome and build fresh firmware for your devices.

- We've seen an increase in disconnects while the log level is set to `VERY_VERBOSE`, especially on single-core
  devices, where the logging code might be interfering with the operation of the networking code. For this reason, we
  advise using a lower log level for production purposes.

- Too many clients simultaneously connected to the native API server on the device may also result in this behavior.
  For example, the Home Assistant ESPHome integration and the log viewer on the
  [ESPHome Device Builder](/guides/getting_started_hassio#installing-esphome-device-builder) each establish a connection to the device. In
  production, you will likely only have a single connection from Home Assistant, making this less of an issue. Still,
  beware that attaching a log viewer might have an impact.

- Reducing the Delivery Traffic Indication Message (DTIM) interval in the Wi-Fi access point may help improve Wi-Fi
  reliability and responsiveness. This will cause Wi-Fi devices in power save mode to wake up more frequently. This
  may mitigate disconnections at the expense of increasing power (and possibly battery) usage of other devices also
  using power save modes.

## Component states not restored after reboot

Some components, such as `climate` and `switch` components, are able to restore their states following a
reboot/power-cycle of the microcontroller. If you've configured this for a given component but find that its state is
not restored as expected, or you get periodic `ESP_ERR_NVS_NOT_ENOUGH_SPACE` errors in your device's log, it could be
that the NVS portion of the flash memory is full. This can happen for a number of reasons, but, regardless, you can try
wiping the NVS partition with the following commands:

```bash
dd if=/dev/zero of=nvs_zero bs=1 count=20480
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x009000 nvs_zero
```

Change `/dev/ttyUSB0` above to your serial port. If you have changed the partition layout, you'll need to adjust the
above offsets and sizes accordingly.

## Docker Reference

Install versions:

```bash
# Stable Release
docker pull ghcr.io/esphome/esphome
# Beta
docker pull ghcr.io/esphome/esphome:beta
# Dev version
docker pull ghcr.io/esphome/esphome:dev
```

ESPHome Command Reference:

```bash
# Start a new file wizard for file livingroom.yaml
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome wizard livingroom.yaml

# Compile and upload livingroom.yaml
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome run livingroom.yaml

# View logs
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome logs livingroom.yaml

# Map /dev/ttyUSB0 into container
docker run --rm -v "${PWD}":/config --device=/dev/ttyUSB0 -it ghcr.io/esphome/esphome ...

# Start dashboard on port 6052 (general command)
# Warning: this command is currently not working with Docker on MacOS. (see note below)
docker run --rm -v "${PWD}":/config --net=host -it ghcr.io/esphome/esphome

# Start dashboard on port 6052 (MacOS specific command)
docker run --rm -p 6052:6052 -e ESPHOME_DASHBOARD_USE_PING=true -v "${PWD}":/config -it ghcr.io/esphome/esphome

# Setup a bash alias:
alias esphome='docker run --rm -v "${PWD}":/config --net=host -it ghcr.io/esphome/esphome'
```

Docker Compose example:

```yaml
version: '3'

services:
  esphome:
    image: ghcr.io/esphome/esphome
    volumes:
      - ./:/config:rw
      # Use local time for logging timestamps
      - /etc/localtime:/etc/localtime:ro
    devices:
      # if needed, add esp device(s) as in command line examples above
      - /dev/ttyUSB0:/dev/ttyUSB0
      - /dev/ttyACM0:/dev/ttyACM0
    # The host networking driver only works on Linux hosts, but is available as a Beta feature,
    # on Docker Desktop version 4.29 and later.
    network_mode: host
    restart: always
```

{{< anchor "docker-reference-notes" >}}

> [!NOTE]
> By default, ESPHome uses mDNS to resolve device IPs on the network; this is used to determine online/offline state
> in the [ESPHome Device Builder](/guides/getting_started_hassio#installing-esphome-device-builder). In order for this feature to work, you
> must use Docker's host networking mode.
>
> The [host networking driver](https://docs.docker.com/network/drivers/host/) only works on Linux hosts; it is
> available on Docker Desktop version 4.29 and later.
>
> If you don't want to use the host networking driver, you have to use an alternate method as described below.
>
> Note that mDNS might not work if your Home Assistant server and your ESPHome nodes are on different subnets
> and/or VLANs. If your router supports Avahi, you can configure mDNS to work across different subnets.
> For example, in OpenWRT or pfSense:
>
> 1. Enable Avahi on both subnets (install Avahi modules on OpenWRT or pfSense).
> 1. Enable UDP traffic from your ESPHome device's subnet to 224.0.0.251/32 on port 5353.
>
> Alternatively, you can configure the [ESPHome Device Builder](/guides/getting_started_hassio#installing-esphome-device-builder) to use ICMP
> pings to check the status of devices by setting `"status_use_ping": true` or, with Docker:
> `-e ESPHOME_DASHBOARD_USE_PING=true`
>
> See also <https://github.com/esphome/issues/issues/641#issuecomment-534156628>.

{{< anchor "faq-notes_on_disabling_mdns" >}}

## Notes on disabling mDNS

Some of ESPHome's functionality relies on {{< docref "/components/mdns" "mDNS" >}}, so, naturally, disabling it will
cause these features to stop working.

Generally speaking, disabling mDNS without setting a [static IP address](/components/wifi#wifi-manual_ip) (or a static DHCP lease)
is bound to cause problems -- mDNS is used to determine the IP address of each ESPHome node.

If you disable mDNS, expect the following repercussions:

- You will not be able to use the node's hostname to ping, find it's IP address or otherwise connect to it.
- Automatic discovery in Home Assistant when using the {{< docref "/components/api" "native API" >}} relies on
  mDNS broadcast messages to detect the presence of new ESPHome nodes. If you need to use the native API with
  mDNS disabled, then you will have to use a static IP address and manually add the ESPHome component with its
  (static) IP address.

- Because status detection in the [ESPHome Device Builder](/guides/getting_started_hassio#installing-esphome-device-builder) uses mDNS by
  default, nodes with mDNS disabled will always appear as "offline". This does not affect any functionality; however,
  if you want to see the online/offline status of your nodes, you may configure the ESPHome Device Builder to ping each
  node instead. See the [notes in the Docker Reference section](#docker-reference-notes) for more information.

## Can configuration files be recovered from the device?

ESPHome YAML configuration files are not stored on ESPHome devices. If you lost your configuration(s), there's no way
to recover them from the device; you'll have to recreate them from scratch if you don't have a backup elsewhere.

Always back up your files!

## Why shouldn't I use underscores in my device name?

The top level `name:` field in your `.yaml` configuration file defines the node name ("hostname") on the local
network. According to [RFC1912](https://datatracker.ietf.org/doc/html/rfc1912), underscore (`_`  ) characters in
hostnames are not valid. In practice, some DNS/DHCP setups may work correctly with underscores while others will not.
If you're using static IP addresses, you're unlikely to encounter any issues. In some cases, initial setup may work,
but connections might fail when Home Assistant restarts or if you change router hardware.

We recommend using a hyphen (`-`  ) instead of underscore.

Important: follow these [instructions](/components/esphome.html#changing-esphome-node-name) to use the
`use_address` parameter when renaming a live device, as the connection to an existing device will only
work with the old name until the name change is complete.

{{< anchor "strapping-warnings" >}}

## Why am I getting a warning about strapping pins?

Most microcontrollers have special "strapping pins" that are read during the boot-up procedure. The state of the pin(s)
determines the outcome of the boot process. There are effectively two "modes" into which the microcontroller can boot:

- Normal mode
- "Flashing" or "bootloader" mode

While the use of these pins in software is not specifically a problem, if some external hardware component/device
connected to one of these pins is arbitrarily changing the state of the pin, boot failures or other
difficult-to-diagnose behavior may occur.

We recommended avoiding these pins unless absolutely necessary and you fully understand the expected state of these
pins at boot.

Some development boards connect GPIO 0 to a button, often labeled "boot". Holding this button while the ESP is powered
on/reset will cause it to go into bootloader mode. Once the ESP is fully booted up, this button can safely be used as a
normal input.

Strapping pins are generally safe to use as outputs if they are *only* connected to other devices that have
high- impedance inputs with no pull-up or pull-down resistors. Note that I2C clock and data lines *require* pull-up
resistors and are not safe on strapping pins.

If you are absolutely sure that your use of strapping pins is safe and you want to suppress the warning, you can add
`ignore_strapping_warning: true` to the relevant pin configuration(s).

## How can I test a pull request?

By leveraging the {{< docref "/components/external_components" >}} feature, it's possible to test most pull requests by
simply adding a few lines to your YAML! You need the number of the pull request as well as the component(s) that have
been added or changed by the pull request (they are listed with the "integration:" labels on the GitHub page of the pull
request). Then, if you add a block of code (similar to that shown below) to your YAML configuration, recompile and
reinstall/update ESPHome onto your device, the code from the pull request will be used for the component(s) changed by
the pull request.

```yaml
external_components:
  # replace 1234 with the number of the pull request
  - source: github://pr#1234
    components:
      # list all components modified by this pull request here
      - ccs811
```

Note that this only works for pull requests that only change files within components. If any files outside
`esphome/components/` are added or changed, this method won't work. Those pull requests are labeled with the "core"
label on GitHub.

## Why do entities appear as "unavailable" during deep sleep?

The {{< docref "/components/deep_sleep" "Deep Sleep" >}} component needs to be present within your device's
configuration when the device is first added to Home Assistant. To prevent entities from appearing as "unavailable",
you can remove and re-add the device in Home Assistant.

## How do I secure my ESPHome devices?

See the comprehensive {{< docref "security_best_practices" >}} guide for detailed recommendations on API encryption, OTA passwords, network segmentation, physical security, and more.

## See Also

- {{< docref "/index" "ESPHome index" >}}
- [Developer site](https://developers.esphome.io)
