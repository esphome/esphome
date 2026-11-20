"""P2 callbacks are optional and use the standard callback automation helper."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_datagram_callbacks_have_typed_value_arguments(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("datagram.yaml"))
    assert any(define.name == "USE_VBUS_DATAGRAM" for define in CORE.defines)
    assert code.count("test_bus->add_on_datagram_callback(") == 2
    assert (
        "uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id, uint32_t value"
        in code
    )
    assert "TriggerForwarder<uint16_t, uint16_t, uint16_t, uint16_t, uint32_t>" in code


def test_legacy_rx_does_not_enable_datagram_storage(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("rx_only.yaml"))
    assert all(define.name != "USE_VBUS_DATAGRAM" for define in CORE.defines)
    assert "add_on_datagram_callback" not in code
