---
description: "Instructions for setting up Seeed Studio MR60BHA2 60GHz mmWave Breathing and Heartbeat Detection Sensor Kit."
title: "Seeed Studio MR60BHA2 60GHz mmWave Breathing and Heartbeat Detection Sensor Kit"
params:
  seo:
    description: Instructions for setting up Seeed Studio MR60BHA2 60GHz mmWave Breathing and Heartbeat Detection Sensor Kit.
    image: seeed_mr60bha2.jpg
---

## Component/Hub

The `seeed_mr60bha2` platform allows you to use Seeed Studio MR60BHA2 60GHz mmWave Fall Detection Sensor Kit with XIAO ESP32C6 ([Product Page](https://www.seeedstudio.com/MR60BHA2-60GHz-mmWave-Sensor-Breathing-and-Heartbeat-Module-p-5945.html)) with ESPHome.

The [UART](/components/uart) is required to be set up in your configuration for this sensor to work, `parity` and `stop_bits` **must be** respectively `NONE` and `1`.
You can use the ESP32 software or hardware serial to use this MR60BHA2, its default baud rate is 115200.

{{< img src="seeed_mr60bha2.jpg" alt="Image" caption="Seeed Studio MR60BHA2 60GHz mmWave Fall Detection Sensor Kit with XIAO ESP32C6" width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
seeed_mr60bha2:
```

### Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID for this {{< docref "seeed_mr60bha2/" >}} component if you need multiple components.

## Binary Sensor

The `seeed_mr60bha2` binary sensor allows you to determine the presence of a human.

```yaml
binary_sensor:
  - platform: seeed_mr60bha2
    has_target:
      name: "Person Information"
```

### Configuration variables

- **has_target** (*Optional*): If true when target (person) is detected.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## Sensor

The `seeed_mr60bha2` sensor allows you to perform different measurements.

```yaml
sensor:
  - platform: seeed_mr60bha2
    breath_rate:
      name: "Real-time respiratory rate"
    heart_rate:
      name: "Real-time heart rate"
    distance:
      name: "Distance to detection object"
    num_targets:
      name: "Target number"
```

### Configuration variables

- **breath_rate** (*Optional*, float): Radar-detected respiratory rate during the first 60 seconds.
  All options from [Sensor](/components/sensor).

- **heart_rate** (*Optional*, float): Heart rate during the first 60 seconds as detected by the radar.
  All options from [Sensor](/components/sensor).

- **distance** (*Optional*, float): Straight-line distance between the radar and the monitoring object.
  All options from [Sensor](/components/sensor).

- **num_targets** (*Optional*, int): The number of target detected by the radar.
  All options from [Sensor](/components/sensor).

## See Also

- [Official Using Documents for Seeed Studio MR60BHA2 60GHz mmWave Breathing and Heartbeat Detection Sensor Kit with XIAO ESP32C6](https://wiki.seeedstudio.com/getting_started_with_mr60bha2_mmwave_kit/)
- [Product Detail Page for Seeed Studio MR60BHA2 60GHz mmWave Breathing and Heartbeat Detection Sensor Kit with XIAO ESP32C6](https://www.seeedstudio.com/MR60BHA2-60GHz-mmWave-Sensor-Breathing-and-Heartbeat-Module-p-5945.html)
- [Source of inspiration for implementation](https://github.com/limengdu/MR60BHA2_ESPHome_external_components/)
- {{< apiref "seeed_mr60bha2/seeed_mr60bha2.h" "seeed_mr60bha2/seeed_mr60bha2.h" >}}
