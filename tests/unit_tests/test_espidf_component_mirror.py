"""Tests for the local IDF component-registry mirror."""

from __future__ import annotations

from collections.abc import Callable
import json
from pathlib import Path
import subprocess
from unittest.mock import MagicMock, patch
import zipfile

from filelock import FileLock
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
  esphome/shorthand:
    version: ==2.1.0
  esphome/unquoted:
    version: 2
"""

_ARDUINOJSON = component_mirror.ServiceDep("bblanchon", "arduinojson", "7.4.3")
_MDNS = component_mirror.ServiceDep("espressif", "mdns", "1.12.0")
_TFLITE = component_mirror.ServiceDep("espressif", "esp-tflite-micro", "1.3.3~1")
_SHORTHAND = component_mirror.ServiceDep("esphome", "shorthand", "2.1.0")
_MANIFEST_DEPS = [_MDNS, _TFLITE, _SHORTHAND]
_NS_CMP_1 = component_mirror.ServiceDep("ns", "cmp", "1.0.0")
_NS_CMP_2 = component_mirror.ServiceDep("ns", "cmp", "2.0.0")


@pytest.fixture(autouse=True)
def _isolate_mirror(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(tmp_path / "idf_install"))


def _write_lock(project_dir: Path, text: str = _LOCK_TEXT) -> Path:
    lock = project_dir / "dependencies.lock"
    lock.parent.mkdir(parents=True, exist_ok=True)
    lock.write_text(text, encoding="utf-8")
    return lock


def _ns_cmp_lock(version: str) -> str:
    return (
        "dependencies:\n"
        "  ns/cmp:\n"
        "    source:\n"
        "      type: service\n"
        f"    version: {version}\n"
    )


def _write_manifest(project_dir: Path, text: str = _MANIFEST_TEXT) -> Path:
    manifest = project_dir / "src" / "idf_component.yml"
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(text, encoding="utf-8")
    return manifest


def _add_to_mirror(mirror: Path, dep: component_mirror.ServiceDep) -> None:
    """Lay out one component the way `registry sync` does."""
    version_dir = f"components/{dep.namespace}/{dep.name}/{dep.version}"
    archive = f"{version_dir}/{dep.namespace}__{dep.name}-v{dep.version}.zip"
    checksums = f"{version_dir}/CHECKSUMS.json"
    json_path = mirror / "components" / dep.namespace / f"{dep.name}.json"
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(
        json.dumps(
            {
                "name": dep.name,
                "namespace": dep.namespace,
                "versions": [
                    {"version": dep.version, "url": archive, "checksums": checksums}
                ],
            }
        ),
        encoding="utf-8",
    )
    archive_path = mirror / archive
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path, "w") as zf:
        zf.writestr("idf_component.yml", "")
    (mirror / checksums).write_text("{}", encoding="utf-8")


# ---------------------------------------------------------------------------
# parse_lock_service_deps / parse_manifest_service_deps / project_service_deps
# ---------------------------------------------------------------------------


def test_parse_lock_keeps_only_default_registry_service_deps(tmp_path: Path) -> None:
    """git, local, idf and non-default registry sources cannot be mirrored."""
    deps = component_mirror.parse_lock_service_deps(_write_lock(tmp_path))
    assert deps == [_ARDUINOJSON, _MDNS]


def test_parse_lock_missing_file(tmp_path: Path) -> None:
    assert component_mirror.parse_lock_service_deps(tmp_path / "none.lock") == []


@pytest.mark.parametrize(
    "text",
    [
        pytest.param("{unbalanced", id="corrupt-yaml"),
        pytest.param("[]", id="not-a-mapping"),
        pytest.param("dependencies:\n  espressif/mdns: not-a-dict\n", id="bad-entry"),
        pytest.param(_ns_cmp_lock("1.2"), id="non-string-version"),
    ],
)
def test_parse_lock_tolerates_bad_content(tmp_path: Path, text: str) -> None:
    assert component_mirror.parse_lock_service_deps(_write_lock(tmp_path, text)) == []


def test_parse_lock_unreadable_file_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Read trouble other than a missing file says so instead of raising."""
    lock = tmp_path / "dependencies.lock"
    lock.mkdir()  # read_text raises OSError, not FileNotFoundError
    assert component_mirror.parse_lock_service_deps(lock) == []
    assert "Could not read" in caplog.text


