"""Tests for rs485_frame on_frame: frame_type validation.

A code review of the upstream PR found that frame_type: accepts a list of alternate
prefixes (e.g. `[[0x01, 0x03], [0x01, 0x09]]`) so one lambda can decode multiple related
frame types, but an *inner* prefix that is itself empty (`[[0x01, 0x03], []]`) silently
becomes match-all for that alternate -- every frame matches, not just frames starting
with 0x01 0x03. This is different from the documented, intentional `frame_type: []`
top-level form (bare, not inside a list of alternates), which is match-all by design.
"""

from __future__ import annotations

from pathlib import Path

import voluptuous as vol

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def test_on_frame_empty_alt_prefix_raises_clean_invalid(fixture_path: Path) -> None:
    """frame_type: [[0x01, 0x03], []] must be rejected -- the second, empty alternate
    would silently match every frame instead of doing nothing."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_on_frame_empty_alt_prefix.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, "expected a validation error for an empty alternate prefix"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
