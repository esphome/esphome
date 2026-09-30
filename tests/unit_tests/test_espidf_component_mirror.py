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


@pytest.fixture(autouse=True)
def _isolate_mirror(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(tmp_path / "idf_install"))


def _write_lock(tmp_path: Path, text: str = _LOCK_TEXT) -> Path:
    lock = tmp_path / "dependencies.lock"
    lock.write_text(text, encoding="utf-8")
    return lock


def _add_to_mirror(mirror: Path, namespace: str, name: str, version: str) -> None:
    """Lay out one component the way `registry sync` does."""
    archive = (
        f"components/{namespace}/{name}/{version}/{namespace}__{name}-v{version}.zip"
    )
    json_path = mirror / "components" / namespace / f"{name}.json"
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(
        json.dumps(
            {
                "name": name,
                "namespace": namespace,
                "versions": [{"version": version, "url": archive}],
            }
        ),
        encoding="utf-8",
    )
    archive_path = mirror / archive
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    archive_path.write_bytes(b"zip")


# ---------------------------------------------------------------------------
# parse_lock_service_deps
# ---------------------------------------------------------------------------


def test_parse_lock_keeps_only_service_deps(tmp_path: Path) -> None:
    """git, local and idf sources cannot come from a registry mirror."""
    deps = component_mirror.parse_lock_service_deps(_write_lock(tmp_path))
    assert deps == [
        component_mirror.ServiceDep(
            "bblanchon", "arduinojson", "7.4.3", "https://components.espressif.com"
        ),
        component_mirror.ServiceDep(
            "espressif", "mdns", "1.12.0", "https://components.espressif.com"
        ),
    ]


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
    lock = _write_lock(
        tmp_path,
        "dependencies:\n"
        "  ns/cmp:\n"
        "    source:\n"
        "      type: service\n"
        "    version: 1.0.0\n",
    )
    (dep,) = component_mirror.parse_lock_service_deps(lock)
    assert dep.registry_url == "https://components.espressif.com"


# ---------------------------------------------------------------------------
# missing_deps
# ---------------------------------------------------------------------------


def test_missing_deps_covered_and_not(tmp_path: Path) -> None:
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, "espressif", "mdns", "1.12.0")
    deps = [
        component_mirror.ServiceDep("espressif", "mdns", "1.12.0", "u"),
        component_mirror.ServiceDep("espressif", "mdns", "1.13.0", "u"),
        component_mirror.ServiceDep("bblanchon", "arduinojson", "7.4.3", "u"),
    ]
    assert component_mirror.missing_deps(mirror, deps) == deps[1:]


def test_missing_deps_archive_gone(tmp_path: Path) -> None:
    """Metadata without the archive must count as missing, not covered."""
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, "espressif", "mdns", "1.12.0")
    next(mirror.rglob("*.zip")).unlink()
    dep = component_mirror.ServiceDep("espressif", "mdns", "1.12.0", "u")
    assert component_mirror.missing_deps(mirror, [dep]) == [dep]


def test_missing_deps_corrupt_json(tmp_path: Path) -> None:
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, "espressif", "mdns", "1.12.0")
    (mirror / "components/espressif/mdns.json").write_text("{broken")
    dep = component_mirror.ServiceDep("espressif", "mdns", "1.12.0", "u")
    assert component_mirror.missing_deps(mirror, [dep]) == [dep]


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


