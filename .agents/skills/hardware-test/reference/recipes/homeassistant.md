# homeassistant

## homeassistant light platform against a local Home Assistant (#11354)

A local Home Assistant dev install (`hass -c config`, https://127.0.0.1:8123 with a self-signed cert); its ESPHome
entries and entity ids are in `config/.storage/`. Two ESPHome RGBCW bulbs with a plaintext API are the lights, so an
"HA side" change is made directly on a bulb with `scripts/homeassistant_light/bulb.py BULB_IP - on|off|state|restart`.
Add the test devices to HA by hand with the shared `api_key` and "Allow the device to perform Home Assistant actions".

`templates/homeassistant-light-dut.yaml` is a package: two mirrors, buttons for each change, a "Log state" button, a
30 s alive line, an `on_boot` `light.turn_on` for the change-before-first-write path, and VERBOSE logging with the
noisy tags at DEBUG (a tag cannot exceed the global level). A platform file sets the board and includes it:

```yaml
esphome:
  name: ha-light-dut
esp32:
  board: esp32dev
  framework:
    type: esp-idf
packages:
  dut: !include homeassistant-light-dut.yaml
```

Flash by USB, start a bounded `esphome logs` capture per device, then press buttons with
`scripts/api_button.py DEV_IP KEY press "Button"` and read the bulb with `bulb.py`. Cases: each
button against the bulb and the device log; a direct bulb change that both mirrors follow; restart the bulb, send a
change, expect the 5 s `did not report the change back` and the fallback (with an off and with an on last report);
OTA reboot with a serial capture running to see the boot-time change (the API log client reconnects too late).

Lessons: the light core clamps mireds to the range HA reported; a flash sends nothing; a change sent while HA is
connected but not yet subscribed is dropped by the api server (`client has not subscribed to actions (yet)`); after a
fallback to an off report the mirror keeps the unsent brightness; a mirror that never saw its light on since boot has
no levels to restore. Results: https://github.com/esphome/esphome/pull/11354#issuecomment-6253630336 and
https://github.com/esphome/esphome/pull/11354#issuecomment-6254161409
