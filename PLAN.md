# Allow `wifi:` and `ethernet:` to coexist with boot-time link detection

## Context

A user wants a single YAML config that:
- At boot, checks whether the Ethernet cable is plugged in.
- If the cable is plugged in, brings up Ethernet only.
- If the cable is unplugged, brings up WiFi (with `improv_serial` / `esp32_improv` provisioning fallback).
- Restarts the device whenever the cable plug state changes.

This is impossible on `dev` today. Two specific things in the codebase prevent it:

1. `esphome/components/ethernet/__init__.py:52` declares `CONFLICTS_WITH = ["wifi"]`, so config validation rejects any YAML that has both.
2. `esphome/components/ethernet/__init__.py:581-583` unconditionally turns off the ESP-IDF WiFi stack (`CONFIG_ESP_WIFI_ENABLED = False`, `CONFIG_SW_COEXIST_ENABLE = False`) when ethernet is configured, so even if the Python conflict were removed, the WiFi driver would not be linked into the firmware.

ESP-IDF itself supports running WiFi and Ethernet simultaneously — they share `esp_netif` / lwIP and routing is decided per netif. ESPHome opted out of dual-stack early to save flash/RAM, but the user's use case (one or the other selected at boot) is reasonable and worth supporting.

The plan removes those two blocks and adds the minimum scaffolding needed to make a "select interface at boot based on PHY link" YAML work.

## Approach

Five focused changes in ESPHome, then the user's YAML works directly.

### 1. Drop the `CONFLICTS_WITH` and make WiFi sdkconfig conditional

File: `esphome/components/ethernet/__init__.py`

- Line 52: remove `CONFLICTS_WITH = ["wifi"]`.
- Lines 580-583: only force `CONFIG_ESP_WIFI_ENABLED=False` / `CONFIG_SW_COEXIST_ENABLE=False` when the wifi component is **not** in the config. Use `CORE.config` (or a `final_validate` hook) to check.
  ```python
  if "wifi" not in CORE.config:
      add_idf_sdkconfig_option("CONFIG_ESP_WIFI_ENABLED", False)
      add_idf_sdkconfig_option("CONFIG_SW_COEXIST_ENABLE", False)
  else:
      add_idf_sdkconfig_option("CONFIG_SW_COEXIST_ENABLE", True)
  ```
  When both are enabled, `CONFIG_SW_COEXIST_ENABLE` is harmless (it gates BT/WiFi sharing) but does cost a few KB; keep an eye on flash budgets and add a warning in `dump_config` if both coexist.

### 2. Add `enable_on_boot` to the ethernet schema

File: `esphome/components/ethernet/__init__.py` and `ethernet_component.h` / `.cpp`

Mirror the existing wifi pattern:
- Schema (`__init__.py` around the rest of `CONFIG_SCHEMA`): `cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean`.
- C++ (`ethernet_component.h:117+`): add `bool enable_on_boot_{true};` field and `set_enable_on_boot(bool)` setter — see the wifi equivalent at `wifi_component.h:536` and `wifi_component.h:891`.
- `setup()` in `ethernet_component_esp32.cpp` should still install the driver (so the PHY can be probed) but should leave `started_ = false` if `enable_on_boot_` is false. The state machine at `ethernet_component_esp32.cpp:81-131` already gates everything off `started_`, so this is a small change.

### 3. Add `ethernet.enable` / `ethernet.disable` actions and `ethernet.connected` / `ethernet.has_link` conditions

New file: `esphome/components/ethernet/automation.h` (model after `esphome/components/wifi/automation.h:26-78`).

- `EthernetEnableAction`: sets `started_ = true`, kicks the loop.
- `EthernetDisableAction`: calls existing `powerdown()` (`ethernet_component.h:144`, already implemented at `ethernet_component_esp32.cpp:812-819`) and sets `started_ = false`.
- `EthernetConnectedCondition`: returns `is_connected()` (already exists at `ethernet_component.h:125`).
- `EthernetHasLinkCondition`: returns the PHY link bit. ESP-IDF exposes this via `esp_eth_ioctl(eth_handle_, ETH_CMD_G_PHY_ADDR, ...)` plus a PHY register read of `MII_BMSR` bit 2; the existing code already does PHY reads (`ethernet_component_esp32.cpp:837,857` for KSZ80XX). Wrap that into a `bool has_link()` method on `EthernetComponent` and have the condition call it.

Register them in `__init__.py` next to the existing trigger registration:
```python
@automation.register_action("ethernet.enable", EthernetEnableAction, ...)
@automation.register_action("ethernet.disable", EthernetDisableAction, ...)
@automation.register_condition("ethernet.connected", EthernetConnectedCondition, ...)
@automation.register_condition("ethernet.has_link", EthernetHasLinkCondition, ...)
```

### 4. Make the ethernet driver install (PHY probe) cheap and fast

File: `esphome/components/ethernet/ethernet_component_esp32.cpp`

`setup()` already does `esp_eth_driver_install` regardless of state. The new `has_link()` method must work right after `setup()` finishes, before `esp_eth_start()`. ESP-IDF reads PHY registers as soon as the driver is installed and `phy->reset()` runs, so this is already true — just expose it. No new init flow needed; the only requirement is that the user's `on_boot` priority runs **after** ethernet `setup_priority`. Ethernet's `get_setup_priority()` returns `setup_priority::WIFI` (verify in `ethernet_component_esp32.cpp`); user's `on_boot` should use `priority: 250` (after WiFi/Ethernet setup, before connection).