def test_component_mirror_env_unwritable_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A read-only cache disables the feature instead of failing the build."""
    with patch.object(Path, "mkdir", side_effect=OSError("read-only")):
        assert component_mirror.component_mirror_env() == {}


# ---------------------------------------------------------------------------
# sync_component_mirror
# ---------------------------------------------------------------------------


def _run_sync(tmp_path: Path, **kwargs) -> tuple[bool, MagicMock]:
    lock = kwargs.pop("lock", None) or _write_lock(tmp_path)
    completed = kwargs.pop(
        "completed", subprocess.CompletedProcess([], 0, stdout="", stderr="")
    )
    with patch.object(
        component_mirror.subprocess,
        "run",
        return_value=completed,
        side_effect=kwargs.pop("side_effect", None),
    ) as mock_run:
        ok = component_mirror.sync_component_mirror(
            lock,
            kwargs.pop("get_python", lambda: "/penv/python"),
            kwargs.pop("get_env", lambda: {"PATH": "/penv"}),
        )
    return ok, mock_run


def test_sync_runs_the_manager_for_missing_deps(tmp_path: Path) -> None:
    ok, mock_run = _run_sync(tmp_path)
    assert ok
    mock_run.assert_called_once()
    argv = mock_run.call_args.args[0]
    mirror = component_mirror.get_mirror_path()
    assert argv == [
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
    # The advisory lock must not linger after a successful sync.
    assert not (mirror / ".sync.lock").exists()


def test_sync_skips_when_covered(tmp_path: Path) -> None:
    """A covered lock resolves no environment and runs no subprocess."""
    mirror = component_mirror.get_mirror_path()
    _add_to_mirror(mirror, "espressif", "mdns", "1.12.0")
    _add_to_mirror(mirror, "bblanchon", "arduinojson", "7.4.3")

    def _boom() -> str:
        raise AssertionError("environment resolved for a covered mirror")

    ok, mock_run = _run_sync(tmp_path, get_python=_boom, get_env=_boom)
    assert ok
    mock_run.assert_not_called()


def test_sync_skips_non_default_registry(tmp_path: Path) -> None:
    lock = _write_lock(
        tmp_path,
        "dependencies:\n"
        "  ns/cmp:\n"
        "    source:\n"
        "      registry_url: https://registry.example.com\n"
        "      type: service\n"
        "    version: 1.0.0\n",
    )
    ok, mock_run = _run_sync(tmp_path, lock=lock)
    assert ok
    mock_run.assert_not_called()


def test_sync_failure_is_tolerated(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    completed = subprocess.CompletedProcess([], 1, stdout="", stderr="boom")
    ok, mock_run = _run_sync(tmp_path, completed=completed)
    assert not ok
    mock_run.assert_called_once()
    assert "Could not mirror" in caplog.text
    assert not (component_mirror.get_mirror_path() / ".sync.lock").exists()


@pytest.mark.parametrize(
    "side_effect",
    [OSError("no such file"), subprocess.TimeoutExpired(cmd=[], timeout=120)],
)
def test_sync_subprocess_errors_are_tolerated(
    tmp_path: Path, side_effect: Exception, caplog: pytest.LogCaptureFixture
) -> None:
    ok, _ = _run_sync(tmp_path, side_effect=side_effect)
    assert not ok
    assert "Could not mirror" in caplog.text
    assert not (component_mirror.get_mirror_path() / ".sync.lock").exists()


def test_sync_environment_resolution_failure_is_tolerated(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    def _boom() -> str:
        raise EsphomeError("python not found")

    ok, mock_run = _run_sync(tmp_path, get_python=_boom)
    assert not ok
    mock_run.assert_not_called()
    assert "Could not mirror" in caplog.text


def test_sync_skips_when_another_process_holds_the_lock(tmp_path: Path) -> None:
    mirror = component_mirror.get_mirror_path()
    mirror.mkdir(parents=True)
    (mirror / ".sync.lock").touch()
    ok, mock_run = _run_sync(tmp_path)
    assert ok
    mock_run.assert_not_called()


def test_sync_reclaims_a_stale_lock(tmp_path: Path) -> None:
    """A lock from a crashed process must not disable syncing forever."""
    import os

    mirror = component_mirror.get_mirror_path()
    mirror.mkdir(parents=True)
    stale = mirror / ".sync.lock"
    stale.touch()
    past = stale.stat().st_mtime - 3600
    os.utime(stale, (past, past))
    ok, mock_run = _run_sync(tmp_path)
    assert ok
    mock_run.assert_called_once()
