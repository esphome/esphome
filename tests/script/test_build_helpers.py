"""Unit tests for script/build_helpers.py."""

from pathlib import Path
import sys

import pytest

# Add the script directory to the path so we can import build_helpers.
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import build_helpers  # noqa: E402

from esphome.core import CORE  # noqa: E402


class _FakeComponent:
    def __init__(self, config_schema, *, is_target_platform=False):
        self.multi_conf = False
        self.is_platform_component = False
        self.is_target_platform = is_target_platform
        self.config_schema = config_schema


@pytest.fixture(autouse=True)
def _restore_core_toolchain():
    """Keep CORE.toolchain changes from leaking between tests."""
    saved = CORE.toolchain
    try:
        yield
    finally:
        CORE.toolchain = saved


def test_populate_dependency_config_skips_target_platforms() -> None:
    """Target-platform deps must be skipped, not config-populated, in a host build.

    Regression test for #17035: esp32 (a target platform) appears only as a
    transitive dependency of a host C++ unit test. Running its schema with {}
    set ``CORE.toolchain = ESP_IDF`` as a side effect before failing validation,
    which crashed the host compile with KeyError('esp32'). The fix skips
    target-platform components entirely so their schema never runs.
    """
    CORE.toolchain = None  # the state a host build starts from
    schema_calls = []

    def leaky_schema(value):
        # If this ever runs for a target platform, the bug is back.
        schema_calls.append(value)
        CORE.toolchain = "esp-idf-leak"
        raise ValueError("no board or variant")

    config: dict = {}
    build_helpers.populate_dependency_config(
        config,
        ["esp32"],
        get_component_fn=lambda name: _FakeComponent(
            leaky_schema, is_target_platform=True
        ),
        register_platform_fn=lambda domain: None,
    )

    assert "esp32" not in config  # skipped: no synthesized entry
    assert schema_calls == []  # schema never run
    assert CORE.toolchain is None  # no global side effect leaked


def test_populate_dependency_config_populates_defaults() -> None:
    """A non-target-platform dep still has its schema defaults harvested."""
    config: dict = {}
    build_helpers.populate_dependency_config(
        config,
        ["ok"],
        get_component_fn=lambda name: _FakeComponent(lambda value: {"default": 1}),
        register_platform_fn=lambda domain: None,
    )
    assert config["ok"] == {"default": 1}
