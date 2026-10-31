# CAN (esp32_can) without a transceiver (#21792, #21795)

`scripts/twai_self`: `esp32_can` with `mode: NO_ACK`, TX on GPIO25 and RX on GPIO26. After boot `canself.hpp` enables
the input on GPIO25 and routes it to the controller's receive signal (`esp_rom_gpio_connect_in_signal(25,
TWAI_RX_IDX, false)`), so the controller hears its own transmit pad. Frames go out with `twai_transmit_v2` and
`TWAI_MSG_FLAG_SELF` (the driver handle is reached through explicit template instantiation), so every frame runs
through the normal receive path and `on_frame`.

- `can.yaml` has nine `on_frame` entries (standard and extended ids, masks, remote-only and data-only filters, two
  entries with the same filter, an id with bits outside its mask that must never fire) and sends 20 rounds of 7
  frames. `run.sh LABEL` writes `out-can<LABEL>.counts` (fires per trigger, id, flags and data); diff dev and the PR.
- Set `rx_queue_len: 32`: with the default 5, a burst of 7 frames loses the last two most of the time, on every
  build.
- ESPHome's own `canbus.send` does not set the self flag, so its frames are not received in this setup.
- Results: https://github.com/esphome/esphome/pull/21795#issuecomment-6491777381
