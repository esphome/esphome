# captive_portal

## Captive portal `/wifisave` hammer (#20802, #20784)

Reproduces a `/wifisave` arriving during the STA cooldown, and proves a fix on every board.
Device under test: `templates/example-dut.yaml` (bogus network, `ap:` with `ap_timeout: 15s`,
`captive_portal:`). Client: `templates/example-client.yaml` on an ESP32 or a D1 mini; it joins the
device's AP, posts 15 bogus saves 3 s apart, then the real credentials. With the portal up the cooldown
is 30 s, so nearly every save lands inside one. Driver: `scripts/wifisave_hammer.sh`. Settle 40 s on
ESP boards, 90 s on the BW15 (its first attempt on the real network fails, see boards.md).

Counts that matter: `Requested WiFi Settings Change` (saves received), `Exiting cooldown early`, crash
markers, boots (`Log initialized`), and the client's `bogus save status 200` and `real save status 200`.
A healthy run: every save answered 200, one cooldown exit per save (two on the BW15, whose retry enters
the adapter restart), no panic, and the real credentials connect the device within about 10 s.

Fault injection for AP start failures: wrap `this->ap_setup_ = this->wifi_start_ap_(this->ap_)` in
`setup_ap_config_()` with an `HWTEST_AP_FAIL_COUNT` build flag
(`esphome: platformio_options: build_flags: -DHWTEST_AP_FAIL_COUNT=2`), raise the portal's "Captive
portal started" from LOGV to LOGI, and after provisioning curl the device's STA address: connection
refused proves the portal's server is gone.
