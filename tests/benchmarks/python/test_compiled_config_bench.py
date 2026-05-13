"""CodSpeed benchmarks for the validated-config cache fast path.

PR #16381 added a cache that lets ``esphome upload`` / ``esphome logs``
skip re-running the full config-validation pipeline. These benchmarks
quantify the win against ``read_config`` on a realistic
bluetooth-proxy YAML (full ESP32 / ESP-IDF stack, ethernet,
bluetooth_proxy, sensors, time, debug, plus a few large lambdas).

The benchmark is skipped when pytest-codspeed is not installed, so
the regular unit-test suite keeps working unchanged.
"""

from __future__ import annotations

from pathlib import Path
import shutil

import pytest

from esphome.compiled_config import compiled_config_path, load_compiled_config
from esphome.config import read_config
from esphome.core import CORE
from esphome.storage_json import StorageJSON, ext_storage_path
from esphome.writer import update_storage_json

# Skip the entire module when CodSpeed's pytest plugin isn't available --
# the regular unit-test environment doesn't need it and these benchmarks
# would otherwise show up as missing-fixture errors.
pytest.importorskip("pytest_codspeed")

HERE = Path(__file__).parent
FIXTURE_YAML = HERE / "fixtures" / "bluetooth_proxy_device.yaml"


def _stage_yaml(tmp_path: Path) -> Path:
    """Copy the fixture YAML into a fresh tmp dir.

    Each benchmark gets its own copy so the cache files (which live next
    to the YAML under ``.esphome/storage/``) don't bleed between cases.
    """
    target = tmp_path / FIXTURE_YAML.name
    shutil.copy2(FIXTURE_YAML, target)
    return target


def _prime_cache(yaml_path: Path) -> None:
    """Run the full validation pipeline once and persist the cache+sidecar.

    Mirrors what ``esphome compile`` does: ``read_config`` populates
    ``CORE.config``, then ``update_storage_json`` writes both the
    StorageJSON sidecar and the ``.validated.yaml`` compiled-config
    cache. Leaves CORE clean so the caller can re-enter from scratch.
    """
    CORE.reset()
    CORE.config_path = yaml_path
    config = read_config({}, skip_external_update=True)
    assert config is not None, f"fixture YAML failed to validate: {yaml_path}"
    CORE.config = config
    update_storage_json()
    CORE.reset()


@pytest.fixture
def primed_yaml(tmp_path: Path) -> Path:
    """A staged YAML with the cache + sidecar already on disk."""
    yaml_path = _stage_yaml(tmp_path)
    _prime_cache(yaml_path)
    # Sanity: cache + sidecar are where load_compiled_config will look.
    CORE.config_path = yaml_path
    assert compiled_config_path(yaml_path.name).is_file()
    assert ext_storage_path(yaml_path.name).is_file()
    CORE.reset()
    return yaml_path


@pytest.fixture
def staged_yaml(tmp_path: Path) -> Path:
    """A staged YAML with no cache -- used by the uncached benchmark."""
    return _stage_yaml(tmp_path)


def test_load_compiled_config_cached(primed_yaml: Path, benchmark) -> None:
    """Fast path: deserialize the cached, already-validated config.

    This is the path ``esphome upload`` / ``esphome logs`` take on the
    second invocation against the same YAML.
    """

    def _setup() -> tuple[tuple, dict]:
        CORE.reset()
        CORE.config_path = primed_yaml
        return (primed_yaml,), {}

    benchmark.pedantic(
        load_compiled_config,
        setup=_setup,
        rounds=5,
        iterations=1,
    )


def test_read_config_uncached(staged_yaml: Path, benchmark) -> None:
    """Slow path: full validation pipeline (yaml load + schema + components).

    This is what runs when the cache is absent / stale, or when the user
    passes ``-s`` substitutions. Same YAML as the cached benchmark so
    the two numbers are directly comparable.
    """

    def _setup() -> tuple[tuple, dict]:
        CORE.reset()
        CORE.config_path = staged_yaml
        # skip_external_update mirrors the `logs`/`clean` fast path so
        # the benchmark doesn't depend on network access for external
        # components (matches what __main__.py passes through).
        return ({},), {"skip_external_update": True}

    benchmark.pedantic(
        read_config,
        setup=_setup,
        rounds=3,
        iterations=1,
    )
