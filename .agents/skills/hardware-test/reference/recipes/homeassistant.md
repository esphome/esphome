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

## Held report versus a later call (#21144)

A report that arrives during a transition is held and applied when the transition ends, unless a call came after
it. `scripts/homeassistant_light/heldcase.py DEV_IP KEY BULB_IP same|many` keeps one API connection to the mirror
device and one to the bulb so the steps land inside a 3 s fade: press "A 50 fade 3s", 0.7 s later set the bulb to
90% directly (the report is held), 1.4 s in press the same fade again (or "A many calls", 16 calls in one loop
pass). Pass: the mirror and the bulb end at the second call's target; a build without the fix logs `Applying the
report that arrived during the transition` and the bulb stays at 90%. Separate button presses are too slow (about
1.5 s each) to land inside the fade. The mirrors are not API entities, so read their state from the "Log state"
button's line. Results: https://github.com/esphome/esphome/pull/21144#issuecomment-6278257695
