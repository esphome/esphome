---
description: "Instructions for setting up Seeed Studio MR60FDA2 60GHz mmWave Fall Detection Sensor Kit."
title: "Seeed Studio MR60FDA2 60GHz mmWave Fall Detection Sensor Kit"
params:
  seo:
    description: Instructions for setting up Seeed Studio MR60FDA2 60GHz mmWave Fall Detection Sensor Kit.
    image: seeed_mr60fda2.jpg
---

## Component/Hub

The `seeed_mr60fda2` platform allows you to use Seeed Studio's MR60FDA2 60GHz mmWave Fall Detection Sensor Kit with XIAO ESP32C6 ([Product Page](https://www.seeedstudio.com/MR60FDA2-60GHz-mmWave-Sensor-Fall-Detection-Module-p-5946.html)) with ESPHome.

The [UART](/components/uart) is required to be set up in your configuration for this sensor to work, `parity` and `stop_bits` **must be** respectively `NONE` and `1`.
You can use the ESP32 software or hardware (recommended) serial to use the MR60FDA2; its default baud rate is 115200.

{{< img src="seeed_mr60fda2.jpg" alt="Image" caption="Seeed Studio MR60FDA2 60GHz mmWave Fall Detection Sensor Kit with XIAO ESP32C6" width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
seeed_mr60fda2:
```

### Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID for this {{< docref "seeed_mr60fda2/" >}} component if you need multiple components.

## Binary Sensor

The `seeed_mr60fda2` binary sensor allows you to determine the presence of a human.

```yaml
binary_sensor:
  - platform: seeed_mr60fda2
    people_exist:
      name: "Person Information"
    fall_detected:
      name: "Falling Detected"
```

### Configuration variables

- **people_exist** (*Optional*): If true when target (person) is detected.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **fall_detected** (*Optional*): An indication of whether the radar has detected a fall.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

## Button

The `seeed_mr60fda2` button allows you to perform actions.

```yaml
button:
  - platform: seeed_mr60fda2
    get_radar_parameters:
      name: "Get Radar Parameters"
    factory_reset:
      name: "Reset"
```

### Configuration variables

- **factory_reset** (*Optional*): Restore all radar settings to factory parameters.
  All options from [Button](/components/button#config-button).

- **get_radar_parameters** (*Optional*): Get all the current setup parameters of the radar.
  All options from [Button](/components/button#config-button).

## Select

The `seeed_mr60fda2` select allows you to control the configuration.

```yaml
select:
  - platform: seeed_mr60fda2
    install_height:
      name: "Set Install Height"
    height_threshold:
      name: "Set Height Threshold"
    sensitivity:
      name: "Set Sensitivity"
```

### Configuration variables

- **install_height** (*Optional*): Before using the MR60FDA2, please select the installation height of the radar according to the actual situation in order to obtain accurate identification results. The default is 3m.
  All options from [Select](/components/select#config-select).

- **height_threshold** (*Optional*): To accurately distinguish between a person falling and sitting still in this area, you need to set the trigger height that triggers fall detection. This height refers to the distance between the person and the ground at the time of the fall. The default is 0.6m.
  All options from [Select](/components/select#config-select).

- **sensitivity** (*Optional*): Fall sensitivity factor. Defaults to 1 with a range of 1-3, 3 = high and 1 = low.
  All options from [Select](/components/select#config-select).

## See Also

- [Official Using Documents for Seeed Studio MR60FDA2 60GHz mmWave Fall Detection Sensor Kit with XIAO ESP32C6](https://wiki.seeedstudio.com/getting_started_with_mr60fda2_mmwave_kit/)
- [Product Detail Page for Seeed Studio MR60FDA2 60GHz mmWave Fall Detection Sensor Kit with XIAO ESP32C6](https://www.seeedstudio.com/MR60FDA2-60GHz-mmWave-Sensor-Fall-Detection-Module-p-5946.html)
- [Source of inspiration for implementation](https://github.com/limengdu/MR60FDA2_ESPHome_external_components)
- {{< apiref "seeed_mr60fda2/seeed_mr60fda2.h" "seeed_mr60fda2/seeed_mr60fda2.h" >}}
