"""Tests for RP2040 component public helpers and variant detection."""

import pytest

from esphome.components.rp2040 import _detect_variant, board_id_has_wifi
from esphome.components.rp2040.const import VARIANT_RP2040, VARIANT_RP2350
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, CONF_VARIANT


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


def test_detect_variant_derives_variant_from_board() -> None:
    """Board alone resolves to the matching variant."""
    result = _detect_variant({CONF_BOARD: "rpipicow"})
    assert result[CONF_BOARD] == "rpipicow"
    assert result[CONF_VARIANT] == VARIANT_RP2040


def test_detect_variant_derives_variant_from_rp2350_board() -> None:
    """An RP2350 board resolves to ``RP2350``."""
    result = _detect_variant({CONF_BOARD: "rpipico2"})
    assert result[CONF_BOARD] == "rpipico2"
    assert result[CONF_VARIANT] == VARIANT_RP2350


def test_detect_variant_only_picks_default_board_rp2040() -> None:
    """Variant alone picks Pico W as the canonical RP2040 board."""
    result = _detect_variant({CONF_VARIANT: VARIANT_RP2040})
    assert result[CONF_BOARD] == "rpipicow"
    assert result[CONF_VARIANT] == VARIANT_RP2040


def test_detect_variant_only_picks_default_board_rp2350() -> None:
    """Variant alone picks Pico 2 W as the canonical RP2350 board."""
    result = _detect_variant({CONF_VARIANT: VARIANT_RP2350})
    assert result[CONF_BOARD] == "rpipico2w"
    assert result[CONF_VARIANT] == VARIANT_RP2350


def test_detect_variant_matching_explicit_variant_passes() -> None:
    """Specifying both a board and the matching variant is allowed."""
    result = _detect_variant({CONF_BOARD: "rpipico2", CONF_VARIANT: VARIANT_RP2350})
    assert result[CONF_BOARD] == "rpipico2"
    assert result[CONF_VARIANT] == VARIANT_RP2350


def test_detect_variant_mismatched_variant_raises() -> None:
    """Board/variant mismatch must be rejected and name the offending board."""
    with pytest.raises(
        cv.Invalid, match=r"does not match the selected board 'rpipicow'"
    ):
        _detect_variant({CONF_BOARD: "rpipicow", CONF_VARIANT: VARIANT_RP2350})


def test_detect_variant_unknown_board_without_variant_raises() -> None:
    """Unknown board with no variant tells the user how to recover."""
    with pytest.raises(cv.Invalid, match="please specify the chip variant"):
        _detect_variant({CONF_BOARD: "not-a-real-board"})


def test_detect_variant_unknown_board_with_variant_passes() -> None:
    """Unknown board + explicit variant is accepted (with a warning)."""
    result = _detect_variant(
        {CONF_BOARD: "not-a-real-board", CONF_VARIANT: VARIANT_RP2040}
    )
    assert result[CONF_BOARD] == "not-a-real-board"
    assert result[CONF_VARIANT] == VARIANT_RP2040
