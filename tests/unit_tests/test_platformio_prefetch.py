"""Tests for the parallel PlatformIO package prefetch."""

import json
from pathlib import Path
import sys
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from esphome.core import CORE
import esphome.platformio.prefetch as pf


@pytest.fixture(autouse=True)
def _core(tmp_path: Path):
    CORE.reset()
    CORE.build_path = str(tmp_path)
    CORE.name = "testenv"
    yield
    CORE.reset()


class _FakeSpec(SimpleNamespace):
    """PackageSpec stand-in for the attributes the prefetch reads."""

    def __init__(self, *, owner=None, requirements=None, **kwargs) -> None:
        super().__init__(owner=owner, requirements=requirements, **kwargs)


def _fake_manager(tmp_path: Path) -> MagicMock:
    m = MagicMock()
    # _resolve and _preinstall construct same-class instances
    m.__class__ = lambda package_dir=None: m
    m.get_package.return_value = None
    m.search_registry_packages.return_value = [{"any": 1}]
    m.find_best_registry_version.return_value = (
        {"name": "toolchain-xtensa"},
        {
            "name": "2.0.0",
            "files": [
                {
                    "download_url": "https://dl.example/t.tar.gz",
                    "checksum": {"sha256": "cafe"},
                    "size": 1000,
                }
            ],
        },
    )
    m.pick_compatible_pkg_file.side_effect = lambda files: files[0]
    m.compute_download_path.side_effect = lambda url, checksum: str(
        tmp_path / "dl" / f"{abs(hash((url, checksum)))}"
    )
    return m


def _mirror_patch():
    return patch.dict(
        "sys.modules",
        {
            "platformio.registry.mirror": SimpleNamespace(
                RegistryFileMirrorIterator=lambda url: iter(
                    [("https://mirror.example/t.tar.gz", "beef")]
                )
            )
        },
    )


def test_registry_jobs_resolves_like_platformio(tmp_path: Path) -> None:
    """A registry spec resolves to a job keyed by mirror URL and checksum."""
    m = _fake_manager(tmp_path)
    with _mirror_patch():
        jobs, failed, installable = pf._registry_jobs(
            m, [_FakeSpec(uri=None, name="toolchain-xtensa")], set()
        )
    assert failed == 0
    assert len(jobs) == 1
    assert [n for n, _ in installable] == ["toolchain-xtensa@2.0.0"]
    name, size, fetch = jobs[0]
    assert name == "toolchain-xtensa@2.0.0"
    assert size == 1000
    m.compute_download_path.assert_called_once_with(
        "https://mirror.example/t.tar.gz", "beef"
    )
    assert callable(fetch)


@pytest.mark.parametrize(
    ("method", "attr", "value"),
    [
        ("get_package", "return_value", object()),  # already installed
        ("search_registry_packages", "return_value", []),  # unknown package
        ("find_best_registry_version", "return_value", (None, None)),  # no match
        ("pick_compatible_pkg_file", "side_effect", lambda files: None),  # no file
    ],
)
def test_registry_jobs_skips(tmp_path: Path, method, attr, value) -> None:
    """Entries PlatformIO would not download produce no job."""
    m = _fake_manager(tmp_path)
    setattr(getattr(m, method), attr, value)
    with _mirror_patch():
        assert pf._registry_jobs(m, [_FakeSpec(uri=None, name="x")], set()) == (
            [],
            0,
            [],
        )


def test_registry_jobs_skips_cached_and_sizeless(tmp_path: Path) -> None:
    """A cached archive needs no download but is still installable; a
    sizeless uncached one is left to PlatformIO entirely."""
    m = _fake_manager(tmp_path)
    dl = Path(m.compute_download_path("https://mirror.example/t.tar.gz", "beef"))
    dl.parent.mkdir(parents=True, exist_ok=True)
    dl.touch()
    with _mirror_patch():
        jobs, failed, installable = pf._registry_jobs(
            m, [_FakeSpec(uri=None, name="x")], set()
        )
    assert (jobs, failed) == ([], 0)
    assert [n for n, _ in installable] == ["toolchain-xtensa@2.0.0"]
    dl.unlink()
    m.find_best_registry_version.return_value[1]["files"][0]["size"] = 0
    with _mirror_patch():
        assert pf._registry_jobs(m, [_FakeSpec(uri=None, name="x")], set()) == (
            [],
            0,
            [],
        )


