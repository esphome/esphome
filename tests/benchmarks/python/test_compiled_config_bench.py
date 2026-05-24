"""CodSpeed benchmarks for the validated-config cache fast path.

PR #16381 added a cache that lets ``esphome upload`` / ``esphome logs``
skip re-running the full config-validation pipeline. These benchmarks
compare the cached path (``load_compiled_config``) against the slow
path (``read_config``) on the same input.

The fixture YAML is a modest bluetooth-proxy device. The two paths
end up close on a config this small -- the win grows with config
complexity (external components, large package trees, deeply nested
schemas), where the slow path can be orders of magnitude slower than
the cache load.

Skipped when ``pytest-codspeed`` isn't installed so the regular
unit-test suite keeps working unchanged.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import shutil
from typing import Any

import pytest

from esphome.compiled_config import compiled_config_path, load_compiled_config
from esphome.config import read_config
from esphome.core import CORE
from esphome.storage_json import ext_storage_path
from esphome.writer import update_storage_json

pytest.importorskip("pytest_codspeed")

HERE = Path(__file__).parent
FIXTURE_YAML = HERE / "fixtures" / "bluetooth_proxy_device.yaml"


def _stage_yaml(tmp_path: Path) -> Path:
    """Copy fixture YAML into a fresh tmp dir.

    Each benchmark gets its own copy so the cache files (under
    ``.esphome/storage/`` next to the YAML) don't bleed between cases.
    """
    target = tmp_path / FIXTURE_YAML.name
    shutil.copy2(FIXTURE_YAML, target)
    return target


def _prime_cache(yaml_path: Path) -> None:
    """Run full validation once and persist the cache + sidecar.

    Mirrors ``esphome compile``: ``read_config`` populates ``CORE.config``,
    then ``update_storage_json`` writes both the StorageJSON sidecar and
    the ``.validated.yaml`` compiled-config cache.
    """
    CORE.config_path = yaml_path
    config = read_config({}, skip_external_update=True)
    assert config is not None, f"fixture YAML failed to validate: {yaml_path}"
    CORE.config = config
    update_storage_json()


@pytest.fixture
def staged_yaml(tmp_path: Path) -> Path:
    """YAML copied into tmp_path; no cache files written yet."""
    return _stage_yaml(tmp_path)


@pytest.fixture
def primed_yaml(staged_yaml: Path) -> Path:
    """YAML plus a fresh cache + sidecar on disk."""
    _prime_cache(staged_yaml)
    assert compiled_config_path(staged_yaml.name).is_file()
    assert ext_storage_path(staged_yaml.name).is_file()
    return staged_yaml


def _resetting_setup(
    yaml_path: Path,
    args: tuple[Any, ...],
    kwargs: dict[str, Any],
) -> Callable[[], tuple[tuple[Any, ...], dict[str, Any]]]:
    """Build a per-iteration setup that resets CORE and re-pins config_path."""

    def setup() -> tuple[tuple[Any, ...], dict[str, Any]]:
        CORE.reset()
        CORE.config_path = yaml_path
        return args, kwargs

    return setup


def test_load_compiled_config_cached(primed_yaml: Path, benchmark) -> None:
    """Fast path: deserialize the cached, already-validated config."""
    benchmark.pedantic(
        load_compiled_config,
        setup=_resetting_setup(primed_yaml, (primed_yaml,), {}),
        rounds=5,
        iterations=1,
    )


def test_read_config_uncached(primed_yaml: Path, benchmark) -> None:
    """Slow path: full validation pipeline (yaml load + schema + components).

    Uses the same primed fixture as the cached path -- ``read_config``
    ignores the cache file on disk, so the two benchmarks measure the
    same input from two different code paths.
    """
    benchmark.pedantic(
        read_config,
        setup=_resetting_setup(primed_yaml, ({},), {"skip_external_update": True}),
        rounds=3,
        iterations=1,
    )
