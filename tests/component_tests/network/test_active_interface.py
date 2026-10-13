"""mDNS follows the interface holding the default route through an
ActiveInterfaceListener, only where NetworkComponent tracks that interface
(ESP32 with more than one interface) and the network TXT record exists."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

LISTENER_DEFINES = {
    "USE_MDNS_ACTIVE_INTERFACE_LISTENER",
    "NETWORK_ACTIVE_INTERFACE_LISTENER_COUNT",
}


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_mdns_registers_on_esp32_with_wifi_ethernet_and_api(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("priority_ethernet_first_api.yaml"))

    assert "add_active_interface_listener(" in main_cpp
    assert _defines() >= LISTENER_DEFINES


@pytest.mark.parametrize(
    "fixture",
    [
        # No api means no _esphomelib service, so no network TXT record
        pytest.param("priority_ethernet_first.yaml", id="esp32_without_api"),
        # No route arbitration on RP2; mDNS keeps its periodic check there
        pytest.param("priority_rp2040_api.yaml", id="rp2040"),
    ],
)
def test_mdns_does_not_register(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    fixture: str,
) -> None:
    main_cpp = generate_main(component_config_path(fixture))

    assert "add_active_interface_listener(" not in main_cpp
    assert not LISTENER_DEFINES & _defines()