def test_registry_jobs_dedupes_download_paths(tmp_path: Path) -> None:
    """Duplicate specs resolve once and one archive yields one job (two
    workers must never share a .part); nine specs against eight workers
    also exercise the thread-local manager reuse."""
    m = _fake_manager(tmp_path)
    specs = [_FakeSpec(uri=None, name="dup"), _FakeSpec(uri=None, name="dup")]
    specs += [_FakeSpec(uri=None, name=f"n{i}") for i in range(8)]
    with _mirror_patch():
        jobs, failed, _installable = pf._registry_jobs(m, specs, set())
    # the fake resolves every spec to the same mirror URL and checksum
    assert failed == 0
    assert len(jobs) == 1
    assert m.search_registry_packages.call_count == 9  # dup resolved once


def test_registry_jobs_uri_specs_excluded(tmp_path: Path) -> None:
    """URL specs never reach the registry resolution."""
    m = _fake_manager(tmp_path)
    assert pf._registry_jobs(
        m, [_FakeSpec(uri="https://x/y.zip", name="y")], set()
    ) == ([], 0, [])
    m.search_registry_packages.assert_not_called()


def test_registry_jobs_dedup_keeps_distinct_owners(tmp_path: Path) -> None:
    """platformio/x and pioarduino/x are different packages."""
    m = _fake_manager(tmp_path)
    specs = [
        _FakeSpec(uri=None, name="framework-x", owner="platformio"),
        _FakeSpec(uri=None, name="framework-x", owner="pioarduino"),
    ]
    with _mirror_patch():
        pf._registry_jobs(m, specs, set())
    assert m.search_registry_packages.call_count == 2


