"""Tests for the local IDF component-registry mirror."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
from unittest.mock import MagicMock, patch

import pytest

from esphome.core import EsphomeError
from esphome.espidf import component_mirror

_LOCK_TEXT = """\
dependencies:
  bblanchon/arduinojson:
    component_hash: 5e6aff2bc5bc988b36c8e90c01e637495ee895ad3af93911860c21ec6da79b6a
    dependencies: []
    source:
      registry_url: https://components.espressif.com/
      type: service
    version: 7.4.3
  espressif/mdns:
    component_hash: 3ba256ac95e07c274be53cbd73f06cb846c403b61e8fbdf1be57bdb79db7a63e
    dependencies:
    - name: idf
      require: private
      version: '>=5.0'
    source:
      registry_url: https://components.espressif.com
      type: service
    version: 1.12.0
  esphome/libsodium:
    source:
      path: /pio_components/idf/cb5ad5b7/esphome/libsodium
      type: local
    version: '*'
  fastled/fastled:
    source:
      git: https://github.com/fastled/fastled
      type: git
    version: 3.7.0
  other/private:
    source:
      registry_url: https://registry.example.com
      type: service
    version: 1.0.0
  idf:
    source:
      type: idf
    version: 5.5.5
direct_dependencies:
- bblanchon/arduinojson
- espressif/mdns
manifest_hash: f4e38c031cbb70d5070c5806fc580cce194b8c9a0b919f52a7e952f7d496dda4
target: esp32
version: 2.0.0
"""

_MANIFEST_TEXT = """\
dependencies:
  espressif/mdns:
    version: 1.12.0
  espressif/esp-tflite-micro:
    version: 1.3.3~1
  fastled/fastled:
    version: 3.7.0
    git: https://github.com/fastled/fastled
  esphome/libsodium:
    version: '*'
    override_path: /stub/libsodium
  espressif/ranged:
    version: ^1.2.0
