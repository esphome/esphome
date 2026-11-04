"""Tests for rs485_frame on_frame validation.

frame_type: a code review of the upstream PR found that frame_type: accepts a list of
alternate prefixes (e.g. `[[0x01, 0x03], [0x01, 0x09]]`) so one lambda can decode multiple
related frame types, but an *inner* prefix that is itself empty (`[[0x01, 0x03], []]`)
silently becomes match-all for that alternate -- every frame matches, not just frames
starting with 0x01 0x03. This is different from the documented, intentional
`frame_type: []` top-level form (bare, not inside a list of alternates), which is
match-all by design.

Deferring actions: payload is a view into the hub's receive buffer, which the next frame
overwrites, so on_frame must not delay or wait. Copying payload into a script parameter
first is the supported way to defer.
"""

from __future__ import annotations

from pathlib import Path

import voluptuous as vol

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def _validate_fixture(fixture_path: Path, name: str) -> esphome_config.Config:
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(fixture_path / name)
    return esphome_config.validate_config(raw_config, {})


def test_on_frame_empty_alt_prefix_raises_clean_invalid(fixture_path: Path) -> None:
    """frame_type: [[0x01, 0x03], []] must be rejected -- the second, empty alternate
    would silently match every frame instead of doing nothing."""
    result = _validate_fixture(
        fixture_path, "rs485_frame_on_frame_empty_alt_prefix.yaml"
    )

    assert result.errors, "expected a validation error for an empty alternate prefix"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)


def test_on_frame_rejects_deferring_actions(fixture_path: Path) -> None:
    """Payload is a view into the hub's receive buffer, which the next frame overwrites.
    A delay (or any other deferring action) would leave the rest of the automation reading
    a later frame, so on_frame must be synchronous."""
    result = _validate_fixture(
        fixture_path, "rs485_frame_on_frame_deferring_action.yaml"
    )

    assert result.errors, "expected a validation error for delay: in on_frame"
    assert any("Deferring actions" in str(err) for err in result.errors), [
        str(err) for err in result.errors
    ]


def test_on_frame_allows_copy_into_script(fixture_path: Path) -> None:
    """Copying payload into a uint8_t[] script parameter is the supported way to defer:
    script.execute itself is synchronous, and the script owns its copy."""
    result = _validate_fixture(fixture_path, "rs485_frame_on_frame_script_copy.yaml")

    assert not result.errors, [str(err) for err in result.errors]
