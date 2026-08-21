"""Tests for rs485_frame response_monitor: trigger cross-validation (button_id: and
button_name:).

A code review of the response_monitor feature found that
_final_validate_response_monitor (esphome/components/rs485_frame/__init__.py) can raise a
raw, unhandled KeyError instead of a clean cv.Invalid in two cases -- both would surface to
a user as a Python traceback instead of a normal "Failed config" message, since
Config.catch_error() only catches vol.Invalid (esphome/config.py's ConfigValidationStep.run).

button_name: was added later as an alternative to button_id: that resolves a button by its
name: instead of its id:, so a response_monitor entry can reference a button without that
button needing an explicit id: added to it.
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


def test_button_name_resolves_the_matching_button_on_the_hub(
    fixture_path: Path,
) -> None:
    """button_name: is an alternative to button_id: that resolves a button by its
    already-required, already-unique name: instead of needing an explicit id: added
    to it -- avoids editing existing button definitions just to reference them."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_response_monitor_button_name_ok.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, f"expected no validation errors, got: {result.errors}"


def test_button_name_matching_no_button_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """button_name: that matches no button on the hub must be rejected cleanly,
    not silently resolve to an empty trigger."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_response_monitor_button_name_zero_matches.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, "expected a validation error for a button_name: with no match"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("matches 0 buttons" in str(err) for err in result.errors)


def test_button_id_and_button_name_together_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """button_id: and button_name: are mutually exclusive trigger forms; specifying
    both must be rejected at schema validation."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_response_monitor_button_id_and_name.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, "expected a validation error for button_id: + button_name:"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("Cannot specify more than one of" in str(err) for err in result.errors)


def test_signature_mask_and_bit_together_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """A signature alt's mask: and bit: are two representations of the same value
    (bit: N is shorthand for mask: 0x<1<<N>) and must be mutually exclusive."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_response_monitor_mask_and_bit.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, "expected a validation error for mask: + bit:"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("Cannot specify more than one of" in str(err) for err in result.errors)