"""

_ARDUINOJSON = component_mirror.ServiceDep("bblanchon", "arduinojson", "7.4.3")
_MDNS = component_mirror.ServiceDep("espressif", "mdns", "1.12.0")
_TFLITE = component_mirror.ServiceDep("espressif", "esp-tflite-micro", "1.3.3~1")


@pytest.fixture(autouse=True)
def _isolate_mirror(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(tmp_path / "idf_install"))


def _write_lock(project_dir: Path, text: str = _LOCK_TEXT) -> Path:
    lock = project_dir / "dependencies.lock"
    lock.parent.mkdir(parents=True, exist_ok=True)
    lock.write_text(text, encoding="utf-8")
    return lock


def _write_manifest(project_dir: Path, text: str = _MANIFEST_TEXT) -> Path:
    manifest = project_dir / "src" / "idf_component.yml"
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(text, encoding="utf-8")
    return manifest


def _add_to_mirror(mirror: Path, dep: component_mirror.ServiceDep) -> None:
    """Lay out one component the way `registry sync` does."""
    archive = (
        f"components/{dep.namespace}/{dep.name}/{dep.version}/"
        f"{dep.namespace}__{dep.name}-v{dep.version}.zip"
    )
    json_path = mirror / "components" / dep.namespace / f"{dep.name}.json"
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(
        json.dumps(
            {
                "name": dep.name,
                "namespace": dep.namespace,
                "versions": [{"version": dep.version, "url": archive}],
            }
        ),
        encoding="utf-8",
    )
    archive_path = mirror / archive
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    archive_path.write_bytes(b"zip")


# ---------------------------------------------------------------------------
# parse_lock_service_deps / parse_manifest_service_deps / project_service_deps
# ---------------------------------------------------------------------------


def test_parse_lock_keeps_only_default_registry_service_deps(tmp_path: Path) -> None:
    """git, local, idf and non-default registry sources cannot be mirrored."""
    deps = component_mirror.parse_lock_service_deps(_write_lock(tmp_path))
    assert deps == [_ARDUINOJSON, _MDNS]


def test_parse_lock_missing_file(tmp_path: Path) -> None:
    assert component_mirror.parse_lock_service_deps(tmp_path / "none.lock") == []


def test_parse_lock_corrupt_yaml(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    lock = _write_lock(tmp_path, "{unbalanced")
    assert component_mirror.parse_lock_service_deps(lock) == []
    assert "Could not parse" in caplog.text


def test_parse_lock_wrong_shape(tmp_path: Path) -> None:
    assert component_mirror.parse_lock_service_deps(_write_lock(tmp_path, "[]")) == []
    lock = _write_lock(tmp_path, "dependencies:\n  espressif/mdns: not-a-dict\n")
    assert component_mirror.parse_lock_service_deps(lock) == []


def test_parse_lock_defaults_registry_url(tmp_path: Path) -> None:
    """A service entry without registry_url is a default-registry dependency."""
    lock = _write_lock(
        tmp_path,
        "dependencies:\n"
        "  ns/cmp:\n"
        "    source:\n"
        "      type: service\n"
        "    version: 1.0.0\n",
    )
    assert component_mirror.parse_lock_service_deps(lock) == [
        component_mirror.ServiceDep("ns", "cmp", "1.0.0")
    ]


def test_parse_manifest_keeps_only_exact_registry_pins(tmp_path: Path) -> None:
    """git, override_path, wildcard and range specs are left to the solver."""
    deps = component_mirror.parse_manifest_service_deps(_write_manifest(tmp_path))
    assert deps == [_MDNS, _TFLITE]


def test_parse_manifest_missing_file(tmp_path: Path) -> None:
    assert component_mirror.parse_manifest_service_deps(tmp_path / "none.yml") == []


def test_project_service_deps_merges_lock_and_manifest(tmp_path: Path) -> None:
    """The lock wins for a component in both; the manifest fills the rest."""
    _write_lock(tmp_path)
    _write_manifest(tmp_path)
    assert component_mirror.project_service_deps(tmp_path) == [
        _ARDUINOJSON,
        _MDNS,
        _TFLITE,
    ]


def test_project_service_deps_manifest_only(tmp_path: Path) -> None:
    """A fresh build has no lock yet; the manifest alone drives the sync."""
    _write_manifest(tmp_path)
    assert component_mirror.project_service_deps(tmp_path) == [_MDNS, _TFLITE]


# ---------------------------------------------------------------------------
# missing_deps
# ---------------------------------------------------------------------------


def test_missing_deps_covered_and_not(tmp_path: Path) -> None:
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, _MDNS)
    deps = [
        _MDNS,
        component_mirror.ServiceDep("espressif", "mdns", "1.13.0"),
        _ARDUINOJSON,
    ]
    assert component_mirror.missing_deps(mirror, deps) == deps[1:]


def test_missing_deps_archive_gone(tmp_path: Path) -> None:
    """Metadata without the archive must count as missing, not covered."""
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, _MDNS)
    next(mirror.rglob("*.zip")).unlink()
    assert component_mirror.missing_deps(mirror, [_MDNS]) == [_MDNS]


def test_missing_deps_corrupt_json(tmp_path: Path) -> None:
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, _MDNS)
    (mirror / "components/espressif/mdns.json").write_text("{broken")
    assert component_mirror.missing_deps(mirror, [_MDNS]) == [_MDNS]


# ---------------------------------------------------------------------------
# component_mirror_env
# ---------------------------------------------------------------------------


def test_component_mirror_env_sets_file_url(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("IDF_COMPONENT_LOCAL_STORAGE_URL", raising=False)
    monkeypatch.delenv("IDF_COMPONENT_CHECK_NEW_VERSION", raising=False)
    env = component_mirror.component_mirror_env()
    mirror = component_mirror.get_mirror_path()
    assert env["IDF_COMPONENT_LOCAL_STORAGE_URL"] == mirror.as_uri()
    assert env["IDF_COMPONENT_LOCAL_STORAGE_URL"].startswith("file:///")
    assert env["IDF_COMPONENT_CHECK_NEW_VERSION"] == "0"
    assert mirror.is_dir()


def test_component_mirror_env_preserves_user_values(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A user local storage list keeps precedence; their new-version choice wins."""
    monkeypatch.setenv("IDF_COMPONENT_LOCAL_STORAGE_URL", "file:///user/mirror")
    monkeypatch.setenv("IDF_COMPONENT_CHECK_NEW_VERSION", "1")
    env = component_mirror.component_mirror_env()
    mirror_uri = component_mirror.get_mirror_path().as_uri()
    assert env["IDF_COMPONENT_LOCAL_STORAGE_URL"] == f"file:///user/mirror;{mirror_uri}"
    assert "IDF_COMPONENT_CHECK_NEW_VERSION" not in env


def test_component_mirror_env_unwritable_cache() -> None:
    """A read-only cache disables the feature instead of failing the build."""
    with patch.object(Path, "mkdir", side_effect=OSError("read-only")):
        assert component_mirror.component_mirror_env() == {}


