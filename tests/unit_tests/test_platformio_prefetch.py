"""Tests for the parallel PlatformIO package prefetch."""

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
    """PackageSpec stand-in: .uri and .name are all the prefetch reads."""


def _fake_manager(tmp_path: Path) -> MagicMock:
    m = MagicMock()
    m.__class__ = lambda: m  # _resolve constructs a same-class instance
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
    """A registry spec resolves to a job keyed by the mirror URL and
    checksum, sized from the registry file entry."""
    m = _fake_manager(tmp_path)
    with _mirror_patch():
        jobs = pf._registry_jobs(
            m, [_FakeSpec(uri=None, name="toolchain-xtensa")], set()
        )
    assert len(jobs) == 1
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
        jobs = pf._registry_jobs(m, [_FakeSpec(uri=None, name="x")], set())
    assert jobs == []


def test_registry_jobs_skips_cached_and_sizeless(tmp_path: Path) -> None:
    """A file already in PlatformIO's cache, or one the registry reports no
    size for, is left to PlatformIO."""
    m = _fake_manager(tmp_path)
    dl = Path(m.compute_download_path("https://mirror.example/t.tar.gz", "beef"))
    dl.parent.mkdir(parents=True, exist_ok=True)
    dl.touch()
    with _mirror_patch():
        assert pf._registry_jobs(m, [_FakeSpec(uri=None, name="x")], set()) == []
    dl.unlink()
    m.find_best_registry_version.return_value[1]["files"][0]["size"] = 0
    with _mirror_patch():
        assert pf._registry_jobs(m, [_FakeSpec(uri=None, name="x")], set()) == []


def test_registry_jobs_dedupes_download_paths(tmp_path: Path) -> None:
    """Duplicate specs resolving to one archive yield a single job; two
    batch workers must never share a .part file."""
    m = _fake_manager(tmp_path)
    specs = [_FakeSpec(uri=None, name="dup"), _FakeSpec(uri=None, name="dup")]
    with _mirror_patch():
        jobs = pf._registry_jobs(m, specs, set())
    assert len(jobs) == 1


def test_registry_jobs_uri_specs_excluded(tmp_path: Path) -> None:
    """URL specs never reach the registry resolution."""
    m = _fake_manager(tmp_path)
    assert (
        pf._registry_jobs(m, [_FakeSpec(uri="https://x/y.zip", name="y")], set()) == []
    )
    m.search_registry_packages.assert_not_called()