def test_parse_lock_non_utf8_file_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A non-UTF-8 lock is a warning, never a failed build."""
    lock = tmp_path / "dependencies.lock"
    lock.write_bytes(b"dependencies:\n  # caf\xe9\n")
    assert component_mirror.parse_lock_service_deps(lock) == []
    assert "Could not read" in caplog.text


def test_parse_lock_defaults_registry_url(tmp_path: Path) -> None:
    """A service entry without registry_url is a default-registry dependency."""
    lock = _write_lock(tmp_path, _ns_cmp_lock("1.0.0"))
    assert component_mirror.parse_lock_service_deps(lock) == [_NS_CMP_1]


def test_parse_manifest_keeps_only_exact_registry_pins(tmp_path: Path) -> None:
    """Only exact pins survive, including the == shorthand prefix."""
    deps = component_mirror.parse_manifest_service_deps(_write_manifest(tmp_path))
    assert deps == _MANIFEST_DEPS


def test_parse_manifest_missing_file(tmp_path: Path) -> None:
    assert component_mirror.parse_manifest_service_deps(tmp_path / "none.yml") == []


def test_parse_manifest_lowercases_mixed_case_keys(tmp_path: Path) -> None:
    """The registry stores lowercase paths; a mixed-case key must match them."""
    manifest = tmp_path / "idf_component.yml"
    manifest.write_text("dependencies:\n  Espressif/MDNS:\n    version: 1.2.0\n")
    deps = component_mirror.parse_manifest_service_deps(manifest)
    assert deps == [component_mirror.ServiceDep("espressif", "mdns", "1.2.0")]


def test_project_service_deps_merges_lock_and_manifest(tmp_path: Path) -> None:
    """The lock wins for a component in both; the manifest fills the rest."""
    lock = _write_lock(tmp_path)
    manifest = _write_manifest(tmp_path)
    assert component_mirror.project_service_deps(lock, manifest) == [
        _ARDUINOJSON,
        _MDNS,
        _TFLITE,
        _SHORTHAND,
    ]


def test_project_service_deps_manifest_only(tmp_path: Path) -> None:
    """A fresh build has no lock yet; the manifest alone drives the sync."""
    manifest = _write_manifest(tmp_path)
    assert (
        component_mirror.project_service_deps(tmp_path / "dependencies.lock", manifest)
        == _MANIFEST_DEPS
    )


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


@pytest.mark.parametrize(
    "damage",
    [
        pytest.param(
            lambda m: next(m.rglob("*.zip")).unlink(),
            id="archive-gone",
        ),
        pytest.param(
            lambda m: (m / "components/espressif/mdns/1.12.0/CHECKSUMS.json").unlink(),
            id="checksums-gone",
        ),
        pytest.param(
            lambda m: (m / "components/espressif/mdns.json").write_text("{broken"),
            id="corrupt-index",
        ),
        pytest.param(
            lambda m: next(m.rglob("*.zip")).write_bytes(b"PK\x03\x04torn"),
            id="truncated-archive",
        ),
    ],
)
def test_missing_deps_damaged_mirror(
    tmp_path: Path, damage: Callable[[Path], object]
) -> None:
    """Every file the index references must exist, and a broken index
    counts as missing, not covered."""
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, _MDNS)
    damage(mirror)
    assert component_mirror.missing_deps(mirror, [_MDNS]) == [_MDNS]


def test_missing_deps_covered_without_checksums_field(tmp_path: Path) -> None:
    """An older payload without a checksums field only needs its archive."""
    mirror = tmp_path / "mirror"
    _add_to_mirror(mirror, _MDNS)
    index = mirror / "components/espressif/mdns.json"
    doc = json.loads(index.read_text())
    del doc["versions"][0]["checksums"]
    index.write_text(json.dumps(doc))
    assert component_mirror.missing_deps(mirror, [_MDNS]) == []


# ---------------------------------------------------------------------------
# _publish_index / _promote
# ---------------------------------------------------------------------------


def _staged_index(tmp_path: Path, versions: list) -> Path:
    src = tmp_path / "staged.json"
    src.write_text(json.dumps({"versions": versions}), encoding="utf-8")
    return src


def test_publish_index_first_sync(tmp_path: Path) -> None:
    """No live index yet: the staged one is published as is."""
    src = _staged_index(tmp_path, [{"version": "2.0.0"}])
    dst = tmp_path / "live.json"
    component_mirror._publish_index(src, dst)
    assert json.loads(dst.read_text()) == {"versions": [{"version": "2.0.0"}]}


@pytest.mark.parametrize(
    "live", ["not json", '{"versions": null}', '{"versions": {"a": 1}}']
)
def test_publish_index_replaces_a_corrupt_live_index(tmp_path: Path, live: str) -> None:
    """An unreadable live index is replaced wholesale; that is the heal."""
    src = _staged_index(tmp_path, [{"version": "2.0.0"}])
    dst = tmp_path / "live.json"
    dst.write_text(live)
    component_mirror._publish_index(src, dst)
    assert json.loads(dst.read_text()) == {"versions": [{"version": "2.0.0"}]}


def test_publish_index_merges_and_filters(tmp_path: Path) -> None:
    """Unfetched live versions survive; non-dict entries drop, not raise."""
    src = _staged_index(tmp_path, [{"version": "2.0.0"}, "junk"])
    dst = tmp_path / "live.json"
    dst.write_text(
        json.dumps({"versions": [{"version": "1.0.0"}, {"version": "2.0.0"}, "bad"]})
    )
    component_mirror._publish_index(src, dst)
    assert json.loads(dst.read_text()) == {
        "versions": [{"version": "2.0.0"}, {"version": "1.0.0"}]
    }


def test_publish_index_unreadable_live_index_raises(tmp_path: Path) -> None:
    """An I/O error is not corruption; raising keeps the live versions and
    a later run retries."""
    src = _staged_index(tmp_path, [{"version": "2.0.0"}])
    dst = tmp_path / "live.json"
    dst.mkdir()  # read_text raises IsADirectoryError, not FileNotFoundError
    with pytest.raises(OSError):
        component_mirror._publish_index(src, dst)


def test_promote_moves_archives_before_indexes(tmp_path: Path) -> None:
    """A concurrent reader must never see an index entry without its files."""
    staging = tmp_path / "staging"
    _add_to_mirror(staging, _NS_CMP_2)
    order: list[str] = []
    real_rename = component_mirror._rename_with_retry
    real_write = component_mirror.write_file

    def recording_rename(src: Path, dst: Path, **kwargs) -> None:
        order.append(src.name)
        real_rename(src, dst, **kwargs)

    def recording_write(path: Path, text: str) -> None:
        order.append(path.name)
        real_write(path, text)

    with (
        patch.object(component_mirror, "_rename_with_retry", recording_rename),
        patch.object(component_mirror, "write_file", recording_write),
    ):
        component_mirror._promote(staging, tmp_path / "mirror")

    assert order[-1] == "cmp.json"
    assert set(order[:-1]) == {"CHECKSUMS.json", "ns__cmp-v2.0.0.zip"}


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
    returncode: int = 0,
    stderr: str = "",
    side_effect: Exception | Callable | None = None,
    get_python=lambda: "/penv/python",
    get_env=lambda: {"PATH": "/penv"},
) -> tuple[bool, MagicMock]:
    with patch.object(
        component_mirror.subprocess,
        "run",
        return_value=subprocess.CompletedProcess([], returncode, "", stderr),
        side_effect=side_effect,
    ) as mock_run:
        ok = component_mirror.sync_component_mirror(
            project_dir / "dependencies.lock",
            project_dir / "src" / "idf_component.yml",
            get_python,
            get_env,
        )
    return ok, mock_run


def _fake_registry_sync(returncode: int = 0):
    """A subprocess.run stand-in that lays files out like `registry sync`."""

    def run(cmd, **kwargs) -> subprocess.CompletedProcess:
        _add_to_mirror(Path(cmd[-1]), _NS_CMP_2)
        return subprocess.CompletedProcess(cmd, returncode, "", "sync failed")

    return run


def _assert_sync_lock_released() -> None:
    lock = FileLock(str(component_mirror.get_mirror_path() / ".sync.lock"))
    lock.acquire(blocking=False)
    lock.release()


def test_sync_runs_the_manager_for_missing_deps(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _write_lock(tmp_path)
    ok, mock_run = _run_sync(tmp_path)
    # The stub stages nothing, so the post-promote recheck reports failure.
    assert not ok
    assert "uncovered" in caplog.text
    mirror = component_mirror.get_mirror_path()
    assert [call.args[0] for call in mock_run.call_args_list] == [
        [
            "/penv/python",
            "-m",
            "idf_component_manager",
            "registry",
            "sync",
            "--resolution",
            "latest",
            "--component",
            spec,
            str(mirror / ".staging"),
        ]
        for spec in ("bblanchon/arduinojson==7.4.3", "espressif/mdns==1.12.0")
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
    ok, mock_run = _run_sync(tmp_path, returncode=1, stderr="boom")
    assert not ok
    mock_run.assert_called_once()
    assert "Could not mirror" in caplog.text
    _assert_sync_lock_released()


def test_sync_subprocess_errors_are_tolerated(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _write_lock(tmp_path)
    ok, _ = _run_sync(tmp_path, side_effect=OSError("no such file"))
    assert not ok
    assert "Could not mirror" in caplog.text
    _assert_sync_lock_released()


def test_sync_timeout_keeps_the_finished_components(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Each component syncs in its own invocation, so the ones that finish
    before a timeout stay promoted and later runs fetch only the rest."""
    _write_lock(tmp_path)

    def slow_sync(cmd, **kwargs):
        namespace, _, rest = cmd[-2].partition("/")
        name, _, version = rest.partition("==")
        if name != "arduinojson":
            raise subprocess.TimeoutExpired(cmd=cmd, timeout=120)
        _add_to_mirror(
            Path(cmd[-1]), component_mirror.ServiceDep(namespace, name, version)
        )
        return subprocess.CompletedProcess(cmd, 0, "", "")

    ok, _ = _run_sync(tmp_path, side_effect=slow_sync)
    assert not ok
    assert "kept 1 of 2" in caplog.text
    mirror = component_mirror.get_mirror_path()
    assert component_mirror.missing_deps(mirror, [_ARDUINOJSON, _MDNS]) == [_MDNS]
    assert not (mirror / ".staging").exists()
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
    _, mock_run = _run_sync(tmp_path)
    mock_run.assert_called()  # the dead lock did not block the sync


