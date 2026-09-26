"""Tests for the online_image platform and the legacy `online_image:` shim."""

from __future__ import annotations

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.components.online_image import (
    DOMAIN,
    LEGACY_REMOVAL_VERSION,
    _capture_legacy_entry,
    _warn_legacy_online_image,
)
from esphome.core import CORE
from esphome.types import ConfigType

# ---------------------------------------------------------------------------
# Legacy top-level `online_image:` deprecation shim -- REMOVE these tests after
# 2027.1.0 together with the shim in esphome/components/online_image/__init__.py.
# ---------------------------------------------------------------------------


def test_warn_legacy_online_image_warns_once(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The deprecation warning fires exactly once and never mutates the config."""
    config: ConfigType = {"id": "test_online_image", "url": "http://example.com/i.png"}

    # A per-entry capture (CONFIG_SCHEMA step) records the raw entry so the
    # one-shot warning can print a pasteable migrated block.
    assert _capture_legacy_entry(config) is config

    with caplog.at_level(logging.WARNING):
        # First call: flag not yet set -> warns and records the flag.
        assert _warn_legacy_online_image(config) is config
        # Second call: flag already set -> stays silent (the dedup branch).
        assert _warn_legacy_online_image(config) is config

    assert CORE.data[DOMAIN]["legacy_warning_shown"] is True
    warnings = [r for r in caplog.records if r.levelno == logging.WARNING]
    assert len(warnings) == 1
    assert "deprecated" in caplog.text
    assert "platform: online_image" in caplog.text
    assert LEGACY_REMOVAL_VERSION in caplog.text


def test_legacy_online_image_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The legacy `online_image:` block validates, warns, and generates codegen
    through the real read_config/codegen pipeline."""
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(component_config_path("online_image_test.yaml"))

    # Deprecation warning surfaced through the real validation pipeline.
    assert "online_image" in caplog.text
    assert "deprecated" in caplog.text

    # setup_online_image ran: OnlineImage object constructed and parented.
    assert "new(test_online_image) online_image::OnlineImage(" in main_cpp


def test_online_image_platform_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The `image:` `platform: online_image` form generates codegen through the
    real platform loader (online_image/image.py) without a deprecation warning."""
    main_cpp = generate_main(component_config_path("online_image_platform_test.yaml"))

    assert "new(test_online_image) online_image::OnlineImage(" in main_cpp
