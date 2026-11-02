# ESP32 GPIO hold through the ISR pin API (#21576)

`scripts/gpio_hold/hold_test.hpp` builds `ESP32InternalGPIOPin`s by hand, calls `to_isr()` and reads each pad back
with `gpio_get_level`: 20 ISR writes, a raw `gpio_set_level` control (a held pad must not change, an unheld one must)
and ISR `pin_mode` pulls (input must drop to `NONE` between pull changes, because the ISR path only applies pulls when
`FLAG_INPUT` toggles). It needs no wiring: unconnected pins read the pulls. Each pin runs held and unheld, then the
round logs `ROUND n PASS failures=0` or `FAIL`.

- `USE_GPIO_HOLD` comes from any pin with `hold_state: true`, so the configs add a `gpio` output on a spare pin.
- `gpio-hold-esp32.yaml` (classic devkit) runs three rounds 5 s after boot and has a `rerun` button;
  `gpio-hold-esp32s3.yaml` (Waveshare ESP32-S3-ETH, where most pins go to the W5500, TF card, camera, WS2812 or
  USB) runs a round every 15 s.
- C3: ESP-IDF 5.5 has `SOC_RTCIO_PIN_COUNT` 0, so `rtc_gpio_is_valid_gpio()` is false on every pin and only the
  digital hold path runs.
- C6 (Waveshare ESP32-C6-GEEK, native USB): LP pads GPIO0 to 7 are RTC IO with hold. GPIO1, 2, 7, 20, 23 read high
  and GPIO6 low whatever the pull (LCD, TF card or resistors); clean pins are 0, 3, 4, 5, 18, 19, 21, 22. Dev
  already holds LP pads through `gpio_hal_hold_en`, so the PR and dev look the same there. H2 is untested.