def test_sync_lock_oserror_is_a_failure(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A broken cache is not contention; the retry guard should apply."""
    _write_lock(tmp_path)
    with patch("filelock.FileLock.acquire", side_effect=PermissionError("ro")):
        ok, mock_run = _run_sync(tmp_path)
    assert not ok
    mock_run.assert_not_called()
    assert "Could not lock" in caplog.text


def test_sync_undeletable_staging_is_a_failure(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An undeletable staging tree aborts the sync; stale files must
    never be promoted."""
    _write_lock(tmp_path)
    with patch.object(component_mirror, "rmtree", side_effect=OSError("stuck staging")):
        ok, mock_run = _run_sync(tmp_path)
    assert not ok
    mock_run.assert_not_called()
    assert "Could not mirror" in caplog.text
    _assert_sync_lock_released()


def test_sync_promotes_staged_files_and_merges_the_index(tmp_path: Path) -> None:
    """New files land atomically and existing versions survive the merge."""
    mirror = component_mirror.get_mirror_path()
    _add_to_mirror(mirror, _NS_CMP_1)
    _write_lock(tmp_path, _ns_cmp_lock("2.0.0"))

    ok, _ = _run_sync(tmp_path, side_effect=_fake_registry_sync())

    assert ok
    doc = json.loads((mirror / "components" / "ns" / "cmp.json").read_text())
    assert {entry["version"] for entry in doc["versions"]} == {"1.0.0", "2.0.0"}
    assert (mirror / "components/ns/cmp/2.0.0/ns__cmp-v2.0.0.zip").is_file()
    assert (mirror / "components/ns/cmp/1.0.0/ns__cmp-v1.0.0.zip").is_file()
    assert not (mirror / ".staging").exists()


def test_sync_failure_leaves_no_staging_behind(tmp_path: Path) -> None:
    """A failed sync promotes nothing and removes its staging directory."""
    _write_lock(tmp_path, _ns_cmp_lock("2.0.0"))

    ok, _ = _run_sync(tmp_path, side_effect=_fake_registry_sync(returncode=1))

    assert not ok
    mirror = component_mirror.get_mirror_path()
    assert not (mirror / ".staging").exists()
    assert not (mirror / "components" / "ns").exists()


def test_sync_failed_index_publish_keeps_the_live_index(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed publish keeps the live index; a later run retries."""
    mirror = component_mirror.get_mirror_path()
    _add_to_mirror(mirror, _NS_CMP_1)
    _write_lock(tmp_path, _ns_cmp_lock("2.0.0"))

    with patch.object(
        component_mirror, "write_file", side_effect=EsphomeError("disk full")
    ):
        ok, _ = _run_sync(tmp_path, side_effect=_fake_registry_sync())

    assert not ok
    assert "Could not mirror" in caplog.text
    doc = json.loads((mirror / "components" / "ns" / "cmp.json").read_text())
    assert {entry["version"] for entry in doc["versions"]} == {"1.0.0"}
    _assert_sync_lock_released()