def test_uri_jobs_head_sizes_the_bar(tmp_path: Path) -> None:
    """A direct-URL spec gets its size from a HEAD; git specs and
    unreachable URLs are left to PlatformIO."""
    m = _fake_manager(tmp_path)
    resp = MagicMock()
    resp.headers = {"content-length": "2222"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        jobs = pf._uri_jobs(
            m,
            [
                _FakeSpec(uri="https://x/big.zip", name="big"),
                _FakeSpec(uri="git+https://x/repo.git", name="repo"),
                _FakeSpec(uri=None, name="registry"),
            ],
            set(),
        )
    assert [(n, s) for n, s, _ in jobs] == [("big", 2222)]


def test_uri_jobs_head_failure_skips(tmp_path: Path) -> None:
    m = _fake_manager(tmp_path)
    with patch("esphome.net_retry.http_request", side_effect=OSError("no route")):
        assert (
            pf._uri_jobs(m, [_FakeSpec(uri="https://x/a.zip", name="a")], set()) == []
        )


def test_uri_jobs_skips_installed_cached_and_seen(tmp_path: Path) -> None:
    m = _fake_manager(tmp_path)
    m.get_package.return_value = object()
    assert pf._uri_jobs(m, [_FakeSpec(uri="https://x/a.zip", name="a")], set()) == []
    m.get_package.return_value = None
    dl = Path(m.compute_download_path("https://x/a.zip", ""))
    dl.parent.mkdir(parents=True, exist_ok=True)
    dl.touch()
    assert pf._uri_jobs(m, [_FakeSpec(uri="https://x/a.zip", name="a")], set()) == []
    dl.unlink()
    # a registry job already claimed this download path
    assert (
        pf._uri_jobs(m, [_FakeSpec(uri="https://x/a.zip", name="a")], {str(dl)}) == []
    )


def test_resume_fetch_job_threads_tracker(tmp_path: Path) -> None:
    """The batch runner passes the tracker positionally; the shared adapter
    must deliver it as download_with_resume's progress keyword."""
    from esphome.framework_helpers import resume_fetch_job

    with patch("esphome.framework_helpers.download_with_resume") as mock_download:
        fetch = resume_fetch_job("https://x/a.zip", tmp_path / "a", sha256="ff", size=9)
        tracker = lambda done: None  # noqa: E731
        fetch(tracker)
    mock_download.assert_called_once_with(
        "https://x/a.zip", tmp_path / "a", progress=tracker, sha256="ff", size=9
    )


def test_warn_prefetch_failures_names_each_failure(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The shared failure loop warns per job with the failure reason."""
    from esphome.framework_helpers import warn_prefetch_failures

    warn_prefetch_failures([("toolchain-x@1", OSError("down"))])
    assert "Could not prefetch toolchain-x@1: down" in caplog.text
    warn_prefetch_failures([("lib", OSError("gone"))], "Prefetch of %s failed: %s")
    assert "Prefetch of lib failed: gone" in caplog.text


def test_prefetch_spawns_isolated_subprocess(tmp_path: Path) -> None:
    """The prefetch runs in a subprocess (platform setup code may rewrite
    the interpreter's sys.path) with pio run's libdeps dir."""
    proc = MagicMock(returncode=0)
    with patch.object(pf.subprocess, "run", return_value=proc) as mock_run:
        pf.prefetch_platformio_packages()
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


def test_prefetch_nonzero_exit_warns(caplog: pytest.LogCaptureFixture) -> None:
    proc = MagicMock(returncode=3)
    with patch.object(pf.subprocess, "run", return_value=proc):
        pf.prefetch_platformio_packages()
    assert "prefetch skipped (exit 3)" in caplog.text


def test_prefetch_launch_failure_never_raises(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Any spawn failure is one warning, never a build failure."""
    with patch.object(pf.subprocess, "run", side_effect=OSError("no exec")):
        pf.prefetch_platformio_packages()
    assert "PlatformIO package prefetch skipped" in caplog.text


def test_main_guards_and_exits_zero(caplog: pytest.LogCaptureFixture) -> None:
    """The subprocess entry never propagates a failure exit code."""
    with patch.object(pf, "_prefetch", side_effect=RuntimeError("boom")):
        assert pf.main(["/b", "testenv"]) == 0
    assert "PlatformIO package prefetch skipped" in caplog.text


def test_main_runs_prefetch(tmp_path: Path) -> None:
    with patch.object(pf, "_prefetch") as mock_prefetch:
        assert pf.main([str(tmp_path), "testenv"]) == 0
    mock_prefetch.assert_called_once_with(tmp_path, "testenv")


def _write_ini(tmp_path: Path, body: str) -> None:
    (tmp_path / "platformio.ini").write_text(body)


def test_prefetch_no_platform_returns(tmp_path: Path) -> None:
    _write_ini(tmp_path, "[env:testenv]\n")
    with patch.object(pf, "_registry_jobs") as mock_jobs:
        pf._prefetch(tmp_path, "testenv")
    mock_jobs.assert_not_called()


def _pio_modules(tmp_path: Path, fake_platform, fake_pm, config, lib_captures=None):
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
            LibraryPackageManager=lambda storage_dir: (
                lib_captures.append(storage_dir) if lib_captures is not None else None,
                _fake_manager(tmp_path),
            )[1]
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
    options = {"libdeps_dir": str(tmp_path / "libdeps"), **env_options}
    config.get.side_effect = lambda section, key, default=None: options.get(
        key, default
    )
    return config


def test_prefetch_all_cached_is_quiet(tmp_path: Path) -> None:
    """With nothing to download the prefetch neither logs nor batches."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    with (
        patch.dict("sys.modules", modules),
        patch.object(pf, "_registry_jobs", return_value=[]),
        patch.object(pf, "_uri_jobs", return_value=[]),
        patch.object(pf, "run_batch_downloads") as mock_batch,
    ):
        pf._prefetch(tmp_path, "testenv")
    mock_batch.assert_not_called()


def test_prefetch_end_to_end_wiring(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Platform installs dep-free, the env is configured, non-optional
    packages plus tool-scons resolve, libraries resolve against the env's
    libdeps dir, a platform sys.path rewrite is undone, and failures warn
    by name."""
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
    captured: dict = {}

    def fake_registry_jobs(manager, specs, seen):
        captured.setdefault("spec_batches", []).append([s.name for s in specs])
        return [("toolchain-x@1", 10, lambda t: None)]

    with (
        patch.dict("sys.modules", modules),
        patch.object(pf, "_registry_jobs", side_effect=fake_registry_jobs),
        patch.object(pf, "_uri_jobs", return_value=[]),
        patch.object(
            pf,
            "run_batch_downloads",
            return_value=[("toolchain-x@1", OSError("down"))],
        ) as mock_batch,
    ):
        pf._prefetch(tmp_path, "testenv")
    fake_pm.install.assert_called_once_with("fake/platform@1.0", skip_dependencies=True)
    fake_platform.configure_project_packages.assert_called_once_with("testenv", ["run"])
    assert bogus not in sys.path
    # non-optional platform package + tool-scons (never piohome), then libs
    assert captured["spec_batches"][0] == ["toolchain-x", "tool-scons"]
    assert captured["spec_batches"][1] == ["esphome/noise-c@1.0"]
    assert lib_dirs == [str(Path(tmp_path / "libdeps") / "testenv")]
    mock_batch.assert_called_once()
    assert "Could not prefetch toolchain-x@1" in caplog.text


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
                batches.append([s.name for s in specs]) or []
            ),
        ),
        patch.object(pf, "_uri_jobs", return_value=[]),
    ):
        pf._prefetch(tmp_path, "testenv")
    assert batches[0] == ["tool-scons"]
