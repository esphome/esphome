# Addressable LED strips (#21447, #21445)

- To exercise a failed strip allocation, use a classic ESP32 without PSRAM with `num_leds: 65535` and
  `channel_colors: GRBW` (262 kB, bigger than any internal block). `num_leds` is uint16 in the drivers, larger
  values wrap. On dev such a strip boot-loops the board.
- Never force allocation failures by hogging RAM instead: Wi-Fi then aborts in `operator new` or the loop task
  trips the task watchdog.
- On the ESP32-S3 give each RMT strip `rmt_symbols: 48`, or the first channel takes all 192 symbols.
- A board with a few WS2812 on a pin already defined as a light can be extended with `id: !extend <light id>` to
  add effects; restore its own config afterwards.
