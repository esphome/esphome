"""Tests for RP2040 component public helpers."""

from esphome.components.rp2040 import board_id_has_wifi


def test_board_id_has_wifi_for_known_wifi_board() -> None:
    """``rpipicow`` is the canonical Pico W → True."""
    assert board_id_has_wifi("rpipicow") is True


def test_board_id_has_wifi_for_known_non_wifi_board() -> None:
    """Plain ``rpipico`` has no CYW43 → False."""
    assert board_id_has_wifi("rpipico") is False


def test_board_id_has_wifi_for_rp2350_w_variant() -> None:
    """``rpipico2w`` is the RP2350 Pico 2 W → True."""
    assert board_id_has_wifi("rpipico2w") is True


def test_board_id_has_wifi_for_unknown_board_returns_true() -> None:
    """Unknown ids fail open so a custom board is not rejected.

    The validator falls back to ESPHome's compile-time check; the
    helper returning True here means the wizard emits a ``wifi:``
    block and any genuinely-unsupported config trips the existing
    "no CYW43" guard at compile time.
    """
    assert board_id_has_wifi("not-a-real-board-id") is True
