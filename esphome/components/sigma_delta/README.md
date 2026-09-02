# Sigma Delta Speaker

See `speaker` platform `sigma_delta` - ESP32 sigma-delta modulation speaker via GPIO + RC filter.

Full documentation draft: `docs/sigma_delta.rst` (to be submitted to esphome/esphome-docs).

Wiring: `GPIO --[1kΩ]--+--> amp/speaker, [10nF]->GND (fc~16kHz)`

Example:

```yaml
speaker:
  - platform: sigma_delta
    id: sdm_speaker
    pin: GPIO1
    sample_rate: 44100
```