### 5. Document the expected boot sequence

Update `esphome/components/ethernet/__init__.py` docstring / `dump_config()` to spell out:
- When both `wifi:` and `ethernet:` are present, the user owns the choice via `enable_on_boot: false` + an `on_boot` automation. ESPHome will not auto-select.
- Flash overhead is roughly +N KB (measure during PR; document in PR description per CLAUDE.md breaking-change guidance).

## Resulting YAML config

Once the changes above land, this YAML works:

```yaml
esphome:
  name: dual-net
  on_boot:
    # Run after both ethernet and wifi setup() but before either is told to connect.
    priority: 250
    then:
      - if:
          condition:
            ethernet.has_link:
          then:
            - logger.log: "Ethernet cable detected, using Ethernet"
            - ethernet.enable:
          else:
            - logger.log: "No Ethernet link, using WiFi"
            - wifi.enable:

esp32:
  board: esp32dev
  framework:
    type: esp-idf

ethernet:
  type: LAN8720
  mdc_pin: GPIO23
  mdio_pin: GPIO18
  clk_mode: GPIO0_IN
  phy_addr: 0
  enable_on_boot: false   # NEW — added in change #2
  on_disconnect:
    then:
      - logger.log: "Ethernet disconnected, restarting"
      - delay: 2s
      - button.press: restart_btn

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  enable_on_boot: false   # already exists today
  ap:                     # AP fallback for captive_portal / improv
    ssid: "Dual-Net Fallback"

captive_portal:
improv_serial:

# esp32_improv is optional — only if you have a button and want BLE provisioning
# esp32_improv:
#   authorizer: !push_button gpio0

button:
  - platform: restart
    id: restart_btn
    name: "Restart"

# Detect cable insertion when running on WiFi: poll PHY link, restart if it changes.
interval:
  - interval: 5s
    then:
      - if:
          condition:
            and:
              - not: ethernet.connected
              - ethernet.has_link
          then:
            - logger.log: "Ethernet cable plugged in, restarting"
            - button.press: restart_btn
```

Notes on the YAML:
- `ethernet.has_link` is the new cheap PHY-only check (change #3); it works even when ethernet is disabled because the driver was installed in `setup()`.
- `ethernet.enable` / `wifi.enable` are mutually exclusive in this YAML — only one path runs at boot.
- The `interval` block handles "cable plugged in while running on WiFi" → restart so ethernet takes over on the next boot. The existing `on_disconnect` trigger handles the inverse (cable unplugged while on ethernet → restart).
- `improv_serial` works as long as wifi is enabled (it depends on wifi per `improv_serial/__init__.py:10`).

## Critical files to modify

- `esphome/components/ethernet/__init__.py` — drop `CONFLICTS_WITH` (line 52), make sdkconfig conditional (lines 580-583), add `enable_on_boot` schema option, register new actions/conditions.
- `esphome/components/ethernet/ethernet_component.h` — add `enable_on_boot_` field, `set_enable_on_boot`, `has_link()` method.
- `esphome/components/ethernet/ethernet_component_esp32.cpp` — implement `has_link()` via existing PHY read path (model after KSZ80XX read at lines 837/857), gate `started_` initialization on `enable_on_boot_`.
- `esphome/components/ethernet/ethernet_component_rp2040.cpp` — implement `has_link()` using the existing `linkStatus()` Arduino API (already used at line 117 in that file).
- `esphome/components/ethernet/automation.h` — new file, model after `esphome/components/wifi/automation.h`.
- `tests/components/ethernet/` — add `common.yaml` test that combines `wifi:` + `ethernet:` and uses the new actions/conditions.

## Reused existing infrastructure

- WiFi already has `enable_on_boot` (`wifi_component.h:536, 891`), `wifi.enable` / `wifi.disable` actions (`wifi/automation.h:26, 31`), and `wifi.connected` / `wifi.enabled` / `wifi.ap_active` conditions.
- Ethernet already has `is_connected()` (`ethernet_component.h:125`), `powerdown()` (`ethernet_component.h:144`), `on_connect` / `on_disconnect` triggers (`ethernet_component.h:185-190`).
- `improv_serial` (`esphome/components/improv_serial/__init__.py`) and `captive_portal` (`esphome/components/captive_portal/__init__.py`) need no changes; they just need wifi to be enabled at boot.
- Restart action: `button.press` on a `platform: restart` button (used in the YAML above) — no need for a new action; this matches existing ESPHome patterns.

## Verification

1. Config validation: `esphome config dual-net.yaml` should pass (today it errors with `Component conflicts with wifi`).
2. Compile both paths: `esphome compile` with cable connected and disconnected — verify only one stack negotiates an IP.
3. Hotplug: boot with cable, then unplug → device restarts within ~5s, comes up on WiFi. Boot without cable, then plug in → device restarts within ~5s, comes up on Ethernet.
4. Improv fallback: boot without cable and with bad WiFi credentials → `improv_serial` should accept new credentials over USB.
5. Run `pytest tests/component_tests/ethernet` and the YAML compile-test under `tests/components/ethernet/test.esp32-idf.yaml` (add a new `dual_net` variant).
6. Flash size sanity check: compare `.bin` size against `dev` for an ethernet-only config to confirm the conditional sdkconfig change doesn't regress single-stack builds.
7. Memory check at runtime: confirm with `top` / `esp_get_free_heap_size()` that boot-time selection actually leaves the unused stack idle (WiFi shouldn't allocate buffers if `wifi.enable` is never called).
