---
description: "Configuration for the host platform for ESPHome."
title: "Host Platform"
params:
  seo:
    description: Configuration for the host platform for ESPHome.
    image: host.svg
---

The `host` platform allows ESPHome configurations to be compiled and run on a desktop computer. This is known
to work on MacOS and Linux. On Windows [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) (Windows Subsystem for Linux) can be used to create a Linux environment that will run ESPHome.

The only configuration required is to optionally set a dummy MAC address that will be used to identify the
configuration to Home Assistant (the native MAC address is not readily available.)

> [!NOTE]
> HA will not automatically discover an ESPHome instance running on `host` using mDNS, and you will need
> to add it explicitly using the IP address of your host computer. If HA cannot establish a connection when
> adding the device manually, the firewall settings of the local host computer may be the cause. The
> ESPHome *API* port (`6053`  ) must be allowed through the firewall.
> See {{< docref "/components/api" >}} for details.

Many components, especially those interfacing to actual hardware, will not be available when using `host`. Do not
configure wifi - network will automatically be available using the host computer.

```yaml
# Example configuration entry
host:
  mac_address: "06:35:69:ab:f6:79"
```

## Configuration variables

- **mac_address** (*Optional*, MAC address): A dummy MAC address to use when communicating with HA.

## Build and run

The `esphome run yourfile.yaml` command will compile and automatically run the build file on the `host` platform.

## See Also

- [SDL display](/components/display/sdl#sdl)
- {{< docref "esphome/" >}}
- {{< docref "/components/time/host" >}}
