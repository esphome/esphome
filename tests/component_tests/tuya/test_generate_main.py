"""End-to-end test of the base-class split through the config pipeline."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.tuya import FILTER_SOURCE_FILES
from esphome.core import CORE


def test_non_uart_transport_loads_base_only(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("fake_transport.yaml"))

    # The fake transport and the tuya switch resolve through the base class
    assert "fake_tuya_transport::FakeTuyaTransport" in main_cpp
    assert "tuya::TuyaSwitch" in main_cpp

    # The UART transport is neither loaded nor instantiated...
    assert "uart" not in CORE.loaded_integrations
    assert "TuyaUART" not in main_cpp

    # ...and its sources are excluded from the build
    assert sorted(FILTER_SOURCE_FILES()) == ["tuya_uart.cpp", "tuya_uart.h"]
