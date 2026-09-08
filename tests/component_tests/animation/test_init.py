"""Tests for the animation image platform and the legacy `animation:` shim."""

from __future__ import annotations

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.components.animation import (
    DOMAIN,
    LEGACY_REMOVAL_VERSION,
    _capture_legacy_entry,
    _warn_legacy_animation,
)
from esphome.core import CORE
from esphome.types import ConfigType

# ---------------------------------------------------------------------------
# Legacy top-level `animation:` deprecation shim -- REMOVE these tests after
# 2027.1.0 together with the shim in esphome/components/animation/__init__.py.
# ---------------------------------------------------------------------------


def test_warn_legacy_animation_warns_once(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The deprecation warning fires exactly once and never mutates the config."""
    config: ConfigType = {"id": "test_animation", "file": "anim.gif", "type": "rgb565"}

    # A per-entry capture (CONFIG_SCHEMA step) records the raw entry so the
    # one-shot warning can print a pasteable migrated block.
    assert _capture_legacy_entry(config) is config

    with caplog.at_level(logging.WARNING):
        # First call: flag not yet set -> warns and records the flag.
        assert _warn_legacy_animation(config) is config
        # Second call: flag already set -> stays silent (the dedup branch).
        assert _warn_legacy_animation(config) is config

    assert CORE.data[DOMAIN]["legacy_warning_shown"] is True
    warnings = [r for r in caplog.records if r.levelno == logging.WARNING]
    assert len(warnings) == 1
    assert "deprecated" in caplog.text
    assert "platform: animation" in caplog.text
    assert LEGACY_REMOVAL_VERSION in caplog.text


def test_legacy_animation_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The legacy `animation:` block validates, warns, and generates codegen
    through the real read_config/codegen pipeline."""
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(component_config_path("animation_test.yaml"))

    # Deprecation warning surfaced through the real validation pipeline.
    assert "animation" in caplog.text
    assert "deprecated" in caplog.text

    # setup_animation ran: Animation object constructed and loop configured.
    assert "new(test_animation) animation::Animation(" in main_cpp
    assert "test_animation->set_loop(0, 2, 3);" in main_cpp


def test_animation_platform_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The `image:` `platform: animation` form generates codegen through the
    real platform loader (animation/image.py) without any deprecation warning."""
    main_cpp = generate_main(component_config_path("animation_platform_test.yaml"))

    assert "new(test_animation) animation::Animation(" in main_cpp
    assert "test_animation->set_loop(0, 2, 3);" in main_cpp
    # The loop-less entry constructs the object but never configures a loop.
    assert "new(test_animation_no_loop) animation::Animation(" in main_cpp
    assert "test_animation_no_loop->set_loop(" not in main_cpp
