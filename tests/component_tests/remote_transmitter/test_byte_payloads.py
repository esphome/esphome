"""The byte payload transmit actions as generated code."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.remote_transmitter import protocol_call_bodies


def test_byte_payloads(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("byte_payloads.yaml"))
    abb, aeha, midea, pioneer_wyt = protocol_call_bodies(main_cpp)
    # The address length goes first, a configured id turns the automatic one off
    assert abb.index("set_three_byte_address(true);") < abb.index("set_source_address(")
    assert abb.index("auto_message_id = true;") < abb.index(
        "set_fixed_message_id(0x42);"
    )
    assert abb.rstrip().endswith("apply_call.data.finalize();")
    # The carrier is set before the payload is encoded
    assert aeha.index("set_carrier_frequency(36700);") < aeha.index(
        "apply_call.encode("
    )
    assert (
        "apply_call.encode(static_cast<uint16_t>(0x8008), []() -> std::vector<uint8_t> {"
        in aeha
    )
    assert (
        "apply_call.data = remote_base::MideaData([]() -> std::vector<uint8_t> {"
        in midea
    )
    assert midea.rstrip().endswith("apply_call.data.finalize();")
    # A 13-byte code is copied into the frame and the checksum is appended by finalize()
    assert (
        "apply_call.data = remote_base::PioneerWytData([]() -> std::vector<uint8_t> {"
        in pioneer_wyt
    )
    assert pioneer_wyt.rstrip().endswith("apply_call.data.finalize();")
