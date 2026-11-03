"""bk72xx_ble no longer keeps WiFi power save off: LibreTiny 1.14.0 sleeps the radio
between DTIM beacons only, so the Beken MCU sleep that halted the chip with BLE running
is never armed (libretiny-eu/libretiny#414)."""

from collections.abc import Callable
from pathlib import Path


def test_power_save_mode_is_applied_with_ble(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_power_save.yaml"))

    assert "bk72xx_ble::BK72xxBLE" in main_cpp
    assert "set_power_save_mode(wifi::WIFI_POWER_SAVE_HIGH)" in main_cpp