def test_registry_jobs_all_failed_warns_once(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A whole-batch failure is a systemic fault and must be visible."""
    m = _fake_manager(tmp_path)
    m.search_registry_packages.side_effect = RuntimeError("registry down")
    with _mirror_patch():
        jobs, failed, installable = pf._registry_jobs(
            m,
            [_FakeSpec(uri=None, name="a"), _FakeSpec(uri=None, name="b")],
            set(),
        )
    assert (jobs, failed, installable) == ([], 2, [])
    assert "Could not resolve any of 2" in caplog.text


def test_uri_fetch_job_promotes_atomically(tmp_path: Path) -> None:
    """Checksum-less URL archives land via a process-unique rename."""
    dl_path = tmp_path / "archive"

    def fake_download(url, dest, progress=None, **kwargs):
        Path(dest).write_bytes(b"data")

    with patch(
        "esphome.framework_helpers.download_with_resume", side_effect=fake_download
    ):
        pf._uri_fetch_job("https://x/a.zip", dl_path, 4)(lambda done: None)
    assert dl_path.read_bytes() == b"data"
    # only the archive and the lock file remain; no orphaned staging file
    leftovers = {f.name for f in tmp_path.iterdir()}
    assert leftovers == {dl_path.name, f"{dl_path.name}.prefetch.lock"}


def test_uri_fetch_job_skips_when_another_process_won(tmp_path: Path) -> None:
    dl_path = tmp_path / "archive"
    dl_path.write_bytes(b"done")
    with patch("esphome.framework_helpers.download_with_resume") as mock_download:
        pf._uri_fetch_job("https://x/a.zip", dl_path, 4)(lambda done: None)
    mock_download.assert_not_called()
    assert dl_path.read_bytes() == b"done"


def test_registry_jobs_one_bad_spec_keeps_the_rest(tmp_path: Path) -> None:
    """A flaky resolution counts as failed without discarding the batch."""
    m = _fake_manager(tmp_path)
    m.search_registry_packages.side_effect = [
        RuntimeError("registry 500"),
        [{"any": 1}],
    ]
    with _mirror_patch():
        jobs, failed, installable = pf._registry_jobs(
            m,
            [_FakeSpec(uri=None, name="flaky"), _FakeSpec(uri=None, name="good")],
            set(),
        )
    assert failed == 1
    assert len(jobs) == 1
    assert len(installable) == 1


def test_uri_jobs_head_sizes_the_bar(tmp_path: Path) -> None:
    """HEAD sizes direct-URL specs; git and unreachable URLs are skipped."""
    m = _fake_manager(tmp_path)
    resp = MagicMock()
    resp.headers = {"content-length": "2222"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        jobs, failed, installable = pf._uri_jobs(
            m,
            [
                _FakeSpec(uri="https://x/big.zip", name="big"),
                _FakeSpec(uri="git+https://x/repo.git", name="repo"),
                _FakeSpec(uri="https://x/repo.git#v1", name="barevcs"),
                _FakeSpec(uri=None, name="registry"),
            ],
            set(),
        )
    assert failed == 0
    assert [(n, s) for n, s, _ in jobs] == [("big", 2222)]
    assert [n for n, _ in installable] == ["big"]
    # a successful HEAD with no Content-Length is a clean skip
    resp.headers = {}
    with patch("esphome.net_retry.http_request", return_value=resp):
        assert pf._uri_jobs(
            m, [_FakeSpec(uri="https://x/nolen.zip", name="nolen")], set()
        ) == ([], 0, [])


def test_uri_jobs_head_failure_counts_as_unresolved(tmp_path: Path) -> None:
    """HEAD errors and error responses both count as unresolved."""
    m = _fake_manager(tmp_path)
    with patch("esphome.net_retry.http_request", side_effect=OSError("no route")):
        assert pf._uri_jobs(m, [_FakeSpec(uri="https://x/a.zip", name="a")], set()) == (
            [],
            1,
            [],
        )
    resp = MagicMock(ok=False, status_code=404)
    resp.headers = {"content-length": "999"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        assert pf._uri_jobs(m, [_FakeSpec(uri="https://x/a.zip", name="a")], set()) == (
            [],
            1,
            [],
        )


def test_uri_jobs_dedupes_duplicate_urls(tmp_path: Path) -> None:
    """Two specs with one URL yield one HEAD and one job."""
    m = _fake_manager(tmp_path)
    resp = MagicMock()
    resp.headers = {"content-length": "5"}
    with patch("esphome.net_retry.http_request", return_value=resp) as mock_head:
        jobs, failed, _installable = pf._uri_jobs(
            m,
            [
                _FakeSpec(uri="https://x/a.zip", name="a"),
                _FakeSpec(uri="https://x/a.zip", name="a"),
            ],
            set(),
        )
    assert failed == 0
    assert len(jobs) == 1
    mock_head.assert_called_once()


def test_uri_jobs_skips_installed_cached_and_seen(tmp_path: Path) -> None:
    m = _fake_manager(tmp_path)
    m.get_package.return_value = object()
    spec = [_FakeSpec(uri="https://x/a.zip", name="a")]
    assert pf._uri_jobs(m, spec, set()) == ([], 0, [])
    m.get_package.return_value = None
    dl = Path(m.compute_download_path("https://x/a.zip", ""))
    dl.parent.mkdir(parents=True, exist_ok=True)
    dl.touch()
    # cached: no download job, but still installable
    jobs, failed, installable = pf._uri_jobs(m, spec, set())
    assert (jobs, failed) == ([], 0)
    assert [n for n, _ in installable] == ["a"]
    dl.unlink()
    # a registry job already claimed this download path
    assert pf._uri_jobs(m, spec, {str(dl)}) == ([], 0, [])


def test_prefetch_spawns_isolated_subprocess(tmp_path: Path) -> None:
    """Heal runs first, then the subprocess spawns with pio run's libdeps
    dir and the parent's PYTHONPATH preserved (the child is esphome)."""
    proc = MagicMock(returncode=0)
    order = MagicMock()
    order.run.return_value = proc
    with (
        patch(
            "esphome.platformio.toolchain.heal_platformio_python_env",
            order.heal,
        ),
        patch.object(pf.subprocess, "run", order.run) as mock_run,
        patch.dict("os.environ", {"PYTHONPATH": "/leak"}),
    ):
        pf.prefetch_platformio_packages()
    assert [c[0] for c in order.mock_calls[:2]] == ["heal", "run"]
    (cmd,), kwargs = mock_run.call_args
    assert cmd == [
        sys.executable,
        "-m",
        "esphome.platformio.prefetch",
        str(CORE.build_path),
        "testenv",
    ]
    assert kwargs["env"]["PLATFORMIO_LIBDEPS_DIR"] == str(
        CORE.relative_piolibdeps_path().absolute()
    )
    # The child is esphome itself; PYTHONPATH must survive so it imports
    # the same tree (tests/integration pins the source tree through it)
    assert kwargs["env"]["PYTHONPATH"] == "/leak"
    assert kwargs["timeout"] == pf._PREFETCH_TIMEOUT


@pytest.mark.parametrize(
    ("run_effect", "expected"),
    [
        (
            {"side_effect": pf.subprocess.TimeoutExpired("cmd", pf._PREFETCH_TIMEOUT)},
            "prefetch timed out",
        ),
        ({"return_value": MagicMock(returncode=3)}, "prefetch skipped (exit 3)"),
        ({"side_effect": OSError("no exec")}, "PlatformIO package prefetch skipped"),
    ],
)
def test_prefetch_spawn_failures_warn_and_continue(
    caplog: pytest.LogCaptureFixture, run_effect, expected
) -> None:
    """Timeouts, nonzero exits, and spawn failures each warn, never raise."""
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "run", **run_effect),
    ):
        pf.prefetch_platformio_packages()
    assert expected in caplog.text


