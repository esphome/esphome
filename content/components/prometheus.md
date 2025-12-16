---
description: "Instructions for setting up a prometheus exporter with ESPHome."
title: "Prometheus Component"
params:
  seo:
    description: Instructions for setting up a prometheus exporter with ESPHome.
    image: prometheus.svg
---

The `prometheus` component enables an HTTP endpoint for the
{{< docref "web_server/" >}} in order to integrate a [Prometheus](https://prometheus.io/) installation.

This can be used to scrape data directly into your Prometheus-based monitoring and alerting-system,
without the need of any other software.

The list of available metrics can be found by directly browsing your node under
`<ip or node_name.local>/metrics`, and may be increased in the future.

```yaml
# Example configuration entry
web_server:

# Activates prometheus /metrics endpoint
prometheus:
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **include_internal** (*Optional*, boolean): Whether `internal` entities should be displayed on the
  web interface. Defaults to `false`.

- **relabel** (*Optional*): Override metric labels. See [`relabel`](#prometheus-relabel)

> [!NOTE]
> Example integration into the configuration of your prometheus:
>
> ```yaml
> scrape_configs:
>   - job_name: esphome
>     static_configs:
>       - targets: [<ip or node_name.local>]
> ```

## Supported ESPHome Components

- Sensor
- Binary Sensor
- Fan
- Light
- Cover
- Switch
- Lock
- Text Sensor
- Text
- Event
- Number
- Select
- Media Player
- Update
- Valve
- Climate

## Supported Prometheus Labels

The following labels are supported in all Prometheus metrics. Some metrics may have more labels.

- entity id
- entity name
- entity friendly name
- area
- node name

## Metric Relabeling

ESPHome allows you to do some basic relabeling of Prometheus metrics.
This is useful if you want to have different metric names or IDs than those shown in Home Assistant or the web interface.

You can relabel metric name or ID labels by adding a `relabel` block in the `prometheus` configuration,
and then adding a block with `id` and/or `name` fields for each sensor whose labels your want to override.

{{< anchor "prometheus-relabel" >}}

### `relabel`

Set the `id` and `name` label values of the Prometheus metric for the sensor with the specified ID.

```yaml
# Example configuration entry
prometheus:
  relabel:
    my_voltage_sensor:
      id: angry_pixies
      name: "Angry Pixies"
```

## Example Metrics

Here's an example of *some* of the many prometheus metrics available:

```text
#TYPE esphome_sensor_value gauge
#TYPE esphome_sensor_failed gauge
esphome_sensor_failed{id="dev_idf_prometheus_goober_wifi_signal_db",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober WiFi Signal dB"} 0
esphome_sensor_value{id="dev_idf_prometheus_goober_wifi_signal_db",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober WiFi Signal dB",unit="dBm"} -35
esphome_sensor_failed{id="dev_idf_prometheus_goober_wifi_signal_percent",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober WiFi Signal Percent"} 0
esphome_sensor_value{id="dev_idf_prometheus_goober_wifi_signal_percent",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober WiFi Signal Percent",unit="Signal %"} 100
esphome_sensor_failed{id="dev_idf_prometheus_goober_uptime_sensor",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Uptime Sensor"} 0
esphome_sensor_value{id="dev_idf_prometheus_goober_uptime_sensor",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Uptime Sensor",unit="s"} 4
esphome_sensor_failed{id="dev_idf_prometheus_goober_uptime_timestamp",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Uptime Timestamp"} 1
esphome_sensor_failed{id="dev_idf_prometheus_goober_esphome_internal_temperature",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober ESPHome Internal Temperature"} 0
esphome_sensor_value{id="dev_idf_prometheus_goober_esphome_internal_temperature",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober ESPHome Internal Temperature",unit="°C"} 33.1
esphome_sensor_failed{id="heap_free",area="Dev",node="devidfprometheus",name="Heap Free"} 0
esphome_sensor_value{id="heap_free",area="Dev",node="devidfprometheus",name="Heap Free",unit="B"} 224724
esphome_sensor_failed{id="heap_max_block",area="Dev",node="devidfprometheus",name="Heap Max Block"} 0
esphome_sensor_value{id="heap_max_block",area="Dev",node="devidfprometheus",name="Heap Max Block",unit="B"} 147456
esphome_sensor_failed{id="loop_time",area="Dev",node="devidfprometheus",name="Loop Time"} 0
esphome_sensor_value{id="loop_time",area="Dev",node="devidfprometheus",name="Loop Time",unit="ms"} 17
esphome_sensor_failed{id="cpu_frequency",area="Dev",node="devidfprometheus",name="CPU Frequency"} 0
esphome_sensor_value{id="cpu_frequency",area="Dev",node="devidfprometheus",name="CPU Frequency",unit="Hz"} 160000000
#TYPE esphome_binary_sensor_value gauge
#TYPE esphome_binary_sensor_failed gauge
esphome_binary_sensor_failed{id="dev_idf_prometheus_goober_button",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Button"} 0
esphome_binary_sensor_value{id="dev_idf_prometheus_goober_button",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Button"} 0.000000
esphome_binary_sensor_failed{id="dev_idf_prometheus_goober_connected_status",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Connected Status"} 0
esphome_binary_sensor_value{id="dev_idf_prometheus_goober_connected_status",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Connected Status"} 0.000000
#TYPE esphome_light_state gauge
#TYPE esphome_light_color gauge
#TYPE esphome_light_effect_active gauge
esphome_light_state{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light"} 0.000000
esphome_light_color{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light",channel="brightness"} 0.000000
esphome_light_color{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light",channel="r"} 0.000000
esphome_light_color{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light",channel="g"} 0.000000
esphome_light_color{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light",channel="b"} 0.000000
esphome_light_color{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light",channel="w"} 0.000000
esphome_light_effect_active{id="dev_idf_prometheus_goober_light",area="Dev",node="devidfprometheus",name="Dev IDF Prometheus Goober Light",effect="None"} 0
#TYPE esphome_switch_value gauge
#TYPE esphome_switch_failed gauge
esphome_switch_failed{id="beta_firmware",area="Dev",node="devidfprometheus",name="Beta firmware"} 0
esphome_switch_value{id="beta_firmware",area="Dev",node="devidfprometheus",name="Beta firmware"} 0.000000
#TYPE esphome_text_sensor_value gauge
#TYPE esphome_text_sensor_failed gauge
esphome_text_sensor_failed{id="esphome_version_detailed",area="Dev",node="devidfprometheus",name="ESPHome Version Detailed"} 0
esphome_text_sensor_value{id="esphome_version_detailed",area="Dev",node="devidfprometheus",name="ESPHome Version Detailed",value="2025.8.0b1 Aug 16 2025, 13:48:52"} 1.0
esphome_text_sensor_failed{id="esphome_version",area="Dev",node="devidfprometheus",name="ESPHome Version"} 0
esphome_text_sensor_value{id="esphome_version",area="Dev",node="devidfprometheus",name="ESPHome Version",value="2025.8.0b1"} 1.0
esphome_text_sensor_failed{id="esphome_project_version",area="Dev",node="devidfprometheus",name="ESPHome Project Version"} 0
esphome_text_sensor_value{id="esphome_project_version",area="Dev",node="devidfprometheus",name="ESPHome Project Version",value="4.13.8"} 1.0
esphome_text_sensor_failed{id="esphome_project_version_detailed",area="Dev",node="devidfprometheus",name="ESPHome Project Version Detailed"} 0
esphome_text_sensor_value{id="esphome_project_version_detailed",area="Dev",node="devidfprometheus",name="ESPHome Project Version Detailed",value="4.13.8 Aug 16 2025, 13:48:52"} 1.0
esphome_text_sensor_failed{id="esphome_board",area="Dev",node="devidfprometheus",name="ESPHome Board"} 0
esphome_text_sensor_value{id="esphome_board",area="Dev",node="devidfprometheus",name="ESPHome Board",value="esp32-s3-devkitc-1"} 1.0
esphome_text_sensor_failed{id="esphome_board_variant",area="Dev",node="devidfprometheus",name="ESPHome Board Variant"} 0
esphome_text_sensor_value{id="esphome_board_variant",area="Dev",node="devidfprometheus",name="ESPHome Board Variant",value="ESP32-S3"} 1.0
esphome_text_sensor_failed{id="esphome_framework_type",area="Dev",node="devidfprometheus",name="ESPHome Framework Type"} 0
esphome_text_sensor_value{id="esphome_framework_type",area="Dev",node="devidfprometheus",name="ESPHome Framework Type",value="ESP-IDF"} 1.0
esphome_text_sensor_failed{id="esphome_idf_version",area="Dev",node="devidfprometheus",name="ESPHome IDF Version"} 0
esphome_text_sensor_value{id="esphome_idf_version",area="Dev",node="devidfprometheus",name="ESPHome IDF Version",value="5.4.2"} 1.0
esphome_text_sensor_failed{id="reset_reason",area="Dev",node="devidfprometheus",name="Reset Reason"} 0
esphome_text_sensor_value{id="reset_reason",area="Dev",node="devidfprometheus",name="Reset Reason",value="USB peripheral"} 1.0
#TYPE esphome_number_value gauge
#TYPE esphome_number_failed gauge
esphome_number_failed{id="template_number",area="Dev",node="devidfprometheus",name="Template number"} 0
esphome_number_value{id="template_number",area="Dev",node="devidfprometheus",name="Template number"} 0.000000
#TYPE esphome_select_value gauge
#TYPE esphome_select_failed gauge
esphome_select_failed{id="template_select",area="Dev",node="devidfprometheus",name="Template select"} 0
esphome_select_value{id="template_select",area="Dev",node="devidfprometheus",name="Template select",value="two"} 1.0
#TYPE esphome_update_entity_state gauge
#TYPE esphome_update_entity_info gauge
#TYPE esphome_update_entity_failed gauge
esphome_update_entity_failed{id="firmware_update",area="Dev",node="devidfprometheus",name="Firmware Update"} 1
#TYPE esphome_text_value gauge
#TYPE esphome_text_failed gauge
esphome_text_failed{id="template_text",area="Dev",node="devidfprometheus",name="Template text"} 0
esphome_text_value{id="template_text",area="Dev",node="devidfprometheus",name="Template text",value="Hello World"} 1.0
#TYPE esphome_event_value gauge
#TYPE esphome_event_failed gauge
esphome_event_failed{id="template_event",area="Dev",node="devidfprometheus",name="Template Event"} 0
esphome_event_value{id="template_event",area="Dev",node="devidfprometheus",name="Template Event",last_event_type="custom_event_1"} 1.0
```

## See Also

- {{< docref "/components/web_server" >}}
- [REST API](/web-api#api-rest)
- {{< docref "/components/http_request" >}}
- {{< apiref "prometheus/prometheus_handler.h" "prometheus/prometheus_handler.h" >}}
- [Prometheus](https://prometheus.io/)
