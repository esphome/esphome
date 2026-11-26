"""Unit tests for script/build_helpers.py."""

from collections.abc import Generator
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


def test_load_component_yaml_configs_extends_lists(tmp_path: Path) -> None:
    """Several directories can each add entries to one platform component."""
    (tmp_path / "first").mkdir()
    (tmp_path / "first" / "benchmark.yaml").write_text(
        "uart:\n  port: /dev/a\npacket_interface:\n  - platform: uart\n"
    )
    (tmp_path / "second").mkdir()
    (tmp_path / "second" / "benchmark.yaml").write_text(
        "uart:\n  port: /dev/b\npacket_interface:\n  - platform: udp\n"
    )
    merged = build_helpers.load_component_yaml_configs(["first", "second"], tmp_path)
    assert merged["packet_interface"] == [{"platform": "uart"}, {"platform": "udp"}]
    # Other values are taken from the first directory that sets them
    assert merged["uart"] == {"port": "/dev/a"}


@pytest.fixture
def fresh_core() -> Generator[None]:
    """Reset the global CORE before and after a test that compiles through it."""
    CORE.reset()
    yield
    CORE.reset()


@pytest.mark.parametrize(
    ("cxx_build_flags", "expected"),
    [(["-fno-rtti"], {"-fno-rtti"}), (None, set())],
)
def test_compile_and_get_binary_cxx_build_flags(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    fresh_core: None,
    cxx_build_flags: list[str] | None,
    expected: set[str],
) -> None:
    """C++-only flags reach CORE.cxx_build_flags, never the shared build flags."""
    seen: dict[str, set[str]] = {}

    def fake_compile(args: object, config: object) -> int:
        seen["cxx"] = set(CORE.cxx_build_flags)
        seen["all"] = set(CORE.build_flags)
        return 0

    binary = tmp_path / "program"
    binary.touch()
    monkeypatch.setattr(build_helpers, "load_component_yaml_configs", lambda *a: {})
    monkeypatch.setattr(
        build_helpers, "resolve_dependencies_with_overrides", lambda *a: []
    )
    monkeypatch.setattr(build_helpers, "validate_config", lambda config, _: config)
    monkeypatch.setattr(build_helpers, "command_compile", fake_compile)
    monkeypatch.setattr(build_helpers, "get_elf_path", lambda: binary)

    exit_code, path = build_helpers.compile_and_get_binary(
        {"esphome": {}},
        [],
        tmp_path,
        lambda *a: None,
        cxx_build_flags=cxx_build_flags,
    )

    assert (exit_code, path) == (build_helpers.EXIT_OK, str(binary))
    assert seen["cxx"] == expected
    assert not expected & seen["all"]