def test_main_guards_and_exits_zero(caplog: pytest.LogCaptureFixture) -> None:
    """The subprocess entry never propagates a failure exit code."""
    with patch.object(pf, "_prefetch", side_effect=RuntimeError("boom")):
        assert pf.main(["/b", "testenv"]) == 0
    assert "PlatformIO package prefetch skipped" in caplog.text


def test_main_runs_prefetch(tmp_path: Path) -> None:
    with patch.object(pf, "_prefetch") as mock_prefetch:
        assert pf.main([str(tmp_path), "testenv"]) == 0
    mock_prefetch.assert_called_once_with(tmp_path, "testenv")


def test_main_bad_argv_is_a_distinct_exit(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A parent/child wiring bug must not look like a network failure."""
    with patch.object(pf, "_prefetch") as mock_prefetch:
        assert pf.main(["only-one"]) == 2
    mock_prefetch.assert_not_called()
    assert "prefetch usage" in caplog.text


def _write_ini(tmp_path: Path, body: str) -> None:
    (tmp_path / "platformio.ini").write_text(body)


def _write_valid_sentinel(tmp_path: Path, dirs: list[str]) -> None:
    (tmp_path / pf._SENTINEL_NAME).write_text(
        json.dumps({**pf._sentinel_state(tmp_path), "dirs": dirs}), encoding="utf-8"
    )


def test_prefetch_no_platform_returns(tmp_path: Path) -> None:
    _write_ini(tmp_path, "[env:testenv]\n")
    with patch.object(pf, "_registry_jobs") as mock_jobs:
        pf._prefetch(tmp_path, "testenv")
    mock_jobs.assert_not_called()


def _pio_modules(tmp_path: Path, fake_platform, fake_pm, config, lib_captures=None):
    def fake_lib_manager(storage_dir):
        if lib_captures is not None:
            lib_captures.append(storage_dir)
        return _fake_manager(tmp_path)

    modules = {
        "platformio": MagicMock(),
        "platformio.app": MagicMock(),
        "platformio.project": MagicMock(),
        "platformio.project.config": MagicMock(),
        "platformio.dependencies": SimpleNamespace(
            get_core_dependencies=lambda: {
                "tool-scons": "~4.0",
                "contrib-piohome": "~3",
            }
        ),
        "platformio.package": MagicMock(),
        "platformio.package.manager": MagicMock(),
        "platformio.package.manager.library": SimpleNamespace(
            LibraryPackageManager=fake_lib_manager
        ),
        "platformio.package.manager.platform": SimpleNamespace(
            PlatformPackageManager=lambda: fake_pm
        ),
        "platformio.package.meta": SimpleNamespace(
            PackageSpec=lambda *a, **kw: _FakeSpec(
                uri=None, name=kw.get("name") or (a[0] if a else None)
            )
        ),
        "platformio.platform": MagicMock(),
        "platformio.platform.factory": SimpleNamespace(
            PlatformFactory=SimpleNamespace(new=lambda pkg: fake_platform)
        ),
    }
    modules[
        "platformio.project.config"
    ].ProjectConfig.get_instance.return_value = config
    return modules


def _fake_config(tmp_path: Path, env_options: dict):
    config = MagicMock()
    options = {
        "libdeps_dir": str(tmp_path / "libdeps"),
        "packages_dir": str(tmp_path / "packages"),
        **env_options,
    }
    config.get.side_effect = lambda section, key, default=None: options.get(
        key, default
    )
    return config


def test_prefetch_all_cached_is_quiet_and_writes_sentinel(tmp_path: Path) -> None:
    """A no-work run neither logs nor batches, and records the sentinel."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    (tmp_path / "packages").mkdir()
    (tmp_path / "libdeps" / "testenv").mkdir(parents=True)
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(
        tmp_path, {"platform": "fake/p@1", "lib_deps": ["esphome/noise-c@1.0"]}
    )
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    with (
        patch.dict("sys.modules", modules),
        patch.object(pf, "_registry_jobs", return_value=([], 0, [])),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
        patch.object(pf, "run_batch_downloads") as mock_batch,
        patch.object(pf, "_preinstall") as mock_install,
    ):
        pf._prefetch(tmp_path, "testenv")
    mock_batch.assert_not_called()
    mock_install.assert_not_called()
    assert pf._prefetch_is_warm(tmp_path)


def test_prefetch_failed_resolution_is_not_cached_as_warm(tmp_path: Path) -> None:
    """A registry outage must not write the sentinel."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    (tmp_path / "packages").mkdir()
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    with (
        patch.dict("sys.modules", modules),
        patch.object(pf, "_registry_jobs", return_value=([], 1, [])),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
        patch.object(pf, "run_batch_downloads") as mock_batch,
    ):
        pf._prefetch(tmp_path, "testenv")
    mock_batch.assert_not_called()
    assert not (tmp_path / pf._SENTINEL_NAME).exists()


def test_sentinel_invalidation(tmp_path: Path) -> None:
    """Ini changes, missing dirs, and garbage sentinels all read as cold."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    pkg_dir = tmp_path / "packages"
    pkg_dir.mkdir()
    assert not pf._prefetch_is_warm(tmp_path)  # no sentinel yet
    _write_valid_sentinel(tmp_path, [str(pkg_dir)])
    assert pf._prefetch_is_warm(tmp_path)
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@2\n")
    assert not pf._prefetch_is_warm(tmp_path)  # ini changed
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    pkg_dir.rmdir()
    assert not pf._prefetch_is_warm(tmp_path)  # recorded dir gone
    (tmp_path / pf._SENTINEL_NAME).write_text("not json", encoding="utf-8")
    assert not pf._prefetch_is_warm(tmp_path)


def test_prefetch_warm_sentinel_skips_spawn(tmp_path: Path) -> None:
    """A valid sentinel skips the subprocess entirely."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    pkg_dir = tmp_path / "packages"
    pkg_dir.mkdir()
    _write_valid_sentinel(tmp_path, [str(pkg_dir)])
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "run") as mock_run,
    ):
        pf.prefetch_platformio_packages()
    mock_run.assert_not_called()


def test_prefetch_end_to_end_wiring(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Platform installs dep-free, non-optional packages plus tool-scons
    resolve, libraries use the env libdeps dir, a platform sys.path rewrite
    is undone, and failures warn by name."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/platform@1.0\n")
    fake_platform = MagicMock()
    fake_platform.packages = {
        "toolchain-x": {"optional": False},
        "framework-y": {"optional": True},
    }
    fake_platform.get_package_spec.side_effect = lambda name: _FakeSpec(
        uri=None, name=name
    )
    # Platform setup code rewrites sys.path (pioarduino penv); _prefetch
    # must restore it
    bogus = str(tmp_path / "penv-site-packages")
    fake_platform.configure_project_packages.side_effect = lambda env, targets: (
        sys.path.insert(0, bogus)
    )
    fake_pm = MagicMock()
    config = _fake_config(
        tmp_path,
        {
            "platform": "fake/platform@1.0",
            "lib_deps": ["esphome/noise-c@1.0", "${common.lib_deps}"],
        },
    )
    lib_dirs: list[str] = []
    modules = _pio_modules(tmp_path, fake_platform, fake_pm, config, lib_dirs)
    (tmp_path / pf._SENTINEL_NAME).write_text("{}", encoding="utf-8")
    captured: dict = {}

    def fake_registry_jobs(manager, specs, seen):
        captured.setdefault("spec_batches", []).append([s.name for s in specs])
        return (
            [("toolchain-x@1", 10, lambda t: None)],
            0,
            [("toolchain-x@1", specs[0])],
        )

    with (
        patch.dict("sys.modules", modules),
        patch.object(pf, "_registry_jobs", side_effect=fake_registry_jobs),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
        patch.object(
            pf,
            "run_batch_downloads",
            return_value=[("toolchain-x@1", OSError("down"))],
        ) as mock_batch,
        patch.object(pf, "_preinstall") as mock_install,
    ):
        pf._prefetch(tmp_path, "testenv")
    fake_pm.install.assert_called_once_with("fake/platform@1.0", skip_dependencies=True)
    assert not (tmp_path / pf._SENTINEL_NAME).exists()  # stale sentinel removed
    fake_platform.configure_project_packages.assert_called_once_with("testenv", ["run"])
    assert bogus not in sys.path
    # non-optional platform package + tool-scons (never piohome), then libs
    assert captured["spec_batches"][0] == ["toolchain-x", "tool-scons"]
    assert captured["spec_batches"][1] == ["esphome/noise-c@1.0"]
    assert lib_dirs == [str(Path(tmp_path / "libdeps") / "testenv")]
    mock_batch.assert_called_once()
    assert "Could not prefetch toolchain-x@1" in caplog.text
    # every installable failed its download; nothing to pre-install
    mock_install.assert_not_called()


def test_prefetch_installs_cached_archives_without_downloads(
    tmp_path: Path,
) -> None:
    """Archives already in the download cache still pre-install (in
    parallel) even when there is nothing to download, and no sentinel is
    written until everything is installed."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    spec = _FakeSpec(uri=None, name="cachedpkg")
    with (
        patch.dict("sys.modules", modules),
        patch.object(
            pf,
            "_registry_jobs",
            side_effect=[([], 0, [("cachedpkg@1", spec)]), ([], 0, [])],
        ),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
        patch.object(pf, "run_batch_downloads") as mock_batch,
        patch.object(pf, "_preinstall") as mock_install,
    ):
        pf._prefetch(tmp_path, "testenv")
    mock_batch.assert_not_called()
    assert mock_install.call_count == 1
    assert mock_install.call_args[0][1] == [spec]
    assert not (tmp_path / pf._SENTINEL_NAME).exists()


def test_preinstall_extracts_in_parallel_under_one_lock(tmp_path: Path) -> None:
    """The manager lock wraps the whole batch; per-thread managers share
    its package dir; one failing install leaves the rest alone."""
    m = _fake_manager(tmp_path)
    installed: list[str] = []

    def fake_install(spec, skip_dependencies):
        # Dependencies must be skipped: a shared dep extracted from two
        # threads would race one destination dir
        assert skip_dependencies is True
        if spec.name == "bad":
            raise RuntimeError("corrupt archive")
        installed.append(spec.name)

    m._install.side_effect = fake_install
    specs = [
        _FakeSpec(uri=None, name="a"),
        _FakeSpec(uri=None, name="bad"),
        _FakeSpec(uri=None, name="b"),
    ]
    pf._preinstall(m, specs)
    assert sorted(installed) == ["a", "b"]
    m.lock.assert_called_once_with()
    m.unlock.assert_called_once_with()
    m.memcache_reset.assert_called_once_with()


def test_preinstall_all_failed_warns_once(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Every install failing is a systemic fault, not archive noise."""
    m = _fake_manager(tmp_path)
    m._install.side_effect = AttributeError("_install went away")
    pf._preinstall(m, [_FakeSpec(uri=None, name="a"), _FakeSpec(uri=None, name="b")])
    assert "Could not pre-install any of 2" in caplog.text


def test_preinstall_dedupes_names_across_entries(tmp_path: Path) -> None:
    """Two entries with one name install once (one destination dir)."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    s1 = _FakeSpec(uri=None, name="dup")
    s2 = _FakeSpec(uri=None, name="dup")
    with (
        patch.dict("sys.modules", modules),
        patch.object(
            pf,
            "_registry_jobs",
            side_effect=[([], 0, [("pkg@1", s1), ("pkg@1", s2)]), ([], 0, [])],
        ),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
        patch.object(pf, "_preinstall") as mock_install,
    ):
        pf._prefetch(tmp_path, "testenv")
    mock_install.assert_called_once()
    assert mock_install.call_args[0][1] == [s1]


def test_preinstall_unlocks_even_when_pool_fails(tmp_path: Path) -> None:
    m = _fake_manager(tmp_path)
    with (
        patch.object(pf, "ThreadPoolExecutor", side_effect=RuntimeError("no threads")),
        pytest.raises(RuntimeError),
    ):
        pf._preinstall(m, [_FakeSpec(uri=None, name="a")])
    m.unlock.assert_called_once_with()


def test_prefetch_skips_duplicate_tool_scons(tmp_path: Path) -> None:
    """A platform that lists tool-scons itself does not get it appended."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    fake_platform = MagicMock()
    fake_platform.packages = {"tool-scons": {"optional": False}}
    fake_platform.get_package_spec.side_effect = lambda name: _FakeSpec(
        uri=None, name=name
    )
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    batches: list[list[str]] = []
    with (
        patch.dict("sys.modules", modules),
        patch.object(
            pf,
            "_registry_jobs",
            side_effect=lambda mgr, specs, seen: (
                batches.append([s.name for s in specs]) or ([], 0, [])
            ),
        ),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
    ):
        pf._prefetch(tmp_path, "testenv")
    assert batches[0] == ["tool-scons"]


def test_platformio_private_api_contract() -> None:
    """The pinned PlatformIO still exposes what the pre-install drives.

    Everything else in this module mocks the managers, so this is the one
    test that fails loudly when a requirements bump changes the private
    surface instead of silently degrading the prefetch to a no-op.
    """
    import inspect

    from platformio.package.manager._install import PackageManagerInstallMixin
    from platformio.package.manager.base import BasePackageManager
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.platform import PlatformPackageManager
    from platformio.package.manager.tool import ToolPackageManager

    params = inspect.signature(PackageManagerInstallMixin._install).parameters
    assert "spec" in params
    assert "skip_dependencies" in params
    for cls in (ToolPackageManager, LibraryPackageManager, PlatformPackageManager):
        assert "package_dir" in inspect.signature(cls.__init__).parameters
    for name in (
        "lock",
        "unlock",
        "memcache_reset",
        "get_package",
        "compute_download_path",
    ):
        assert callable(getattr(BasePackageManager, name))
