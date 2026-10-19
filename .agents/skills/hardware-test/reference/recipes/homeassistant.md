# homeassistant

## homeassistant light platform against a local Home Assistant (#11354, 2026-10-18)

Setup: a local Home Assistant dev install (`hass -c config`, https://127.0.0.1:8123 with a self-signed cert); its
ESPHome entries and entity ids are readable in `config/.storage/` (`core.config_entries`, `core.entity_registry`).
Two ESPHome RGBCW bulbs are devices in it with a plaintext
API (no key), so an "HA side" change is made directly on a bulb with `scripts/homeassistant_light/bulb.py BULB_IP -
on|off|state|restart`, independent of the mirror. Add the
test devices to HA by hand (shared `api_key`, "Allow the device to perform Home Assistant actions" on); an agent
could also do it with a long-lived token.

DUT template `templates/homeassistant-light-dut.yaml` (package: two mirrors, twelve buttons, a "Log state" button,
a 30 s alive line; logger VERBOSE with the noisy tags at DEBUG because a tag cannot exceed the global level).
Platform files just set the board and include it (one ESP32 ESP-IDF, one D1 mini). Flash by USB,
take the IP from the boot capture, start `esphome logs <yaml> --device IP` bounded captures, then:
`scripts/homeassistant_light/drive.py DEV_IP KEY press "Button"` presses the device's buttons and
`scripts/homeassistant_light/bulb.py BULB_IP - state|on|off|restart` reads or changes a bulb directly. The cases:
press each button and read the bulb and the device log; change a bulb directly and watch both mirrors follow;
restart the bulb, send a change, expect the 5 s `did not report the change back` and the fallback; OTA reboot the
device, press before HA resubscribes, watch traits and state arrive.

Lessons: the light core clamps mireds to the range HA reported; a flash sends nothing; a change sent while HA is
connected but not yet subscribed is dropped by the api server (`client has not subscribed to actions (yet)`), not by
the platform's `No clients` path; after a timeout fallback to an off report the mirror keeps the unsent brightness.
Results: https://github.com/esphome/esphome/pull/11354#issuecomment-6253630336

Retest of b19a776 (2026-10-18): `timeoutcase66.sh` and `fallback-on66.sh` (sed DEV/LOG for the ESP32) cover the
fallback with an off and an on last report; the on_boot `light.turn_on` added to the template shows the
"change before the first write" path over the serial console (OTA reboot with a serial capture running, the API log
client reconnects too late to see boot lines). A mirror that never saw its light on since boot has no reported
levels to restore. Results: https://github.com/esphome/esphome/pull/11354#issuecomment-6254161409
