# GPIO wakeup from auto light sleep (#21074)

`scripts/light_sleep_wake` needs no wiring. LEDC on `LEDC_USE_RC_FAST_CLK` with
`sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE` keeps toggling GPIO10 at 2 Hz through light sleep, and an
`ESP32InternalGPIOPin` interrupt (`ANY_EDGE`) on the same pad timestamps every edge in an IRAM handler.
`wakeprobe::report()` logs every 30 s: edges seen and expected, the shortest and longest gap, how many edges came
more than 20 ms off the half period, and the SLEEP share from `esp_pm_dump_locks()` (needs `profiling: true`).

- Native ESP-IDF builds need `include_builtin_idf_components: [esp_driver_ledc]`.
- After `ledc_channel_config` call `gpio_input_enable(pin)`, never `gpio_set_direction(INPUT_OUTPUT)`: that routes the
  pad back to a plain GPIO output, the wave stops and the pad reads a constant 1.
- The C3 shares one LEDC clock across timers, so a board whose own config drives LEDs through `ledc` needs a test
  config without them. Logging over the API works with `logger: baud_rate: 0`.
- Read the reports over the API with `esphome logs ... --device <ip>` under `timeout`. Run PR, dev, then the PR
  again. On a C3 bulb dev caught every edge but handled 8 of 845 more than 20 ms late
  (up to 45 ms), the PR none; both slept 57 to 58% of the time.
