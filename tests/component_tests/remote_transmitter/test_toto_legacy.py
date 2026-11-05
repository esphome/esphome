"""transmit_toto still takes the fields of the old most-significant-bit-first reading of code."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.remote_base import _toto_legacy_code
from tests.component_tests.remote_transmitter import protocol_call_bodies


def test_legacy_code() -> None:
    assert (
        _toto_legacy_code({"command": 0xEC, "rc_code_1": 0x0D, "rc_code_2": 0x0D})
        == 0x37BB
    )
    assert _toto_legacy_code({"command": 0x00}) == 0x0000
    assert _toto_legacy_code({"command": 0xB0}) == 0x0D00
    assert _toto_legacy_code({"command": 0x80, "rc_code_1": 0x8}) == 0x0101


def test_legacy_fields_set_code_bits(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    main_cpp = generate_main(component_config_path("toto_legacy.yaml"))
    code, legacy, command_only = protocol_call_bodies(main_cpp)
    assert "apply_call.data.code = 0x37BB;" in code
    assert "apply_call.data.code |= reverse_bits(uint8_t((0x0D) << 4));" in legacy
    assert "apply_call.data.code |= reverse_bits(uint8_t((0x0D) & 0x0F));" in legacy
    assert "apply_call.data.code |= reverse_bits(uint8_t(0xEC)) << 8;" in legacy
    assert "apply_call.data.code |= reverse_bits(uint8_t(0xB0)) << 8;" in command_only
    assert "rc_code" not in command_only
    assert "use 'code: 0x37BB'" in caplog.text
    assert "use 'code: 0x0D00'" in caplog.text
