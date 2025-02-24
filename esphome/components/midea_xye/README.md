# Overview

This component communicates with Midea-like air conditioners (heat pumps) via the XYE protocol over RS485.
Kudos to these projects:
- Reverse engineering of the protocol: https://codeberg.org/xye/xye
- Working implementation using ESP32: https://github.com/Bunicutz/ESP32_Midea_RS485
- Fully integrated Midea Climate component: https://github.com/esphome/esphome/tree/dev/esphome/components/midea

To get this to work you need an esphome configuration like this:

```yaml
esphome:
  name: heatpump
  friendly_name: Heatpump

esp8266:  #also works with esp32
  board: d1_mini

# Enable logging (but not via UART)
logger:
  baud_rate: 0

external_components:
  - source: 
      type: git
      url: https://github.com/exciton/esphome
      ref: dev
    components: [midea_xye]
  
# UART settings for RS485 covnerter dongle (required)
uart:
  tx_pin: TX
  rx_pin: RX
  baud_rate: 4800
  debug: #If you want to help reverse engineer
    direction: BOTH


# Main settings
climate:
  - platform: midea_xye
    name: Heatpump
    period: 1s                  # Optional. Defaults to 1s
    timeout: 100ms              # Optional. Defaults to 100ms
    #beeper: true               # Optional. Beep on commands.
    visual:                     # Optional. Example of visual settings override.
      min_temperature: 17 °C    # min: 17
      max_temperature: 30 °C    # max: 30
      temperature_step: 1.0 °C  # min: 0.5
    supported_modes:            # Optional. 
      - FAN_ONLY
      - HEAT_COOL              
      - COOL
      - HEAT
      - DRY
    custom_fan_modes:           # Optional
      - SILENT
      - TURBO
    supported_presets:          # Optional. 
      - BOOST
      - SLEEP
    supported_swing_modes:      # Optional
      - VERTICAL
    outdoor_temperature:        # Optional. Outdoor temperature sensor
      name: Outside Temp
    temperature_2a:             # Optional. Inside coil temperature
      name: Inside Coil Temp
    temperature_2b:             # Optional. Outside coil temperature
      name: Outside Coil Temp
    current:                    # Optional. Current measurement
      name: Current
    timer_start:                # Optional. On timer duration
      name: Timer Start
    timer_stop:                 # Optional. Off timer duration
      name: Timer Stop
    error_flags:                # Optional.
      name: Error Flags
    protect_flags:              # Optional. 
      name: Protect Flags

```

# What works
- Setting mode (off, fan, cool, heat, dry). Auto mode doesn't work for me
- Setting temperature. Can send in C or F.
- Setting fan mode (auto, low, med, high).
- Reading inside, outside air temperatures, and inside coil temperature.
- Reading timer start/stop times (set by remote)

# What doesn't work
- Auto mode always defaults to heat (regardless of temperature)
- Outside coil temperature doesn't work for me (shows 0)
- Current reading always shows 255
- Reading back fan mode when in Auto shows the acutal mode (i.e. no way to check it's in auto)
- Setting turbo (aux heat), sleep doesn't work (although reading it back from the unit does seem to)
- Setting swing mode 

# Not yet implemented
- Setting timers direct to unit. No real need since automations can do this 
- Need to check calibration calculation for temperature
- Figure out how to force display to C or F. Setting temp in C doesn't force display to C.
- Freeze protection
- Silent mode
- Lock/Unlock

# Not tested
- IR integration

