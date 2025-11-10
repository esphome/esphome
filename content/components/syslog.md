---
description: "Instructions for setting up a syslog component in ESPHome"
title: "Syslog Component"
params:
  seo:
    description: Instructions for setting up a syslog component in ESPHome
---

The `syslog` component can be used to send ESPHome logs to a [syslog server](https://en.wikipedia.org/wiki/Syslog).
It requires both a {{< docref "udp" "UDP component" >}} and a {{< docref "time/index" "Time component" >}} to be configured.

```yaml
# Example configuration entry

udp:
  addresses: 10.0.0.1

time:
  platform: sntp

syslog:
```

## Configuration Options

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **udp_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the UDP client to use for sending logs. May be omitted if only
  one UDP client is configured.
- **time_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the time client to use for time-stamping logs. May be omitted
  if only one time client is configured.
- **port** (*Optional*, int): The port to send logs to. Defaults to `514`.
- **facility** (*Optional*, int): The syslog facility to use. Defaults to `16` (corresponding to `local0`  ).
- **level** (*Optional*, string): The highest log level to send to the syslog server. Defaults to `DEBUG`.
- **strip** (*Optional*, boolean): If set, remove color-codes from log messages. Defaults to `true`.