# ---------------------------------------------------------------------------
# sync_component_mirror
# ---------------------------------------------------------------------------


def _run_sync(
    project_dir: Path,
    *,
    completed: subprocess.CompletedProcess | None = None,
    side_effect: Exception | None = None,
    get_python=lambda: "/penv/python",
    get_env=lambda: {"PATH": "/penv"},
) -> tuple[bool, MagicMock]:
    if completed is None:
        completed = subprocess.CompletedProcess([], 0, stdout="", stderr="")
    with patch.object(
        component_mirror.subprocess,
        "run",
        return_value=completed,
        side_effect=side_effect,
    ) as mock_run:
        ok = component_mirror.sync_component_mirror(project_dir, get_python, get_env)
    return ok, mock_run


def _assert_sync_lock_released() -> None:
    from filelock import FileLock

    lock = FileLock(str(component_mirror.get_mirror_path() / ".sync.lock"))
    lock.acquire(blocking=False)
    lock.release()


def test_sync_runs_the_manager_for_missing_deps(tmp_path: Path) -> None:
    _write_lock(tmp_path)
    ok, mock_run = _run_sync(tmp_path)
    assert ok
    mock_run.assert_called_once()
    mirror = component_mirror.get_mirror_path()
    assert mock_run.call_args.args[0] == [
        "/penv/python",
        "-m",
        "idf_component_manager",
        "registry",
        "sync",
        "--component",
        "bblanchon/arduinojson==7.4.3",
        "--component",
        "espressif/mdns==1.12.0",
        str(mirror),
    ]
    assert mock_run.call_args.kwargs["env"] == {"PATH": "/penv"}
    _assert_sync_lock_released()


def test_sync_skips_when_covered(tmp_path: Path) -> None:
    """A covered project resolves no environment and runs no subprocess."""
    _write_lock(tmp_path)
    mirror = component_mirror.get_mirror_path()
    _add_to_mirror(mirror, _MDNS)
    _add_to_mirror(mirror, _ARDUINOJSON)

    def _boom() -> str:
        raise AssertionError("environment resolved for a covered mirror")

    ok, mock_run = _run_sync(tmp_path, get_python=_boom, get_env=_boom)
    assert ok
    mock_run.assert_not_called()


def test_sync_failure_is_tolerated(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _write_lock(tmp_path)
    completed = subprocess.CompletedProcess([], 1, stdout="", stderr="boom")
    ok, mock_run = _run_sync(tmp_path, completed=completed)
    assert not ok
    mock_run.assert_called_once()
    assert "Could not mirror" in caplog.text
    _assert_sync_lock_released()


@pytest.mark.parametrize(
    "side_effect",
    [OSError("no such file"), subprocess.TimeoutExpired(cmd=[], timeout=120)],
)
def test_sync_subprocess_errors_are_tolerated(
    tmp_path: Path, side_effect: Exception, caplog: pytest.LogCaptureFixture
) -> None:
    _write_lock(tmp_path)
    ok, _ = _run_sync(tmp_path, side_effect=side_effect)
    assert not ok
    assert "Could not mirror" in caplog.text
    _assert_sync_lock_released()


def test_sync_environment_resolution_failure_is_tolerated(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _write_lock(tmp_path)

    def _boom() -> str:
        raise EsphomeError("python not found")

    ok, mock_run = _run_sync(tmp_path, get_python=_boom)
    assert not ok
    mock_run.assert_not_called()
    assert "Could not mirror" in caplog.text


def test_sync_skips_when_another_process_holds_the_lock(tmp_path: Path) -> None:
    from filelock import FileLock

    _write_lock(tmp_path)
    mirror = component_mirror.get_mirror_path()
    mirror.mkdir(parents=True)
    held = FileLock(str(mirror / ".sync.lock"))
    held.acquire(blocking=False)
    try:
        ok, mock_run = _run_sync(tmp_path)
    finally:
        held.release()
    assert ok
    mock_run.assert_not_called()


def test_sync_ignores_a_leftover_lock_file(tmp_path: Path) -> None:
    """A lock file from a dead process does not block: the OS lock is gone."""
    _write_lock(tmp_path)
    mirror = component_mirror.get_mirror_path()
    mirror.mkdir(parents=True)
    (mirror / ".sync.lock").touch()
    ok, mock_run = _run_sync(tmp_path)
    assert ok
    mock_run.assert_called_once()
