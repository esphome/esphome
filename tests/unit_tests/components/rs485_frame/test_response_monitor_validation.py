"""Tests for rs485_frame response_monitor: button_id: cross-validation.

A code review of the response_monitor feature found that
_final_validate_response_monitor (esphome/components/rs485_frame/__init__.py) can raise a
raw, unhandled KeyError instead of a clean cv.Invalid in two cases -- both would surface to
a user as a Python traceback instead of a normal "Failed config" message, since
Config.catch_error() only catches vol.Invalid (esphome/config.py's ConfigValidationStep.run).
"""

from __future__ import annotations

from pathlib import Path

import voluptuous as vol

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def test_button_id_referencing_a_non_button_entity_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """button_id: pointing at a sensor (not a button) on the same hub must be rejected
    cleanly, not crash _resolve_button_trigger indexing keys a sensor config lacks."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_response_monitor_non_button_id.yaml"
    )

    try:
        result = esphome_config.validate_config(raw_config, {})
    except KeyError as err:  # pragma: no cover -- documents the bug when it's present
        raise AssertionError(
            "response_monitor button_id: validation raised an unhandled KeyError "
            f"({err!r}) instead of a clean cv.Invalid for a non-button target"
        ) from err

    assert result.errors, "expected a validation error for a non-button button_id:"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("not_a_button" in str(err) for err in result.errors)


def test_button_id_with_no_command_format_anywhere_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """A button_id: trigger whose button uses `value:` with no command_format on the
    button or the hub must be rejected cleanly, not crash on config[CONF_COMMAND_FORMAT]
    (the hub's own key) before the button platform's own friendlier check ever runs."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_response_monitor_missing_command_format.yaml"
    )

    try:
        result = esphome_config.validate_config(raw_config, {})
    except KeyError as err:  # pragma: no cover -- documents the bug when it's present
        raise AssertionError(
            "response_monitor button_id: validation raised an unhandled KeyError "
            f"({err!r}) instead of a clean cv.Invalid when the hub has no command_format:"
        ) from err

    assert result.errors, "expected a validation error for a missing command_format:"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("command_format" in str(err) for err in result.errors)
