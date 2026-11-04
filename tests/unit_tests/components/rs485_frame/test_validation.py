"""Tests for rs485_frame config validation that only a whole config can trigger.

A button or constant send_frame whose frame is shorter than rs485_frame accepts on receive, or
longer than the hub's max_frame_length, would only be dropped at run time, so it is rejected at
validation, as is a TX gate prefix longer than max_frame_length, which no frame can match. An
empty alternate on_frame prefix or a bare `frame_type:` would match every frame, so both are
rejected too; the documented top-level `frame_type: []` matches every frame. Every hub needs
the RX pin of its UART to itself, and a hub that transmits needs the TX pin too.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


@pytest.mark.parametrize(
    ("fixture", "expected"),
    [
        ("rs485_frame_button_raw_exceeds_max_frame_length.yaml", "max_frame_length"),
        ("rs485_frame_button_value_exceeds_max_frame_length.yaml", "max_frame_length"),
        ("rs485_frame_button_frame_too_short.yaml", "at least 2 bytes"),
        ("rs485_frame_button_value_too_wide.yaml", "does not fit in 1 byte"),
        ("rs485_frame_send_frame_too_short.yaml", "at least 2 bytes"),
        ("rs485_frame_send_frame_exceeds_max_frame_length.yaml", "5-byte frame"),
        (
            "rs485_frame_gate_frame_type_exceeds_max_frame_length.yaml",
            "longer than max_frame_length",
        ),
        ("rs485_frame_on_frame_empty_alt_prefix.yaml", "length of value must be"),
        ("rs485_frame_on_frame_null_frame_type.yaml", "frame_type needs a value"),
        ("rs485_frame_uart_without_tx.yaml", "to declare a tx_pin"),
        ("rs485_frame_uart_without_rx.yaml", "to declare a rx_pin"),
        (
            "rs485_frame_uart_shared_rx.yaml",
            "rx_pin is used both by rs485_frame sniffer_b and rs485_frame sniffer_a",
        ),
    ],
)
def test_invalid_config_raises(fixture_path: Path, fixture: str, expected: str) -> None:
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(fixture_path / fixture)

    result = esphome_config.validate_config(raw_config, {})

    assert any(expected in str(err) for err in result.errors), [
        str(err) for err in result.errors
    ]


def test_sniffer_accepts_rx_only_uart(fixture_path: Path) -> None:
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_sniffer_rx_only_uart.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, [str(err) for err in result.errors]
