# Motion Map Component

## Overview

The Motion Map component uses Wi-Fi Channel State Information (CSI) to detect motion without cameras or microphones, providing privacy-preserving presence detection for home automation.

## Features

- **Privacy-First**: No cameras or microphones required
- **CSI-Based Detection**: Analyzes Wi-Fi signal variations to detect movement
- **Multiple Sensors**: Provides variance, amplitude, entropy, and skewness measurements
- **Configurable**: Adjustable thresholds and sensitivity
- **ESP32-S3 Optimized**: Takes advantage of ESP32-S3's CSI capabilities

## Platform Support

- **ESP32-S3** with **ESP-IDF** framework only (CSI support required)

## Configuration

### Basic Example

```yaml
wifi:
  ssid: "YourSSID"
  password: "YourPassword"

motion_map:
  id: motion_map_component
  motion_threshold: 0.6
  idle_threshold: 0.2
  window_size: 100
  sensitivity: 1.5

binary_sensor:
  - platform: motion_map
    motion_map_id: motion_map_component
    name: "Motion Detected"

sensor:
  - platform: motion_map
    motion_map_id: motion_map_component
    variance:
      name: "CSI Variance"
    amplitude:
      name: "CSI Amplitude"
    entropy:
      name: "CSI Entropy"
    skewness:
      name: "CSI Skewness"
```

### Configuration Variables

#### Motion Map Component

- **motion_threshold** (*Optional*, float): Variance threshold for detecting motion. Range: 0.0-1.0. Default: 0.5
- **idle_threshold** (*Optional*, float): Variance threshold for detecting idle state. Range: 0.0-1.0. Default: 0.2
- **window_size** (*Optional*, int): Number of samples for moving window analysis. Range: 10-500. Default: 100
- **sensitivity** (*Optional*, float): Sensitivity multiplier for variance detection. Range: 0.1-5.0. Default: 1.0
- **mac_address** (*Optional*, MAC address): Filter CSI data by specific MAC address

#### Binary Sensor

- **motion_map_id** (*Required*, ID): The ID of the motion_map component
- All standard binary sensor options

#### Sensors

- **motion_map_id** (*Required*, ID): The ID of the motion_map component
- **variance** (*Optional*): CSI variance sensor configuration
- **amplitude** (*Optional*): CSI amplitude sensor configuration
- **entropy** (*Optional*): Signal entropy sensor configuration
- **skewness** (*Optional*): Signal skewness sensor configuration

Each sensor supports all standard sensor configuration options.

## How It Works

1. **CSI Capture**: The component captures Channel State Information from Wi-Fi packets
2. **Signal Analysis**: Calculates variance and amplitude from CSI subcarrier data
3. **Motion Detection**: Uses variance thresholds to determine motion vs. idle state
4. **Feature Extraction**: Computes statistical features (entropy, skewness) from the signal window
5. **State Publishing**: Updates sensors and binary sensors for Home Assistant integration

## Technical Details

### CSI (Channel State Information)

CSI provides detailed radio channel data that reveals how Wi-Fi signals propagate through space. When people move, they alter these propagation patterns, creating detectable electromagnetic changes.

### Detection Algorithm

The component uses a Moving Variance Segmentation algorithm:
- Continuously calculates variance from CSI subcarrier amplitudes
- Compares variance against configurable thresholds
- Transitions between IDLE and MOTION states based on signal characteristics

### Performance Considerations

- **Update Rate**: Sensors update every 1 second
- **Memory Usage**: Window size affects RAM usage (default: 100 samples ≈ 400 bytes)
- **CPU Usage**: CSI processing runs in Wi-Fi callback context

## Use Cases

- **Room Occupancy**: Detect presence in rooms without cameras
- **Smart Lighting**: Trigger lights based on motion
- **Security**: Privacy-preserving motion alerts
- **Multi-Room Mapping**: Deploy multiple sensors for whole-home coverage
- **Activity Recognition**: Use extracted features for ML-based activity classification

## Comparison with PIR Sensors

| Feature | Motion Map (CSI) | PIR Sensor |
|---------|------------------|------------|
| Privacy | High (no imaging) | High |
| Detection Area | 360° coverage | Directional |
| Sensitivity | Configurable | Fixed |
| Through Walls | Limited | No |
| Setup Complexity | Medium | Low |

## Troubleshooting

### CSI Not Initializing

- Verify you're using ESP32-S3 with ESP-IDF framework
- Check that Wi-Fi is enabled and connected
- Review logs for CSI initialization errors

### No Motion Detection

- Adjust `sensitivity` parameter (try 2.0-3.0 for higher sensitivity)
- Lower `motion_threshold` (try 0.3-0.4)
- Increase `window_size` for more stable detection
- Check CSI variance sensor values to verify signal capture

### False Positives

- Increase `motion_threshold` (try 0.7-0.8)
- Increase `idle_threshold` to add hysteresis
- Reduce `sensitivity` parameter
- Use `mac_address` filter to focus on specific transmitter

## Credits

Inspired by the [ESPectre project](https://github.com/francescopace/espectre) by Francesco Pace.

## See Also

- [ESPHome Binary Sensor](https://esphome.io/components/binary_sensor/)
- [ESPHome Sensor](https://esphome.io/components/sensor/)
- [ESP32 CSI Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi.html#wi-fi-channel-state-information)
