"""Tests for the parallel PlatformIO package prefetch."""

import errno
import inspect
import json
import logging
import os
from pathlib import Path
import signal
import sys
import threading
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from filelock import Timeout
from platformio.package.manager._install import PackageManagerInstallMixin
from platformio.package.manager.base import BasePackageManager
from platformio.package.manager.library import LibraryPackageManager
from platformio.package.manager.platform import PlatformPackageManager
from platformio.package.manager.tool import ToolPackageManager
from platformio.package.meta import PackageCompatibility, PackageSpec
import pytest

from esphome.core import CORE
import esphome.platformio.prefetch as pf


@pytest.fixture(autouse=True)
def _core(tmp_path: Path):
    CORE.reset()
    CORE.build_path = str(tmp_path)
    CORE.name = "testenv"
    saved_bar = os.environ.get("PLATFORMIO_DISABLE_PROGRESSBAR")
    saved_sigterm = signal.getsignal(signal.SIGTERM)
    pio_loggers = ("Tool Manager", "Library Manager", "Platform Manager")
    saved_propagate = {n: pf.logging.getLogger(n).propagate for n in pio_loggers}
    saved_filters = {n: list(pf.logging.getLogger(n).filters) for n in pio_loggers}
    # The real setup_log would swap pytest's root-handler formatter
    with patch("esphome.log.setup_log"):
        yield
    # _preinstall and main() set these process-wide; keep the suite hermetic
    if saved_bar is None:
        os.environ.pop("PLATFORMIO_DISABLE_PROGRESSBAR", None)
    else:
        os.environ["PLATFORMIO_DISABLE_PROGRESSBAR"] = saved_bar
    signal.signal(signal.SIGTERM, saved_sigterm)
    for n, flag in saved_propagate.items():
        pf.logging.getLogger(n).propagate = flag
        pf.logging.getLogger(n).filters[:] = saved_filters[n]
    CORE.reset()


class _FakeSpec(SimpleNamespace):
    """PackageSpec stand-in for the attributes the prefetch reads."""

    def __init__(
        self,
        *,
        uri=None,
        owner=None,
        requirements=None,
        external=False,
        custom_name=False,
        **kwargs,
    ) -> None:
        super().__init__(
            uri=uri, owner=owner, requirements=requirements, external=external, **kwargs
        )
        self._custom_name = custom_name

    def has_custom_name(self) -> bool:
        return self._custom_name


def _fake_manager(tmp_path: Path) -> MagicMock:
    m = MagicMock()
    # _resolve and _preinstall construct same-class instances
    m.__class__ = lambda package_dir=None, **kwargs: m
    m.get_package.return_value = None
    m.compatibility = None
    m.is_builtin_lib.return_value = False
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
            m, [_FakeSpec(name="toolchain-xtensa")], set()
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
        assert pf._registry_jobs(m, [_FakeSpec(name="x")], set()) == (
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
        jobs, failed, installable = pf._registry_jobs(m, [_FakeSpec(name="x")], set())
    assert (jobs, failed) == ([], 0)
    assert [n for n, _ in installable] == ["toolchain-xtensa@2.0.0"]
    dl.unlink()
    m.find_best_registry_version.return_value[1]["files"][0]["size"] = 0
    with _mirror_patch():
        assert pf._registry_jobs(m, [_FakeSpec(name="x")], set()) == (
            [],
            0,
            [],
        )


def test_registry_jobs_dedupes_download_paths(tmp_path: Path) -> None:
    """Duplicate specs resolve once and one archive yields one job (two
    workers must never share a .part); nine specs against eight workers
    also exercise the thread-local manager reuse."""
    m = _fake_manager(tmp_path)
    specs = [_FakeSpec(name="dup"), _FakeSpec(name="dup")]
    specs += [_FakeSpec(name=f"n{i}") for i in range(8)]
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
        _FakeSpec(name="framework-x", owner="platformio"),
        _FakeSpec(name="framework-x", owner="pioarduino"),
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
            [_FakeSpec(name="a"), _FakeSpec(name="b")],
            set(),
        )
    assert (jobs, failed, installable) == ([], 2, [])
    # The aggregate warning names a cause so an API break does not read
    # as a registry outage
    assert "Could not resolve 2 of 2" in caplog.text
    assert "registry down" in caplog.text


def test_uri_fetch_job_promotes_atomically(tmp_path: Path) -> None:
    """Checksum-less URL archives land via a locked staging file and an
    atomic rename (the stable name is what keeps .part resume working)."""
    dl_path = tmp_path / "archive"

    def fake_download(url, dest, progress=None, **kwargs):
        Path(dest).write_bytes(b"data")

    manager = MagicMock()
    with patch(
        "esphome.framework_helpers.download_with_resume", side_effect=fake_download
    ):
        pf._uri_fetch_job(manager, "https://x/a.zip", dl_path, 4)(lambda done: None)
    assert dl_path.read_bytes() == b"data"
    # The archive is handed to pio's usage.db pruner
    manager.set_download_utime.assert_called_once_with(str(dl_path))
    # no orphaned staging file; the lock file may or may not persist
    # (filelock removes it on release on some platforms)
    leftovers = {f.name for f in tmp_path.iterdir()}
    assert leftovers - {f"{dl_path.name}.prefetch.lock"} == {dl_path.name}


def test_registry_fetch_job_skips_when_cached(tmp_path: Path) -> None:
    """A destination another process completed is not re-downloaded."""
    dl_path = tmp_path / "archive"
    dl_path.write_bytes(b"done")
    with patch("esphome.framework_helpers.download_with_resume") as mock_download:
        pf._registry_fetch_job(
            MagicMock(), "https://x/a.tar.gz", dl_path, "ab" * 32, 4
        )(lambda done: None)
    mock_download.assert_not_called()
    assert dl_path.read_bytes() == b"done"


def test_registry_fetch_job_downloads_under_lock(tmp_path: Path) -> None:
    """Registry downloads write the shared cache path under the same lock
    the URL path uses; interleaved writers would corrupt the archive."""
    dl_path = tmp_path / "archive"
    order: list[str] = []
    with (
        patch(
            "esphome.framework_helpers.download_with_resume",
            side_effect=lambda url, dest, progress=None, **kw: (
                order.append("fetch"),
                Path(dest).write_bytes(b"data"),  # registration needs a real file
            ),
        ),
        patch(
            "filelock.FileLock.acquire",
            side_effect=lambda *a, **k: order.append("lock"),
        ),
        patch(
            "filelock.FileLock.release",
            side_effect=lambda *a, **k: order.append("unlock"),
        ),
    ):
        manager = MagicMock()
        pf._registry_fetch_job(manager, "https://x/a.tar.gz", dl_path, "ab" * 32, 4)(
            lambda done: None
        )
    # FileLock.__del__ may add a trailing release; the contract is the order
    assert order[:2] == ["lock", "fetch"]
    assert "unlock" in order[2:]
    manager.set_download_utime.assert_called_once_with(str(dl_path))


def test_lockless_filesystem_downloads_unlocked(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A filesystem without lock support (ENOSYS/EPERM) degrades to an
    unlocked download with one warning, never a per-package failure."""
    dl_path = tmp_path / "archive"
    with (
        patch("esphome.framework_helpers.download_with_resume") as mock_download,
        patch(
            "filelock.FileLock.acquire",
            side_effect=OSError(errno.ENOSYS, "no locks"),
        ),
        patch("filelock.FileLock.release"),
    ):
        pf._registry_fetch_job(
            MagicMock(), "https://x/a.tar.gz", dl_path, "ab" * 32, 4
        )(lambda done: None)
    mock_download.assert_called_once()
    assert "downloading unlocked" in caplog.text


def test_uri_fetch_job_failed_download_keeps_staging(tmp_path: Path) -> None:
    """A failed fetch keeps the .part staging bytes for the next resume."""
    dl_path = tmp_path / "archive"
    part = tmp_path / "archive.prefetch.part"
    part.write_bytes(b"partial")
    with (
        patch(
            "esphome.framework_helpers.download_with_resume",
            side_effect=OSError("network gone"),
        ),
        pytest.raises(OSError, match="network gone"),
    ):
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 4)(lambda done: None)
    assert part.read_bytes() == b"partial"
    assert not dl_path.exists()


def test_uri_fetch_job_rejects_wrong_length(tmp_path: Path) -> None:
    """A checksum-less body of the wrong length is never published under a
    cache key pio would trust forever."""
    dl_path = tmp_path / "archive"

    def fake_download(url, dest, progress=None, **kwargs):
        Path(dest).write_bytes(b"short")

    with (
        patch(
            "esphome.framework_helpers.download_with_resume", side_effect=fake_download
        ),
        pytest.raises(ValueError, match="expected 9999 bytes"),
    ):
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 9999)(
            lambda done: None
        )
    assert not dl_path.exists()
    assert not (tmp_path / "archive.prefetch").exists()


def test_sweep_stale_sidecars(tmp_path: Path) -> None:
    """Sidecars past pio's own expiry are pruned; fresh and foreign files
    stay."""
    old_time = pf.time.time() - 110
    stale = tmp_path / "a.tar.gz.part"
    stale.write_bytes(b"x")
    os.utime(stale, (old_time, old_time))
    fresh = tmp_path / "b.tar.gz.part"
    fresh.write_bytes(b"x")
    keep = tmp_path / "c.tar.gz"
    keep.write_bytes(b"x")
    os.utime(keep, (old_time, old_time))
    # A held lock can carry an ancient mtime (O_TRUNC keeps it); locks
    # must never be swept or the single-writer guarantee reopens
    held_lock = tmp_path / "d.tar.gz.esphome.lock"
    held_lock.write_bytes(b"")
    os.utime(held_lock, (old_time, old_time))
    pf._sweep_stale_sidecars(tmp_path, 100)
    assert not stale.exists()
    assert fresh.exists()
    assert keep.exists()
    assert held_lock.exists()
    pf._sweep_stale_sidecars(tmp_path / "missing", 100)  # tolerated


def test_register_download_failure_leaves_a_trace(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed usage.db registration is traced; an unregistered archive
    is never pruned, so silence would hide the leak coming back."""
    manager = MagicMock()
    manager.set_download_utime.side_effect = RuntimeError("db locked")
    with caplog.at_level(pf.logging.DEBUG):
        pf._register_download(manager, tmp_path / "a.tar.gz")
    assert "Could not register" in caplog.text


def test_sweep_logs_unprunable_files(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A sidecar that cannot be removed leaves a trace; a sweep that never
    prunes must not look like a clean sweep."""
    old_time = pf.time.time() - 110
    stale = tmp_path / "a.tar.gz.part"
    stale.write_bytes(b"x")
    os.utime(stale, (old_time, old_time))
    with (
        patch.object(Path, "unlink", side_effect=OSError("busy")),
        caplog.at_level(pf.logging.DEBUG),
    ):
        pf._sweep_stale_sidecars(tmp_path, 100)
    assert "Could not remove" in caplog.text


def test_uri_lock_failure_is_a_counted_failure(tmp_path: Path) -> None:
    """The checksum-less URL path never degrades to an unlocked shared
    write; interleaved right-length corruption would go undetected."""
    dl_path = tmp_path / "archive"
    with (
        patch("esphome.framework_helpers.download_with_resume") as mock_download,
        patch(
            "filelock.FileLock.acquire",
            side_effect=OSError(errno.ENOSYS, "no locks"),
        ),
        pytest.raises(OSError),
    ):
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 4)(lambda done: None)
    mock_download.assert_not_called()


def test_uri_fetch_job_no_discard_without_a_file(tmp_path: Path) -> None:
    """When no archive landed (degraded serialized run), the staging bytes
    stay for the next resume instead of being discarded."""
    dl_path = tmp_path / "archive"
    part = tmp_path / "archive.prefetch.part"
    part.write_bytes(b"partial")
    with patch.object(pf, "_serialized_fetch_job", return_value=lambda tracker: None):
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 4)(lambda done: None)
    assert part.read_bytes() == b"partial"


def test_uri_fetch_job_waits_out_a_briefly_held_lock(tmp_path: Path) -> None:
    """A lock freed within the deadline lets the job proceed normally."""
    dl_path = tmp_path / "archive"

    def fake_download(url, dest, progress=None, **kwargs):
        Path(dest).write_bytes(b"data")

    with (
        patch(
            "esphome.framework_helpers.download_with_resume", side_effect=fake_download
        ),
        patch("filelock.FileLock.acquire", side_effect=[Timeout("held"), None]),
        patch("filelock.FileLock.release"),
    ):
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 4)(lambda done: None)
    assert dl_path.read_bytes() == b"data"


def test_lock_deadline_leaves_download_to_the_holder(tmp_path: Path) -> None:
    """A lock held past the deadline means another process is fetching the
    same file; skipping cleanly beats a misleading failure warning. The
    tracker is still polled so a parked worker observes cancellation."""
    dl_path = tmp_path / "archive"
    ticks: list[int] = []
    with (
        patch("esphome.framework_helpers.download_with_resume") as mock_download,
        patch("filelock.FileLock.acquire", side_effect=Timeout("held")),
        patch.object(pf, "_DOWNLOAD_LOCK_TIMEOUT", 0),
    ):
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 4)(ticks.append)
    mock_download.assert_not_called()
    assert ticks == [0]
    assert not dl_path.exists()


def test_registry_lock_deadline_skips_registration(tmp_path: Path) -> None:
    """A registry job that lost the download race to another process
    must not stamp a nonexistent archive into pio's usage.db."""
    manager = MagicMock()
    dl_path = tmp_path / "archive"
    with (
        patch("esphome.framework_helpers.download_with_resume") as mock_download,
        patch("filelock.FileLock.acquire", side_effect=Timeout("held")),
        patch.object(pf, "_DOWNLOAD_LOCK_TIMEOUT", 0),
    ):
        pf._registry_fetch_job(manager, "https://x/a.tar.gz", dl_path, "ab" * 32, 4)(
            lambda done: None
        )
    mock_download.assert_not_called()
    manager.set_download_utime.assert_not_called()


def test_main_interrupt_exits_quietly(tmp_path: Path) -> None:
    """Ctrl-C reaches the child via the shared process group; it must exit
    without a traceback."""
    with (
        patch("esphome.log.setup_log"),
        patch.object(pf, "_prefetch", side_effect=KeyboardInterrupt),
    ):
        assert pf.main([str(tmp_path), "testenv"]) == 130


def test_main_bad_log_level_falls_back(tmp_path: Path) -> None:
    with (
        patch.dict("os.environ", {"ESPHOME_PREFETCH_LOG_LEVEL": "verbose"}),
        patch("esphome.log.setup_log") as mock_setup,
        patch.object(pf, "_prefetch"),
    ):
        assert pf.main([str(tmp_path), "testenv"]) == 0
    assert mock_setup.call_args[0][0] == pf.logging.INFO


def test_main_silences_pio_manager_propagation(tmp_path: Path) -> None:
    """The pio manager loggers carry their own handler; propagation to
    the root handler would print every install line twice."""
    with patch("esphome.log.setup_log"), patch.object(pf, "_prefetch"):
        assert pf.main([str(tmp_path), "testenv"]) == 0
    for name in ("Tool Manager", "Library Manager", "Platform Manager"):
        assert pf.logging.getLogger(name).propagate is False


def test_main_quiet_level_reaches_pio_manager_loggers(tmp_path: Path) -> None:
    """Manager construction re-pins its logger to INFO, so a quiet run
    needs the logger-level filter to keep per-package lines out."""
    with (
        patch.dict("os.environ", {"ESPHOME_PREFETCH_LOG_LEVEL": "30"}),
        patch("esphome.log.setup_log"),
        patch.object(pf, "_prefetch"),
    ):
        assert pf.main([str(tmp_path), "testenv"]) == 0
    lib_logger = pf.logging.getLogger("Library Manager")
    lib_logger.setLevel(pf.logging.INFO)  # what pio's _setup_logger does
    info = pf.logging.LogRecord("Library Manager", 20, __file__, 1, "x", (), None)
    warning = pf.logging.LogRecord("Library Manager", 30, __file__, 1, "x", (), None)
    # Logger.filter returns falsy to drop, the record itself to pass
    assert not lib_logger.filter(info)
    assert lib_logger.filter(warning)


def test_main_mirrors_parent_log_setup(tmp_path: Path) -> None:
    """The child adopts the parent's dashboard flag and log formatter so
    its warnings and progress bar match the parent's."""
    with (
        patch.dict(
            "os.environ",
            {"ESPHOME_PREFETCH_LOG_LEVEL": "30", "ESPHOME_PREFETCH_DASHBOARD": "1"},
        ),
        patch("esphome.log.setup_log") as mock_setup,
        patch.object(pf, "_prefetch"),
    ):
        assert pf.main([str(tmp_path), "testenv"]) == 0
    mock_setup.assert_called_once_with(30)
    assert CORE.dashboard is True


def test_uri_fetch_job_skips_when_another_process_won(tmp_path: Path) -> None:
    """A lost race discards the staging files; the cache never prunes them."""
    dl_path = tmp_path / "archive"
    dl_path.write_bytes(b"done")
    stale = [
        tmp_path / "archive.prefetch",
        tmp_path / "archive.prefetch.part",
        tmp_path / "archive.prefetch.part.meta",
    ]
    for f in stale:
        f.write_bytes(b"stale")
    with patch("esphome.framework_helpers.download_with_resume") as mock_download:
        pf._uri_fetch_job(MagicMock(), "https://x/a.zip", dl_path, 4)(lambda done: None)
    mock_download.assert_not_called()
    assert dl_path.read_bytes() == b"done"
    assert not any(f.exists() for f in stale)


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
            [_FakeSpec(name="flaky"), _FakeSpec(name="good")],
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
                _FakeSpec(uri="https://x/big.zip", name="big", custom_name=True),
                _FakeSpec(uri="git+https://x/repo.git", name="repo"),
                _FakeSpec(uri="https://x/repo.git#v1", name="barevcs"),
                _FakeSpec(name="registry"),
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


def test_uri_jobs_head_failure_counts_as_unresolved(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Network errors and transient statuses count as unresolved (and warn);
    any permanent error status is a clean skip so the sentinel can still
    be written (pio run names a broken URL when it downloads)."""
    m = _fake_manager(tmp_path)
    spec = [_FakeSpec(uri="https://x/a.zip", name="a")]
    with patch("esphome.net_retry.http_request", side_effect=OSError("no route")):
        assert pf._uri_jobs(m, spec, set()) == ([], 1, [])
    resp = MagicMock(ok=False, status_code=503)
    resp.headers = {"content-length": "999"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        assert pf._uri_jobs(m, spec, set()) == ([], 1, [])
    # 403 is how registries rate-limit; it must not be cached as warm
    resp = MagicMock(ok=False, status_code=403)
    resp.headers = {"content-length": "999"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        assert pf._uri_jobs(m, spec, set()) == ([], 1, [])
    resp = MagicMock(ok=False, status_code=405)
    resp.headers = {"content-length": "999"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        assert pf._uri_jobs(m, spec, set()) == ([], 0, [])
    assert "HEAD https://x/a.zip" not in caplog.text
    resp = MagicMock(ok=False, status_code=404)
    resp.headers = {"content-length": "999"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        assert pf._uri_jobs(m, spec, set()) == ([], 0, [])
    assert "returned 404" not in caplog.text


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
    spec = [_FakeSpec(uri="https://x/a.zip", name="a", custom_name=True)]
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
    proc = MagicMock()
    proc.wait.return_value = 0
    order = MagicMock()
    order.run.return_value = proc
    with (
        patch(
            "esphome.platformio.toolchain.heal_platformio_python_env",
            order.heal,
        ),
        patch.object(pf.subprocess, "Popen", order.run) as mock_run,
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
    assert "ESPHOME_PREFETCH_DASHBOARD" not in kwargs["env"]
    proc.wait.assert_called_once_with(timeout=pf._PREFETCH_TIMEOUT)


def test_stop_child_windows_never_terminates() -> None:
    """The Windows TerminateProcess cannot reach the SIGTERM handler, so
    the graceful arm becomes a plain longer wait."""
    proc = MagicMock()
    proc.wait.side_effect = [pf.subprocess.TimeoutExpired("x", 1), 0]
    with patch.object(pf.sys, "platform", "win32"):
        pf._stop_child(proc)
    proc.terminate.assert_not_called()
    proc.kill.assert_not_called()


def test_stop_child_surviving_child_warns(caplog: pytest.LogCaptureFixture) -> None:
    """A child that outlives kill() may still be writing packages pio run
    trusts; that must be visible at default verbosity."""
    timeout = pf.subprocess.TimeoutExpired("cmd", 5)
    proc = MagicMock()
    proc.poll.return_value = None  # still running: the wait is announced
    proc.wait.side_effect = [timeout, timeout, timeout]
    with (
        patch.object(pf.sys, "platform", "linux"),
        caplog.at_level(pf.logging.INFO),
    ):
        pf._stop_child(proc)
    assert "Waiting for the prefetch child" in caplog.text
    assert "could not be confirmed stopped" in caplog.text


def test_dependency_entries_isolate_a_bad_manifest(tmp_path: Path) -> None:
    """One unreadable manifest skips that entry only, never the group."""
    m = _fake_manager(tmp_path)
    m.get_package.side_effect = lambda spec: (
        SimpleNamespace(spec=spec)
        if getattr(spec, "name", "") in ("bad", "good")
        else None
    )

    def deps_for(pkg):
        if pkg.spec.name == "bad":
            raise RuntimeError("manifest unreadable")
        return [{"owner": "o", "name": "dep", "version": "^1"}]

    m.get_pkg_dependencies.side_effect = deps_for
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    entries = pf._dependency_entries(
        m,
        [
            ("bad@1", _FakeSpec(name="bad")),
            ("good@1", _FakeSpec(name="good")),
        ],
        set(),
    )
    assert [name for name, *_ in entries] == ["dep"]


def test_dependency_entries_skip_nameless_spec(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A dependency whose spec has no name has no destination identity;
    the drop is diagnosable under -v."""
    m = _fake_manager(tmp_path)
    m.get_package.side_effect = lambda spec: (
        SimpleNamespace(spec=spec) if getattr(spec, "name", "") == "top" else None
    )
    m.get_pkg_dependencies.return_value = [{"owner": "o", "version": "^1"}]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=None)
    with caplog.at_level(logging.DEBUG):
        assert (
            pf._dependency_entries(m, [("top@1", _FakeSpec(name="top"))], set()) == []
        )
    assert "has no name; left to pio run" in caplog.text


def test_dependency_entries_filter_seen_names(tmp_path: Path) -> None:
    """A dependency already waved under its name is not queued again."""
    m = _fake_manager(tmp_path)
    m.get_package.side_effect = lambda spec: (
        SimpleNamespace(spec=spec) if getattr(spec, "name", "") == "top" else None
    )
    m.get_pkg_dependencies.return_value = [
        {"owner": "o", "name": "dep", "version": "^1"}
    ]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    assert pf._dependency_entries(m, [("top@1", _FakeSpec(name="top"))], {"dep"}) == []


def test_preinstall_cleanup_cannot_displace_the_inflight_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A failing unlock or cwd restore must not replace the pool's own
    exception (SIGTERM's SystemExit included) with a downgradeable one."""
    m = _fake_manager(tmp_path)
    m._install.side_effect = SystemExit(143)
    m.unlock.side_effect = RuntimeError("flock broke")
    real_chdir = pf.os.chdir
    monkeypatch.setattr(pf.os, "chdir", MagicMock(side_effect=OSError("cwd removed")))
    try:
        with pytest.raises(SystemExit):
            pf._preinstall(m, [("a@1", _FakeSpec(name="a"))])
    finally:
        monkeypatch.setattr(pf.os, "chdir", real_chdir)
    assert "Could not release the manager lock" in caplog.text


def test_preinstall_memcache_failure_leaves_a_trace(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failing cache reset warns and skips the dependency wave; the
    wave itself still completes."""
    m = _fake_manager(tmp_path)
    m.memcache_reset.side_effect = RuntimeError("cache broken")
    pf._preinstall(m, [("a@1", _FakeSpec(name="a"))])
    assert "Could not reset the storage cache" in caplog.text
    assert "Skipping the dependency wave" in caplog.text
    m.get_pkg_dependencies.assert_not_called()


def test_prefetch_wait_failure_degrades(caplog: pytest.LogCaptureFixture) -> None:
    """An unexpected wait() failure warns and continues; the prefetch must
    never become a new way for the build to fail."""
    proc = MagicMock()
    proc.wait.side_effect = [RuntimeError("wait broke"), 0]
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "Popen", return_value=proc),
    ):
        pf.prefetch_platformio_packages()
    assert "prefetch skipped" in caplog.text


def test_preinstall_stuck_lock_skips_dependency_wave(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed unlock leaves the lock state unknown; the recursive wave
    would install under a lock() that silently no-ops."""
    m = _fake_manager(tmp_path)
    m.unlock.side_effect = RuntimeError("flock broke")
    pf._preinstall(m, [("a@1", _FakeSpec(name="a"))])
    assert "Skipping the dependency wave" in caplog.text
    m.get_pkg_dependencies.assert_not_called()


def test_preinstall_lost_cwd_warns_and_skips_wave(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed cwd restore is process-global state loss: it warns and
    the rest is left to pio run from a clean process."""
    m = _fake_manager(tmp_path)
    with patch.object(pf.os, "chdir", side_effect=OSError("cwd gone")):
        pf._preinstall(m, [("a@1", _FakeSpec(name="a"))])
    assert "Could not restore the working dir" in caplog.text
    assert "Skipping the dependency wave" in caplog.text
    m.get_pkg_dependencies.assert_not_called()


def test_stop_child_interrupted_and_still_alive_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An interrupt triggers a best-effort kill, warns when the child
    cannot be confirmed dead, and re-raises so the build aborts."""
    proc = MagicMock()
    proc.wait.side_effect = KeyboardInterrupt()
    proc.poll.return_value = None
    with pytest.raises(KeyboardInterrupt):
        pf._stop_child(proc)
    proc.kill.assert_called_once_with()
    assert "could not be confirmed stopped" in caplog.text


def test_uri_derived_name_spec_downloads_but_never_installs(tmp_path: Path) -> None:
    """A URL spec whose name is derived from the URI installs into a dir
    named by the archive manifest, not the derived name; its archive is
    prefetched, but the install stays with pio run."""
    m = _fake_manager(tmp_path)
    resp = MagicMock(ok=True)
    resp.headers = {"content-length": "4"}
    with patch("esphome.net_retry.http_request", return_value=resp):
        jobs, failed, installable = pf._uri_jobs(
            m, [_FakeSpec(uri="https://x/v1.zip", name="v1")], set()
        )
    assert failed == 0
    assert len(jobs) == 1  # still prefetched
    assert installable == []
    # Cached-from-an-earlier-run archives are skipped the same way
    dl = Path(m.compute_download_path("https://x/v1.zip", ""))
    dl.parent.mkdir(parents=True, exist_ok=True)
    dl.touch()
    assert pf._uri_jobs(m, [_FakeSpec(uri="https://x/v1.zip", name="v1")], set()) == (
        [],
        0,
        [],
    )


def test_dependency_entries_warn_when_all_reads_fail(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Every manifest read failing is a systematic fault (a pio API
    break), not one bad package; the waves must not vanish silently."""
    m = _fake_manager(tmp_path)
    m.get_package.side_effect = lambda spec: SimpleNamespace(spec=spec)
    m.get_pkg_dependencies.side_effect = RuntimeError("api break")
    assert (
        pf._dependency_entries(
            m,
            [
                ("a@1", _FakeSpec(name="a")),
                ("b@1", _FakeSpec(name="b")),
            ],
            set(),
        )
        == []
    )
    assert "Could not read dependencies of 2 of 2" in caplog.text


def _proc(wait_effect) -> MagicMock:
    proc = MagicMock()
    if isinstance(wait_effect, BaseException):
        proc.wait.side_effect = [wait_effect, 0]
    else:
        proc.wait.return_value = wait_effect
    return proc


def test_prefetch_passes_dashboard_flag(tmp_path: Path) -> None:
    """The dashboard flag reaches the child so its bar still draws."""
    CORE.dashboard = True
    proc = MagicMock()
    proc.wait.return_value = 0
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "Popen", return_value=proc) as mock_popen,
    ):
        pf.prefetch_platformio_packages()
    assert mock_popen.call_args[1]["env"]["ESPHOME_PREFETCH_DASHBOARD"] == "1"


@pytest.mark.parametrize(
    ("wait_effect", "spawn_error", "expected"),
    [
        ("timeout", None, "prefetch timed out"),
        (4, None, "prefetch skipped (exit 4)"),
        # Exit 1 is the interpreter's own import-failure code, never quiet
        (1, None, "prefetch skipped (exit 1)"),
        (None, OSError("no exec"), "PlatformIO package prefetch skipped"),
    ],
)
def test_prefetch_spawn_failures_warn_and_continue(
    caplog: pytest.LogCaptureFixture, wait_effect, spawn_error, expected
) -> None:
    """Timeouts, nonzero exits, and spawn failures each warn, never raise;
    a timed-out child is stopped gracefully. The mock is built per test:
    a collection-time mock's consumable side_effect breaks reruns."""
    if spawn_error is not None:
        popen_effect = {"side_effect": spawn_error}
    elif wait_effect == "timeout":
        popen_effect = {
            "return_value": _proc(
                pf.subprocess.TimeoutExpired("cmd", pf._PREFETCH_TIMEOUT)
            )
        }
    else:
        popen_effect = {"return_value": _proc(wait_effect)}
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "Popen", **popen_effect),
    ):
        pf.prefetch_platformio_packages()
    assert expected in caplog.text


def test_stop_child_waits_terminates_then_kills() -> None:
    """The stop sequence waits for a self-unwinding child first, then
    SIGTERMs, and kills only a child that will not stop. The platform is
    pinned: on Windows the terminate arm is deliberately skipped."""
    timeout = pf.subprocess.TimeoutExpired("cmd", 5)
    with patch.object(pf.sys, "platform", "linux"):
        # Child already unwinding from its own SIGINT: no signals at all
        proc = MagicMock()
        proc.wait.return_value = 0
        pf._stop_child(proc)
        proc.terminate.assert_not_called()
        # Child needs the SIGTERM unwind
        proc = MagicMock()
        proc.wait.side_effect = [timeout, 0]
        pf._stop_child(proc)
        proc.terminate.assert_called_once_with()
        proc.kill.assert_not_called()
    # Child ignoring SIGTERM is killed with a bounded reap
    proc = MagicMock()
    proc.wait.side_effect = [timeout, timeout, 0]
    pf._stop_child(proc)
    proc.kill.assert_called_once_with()
    # An interrupt mid-stop re-raises so the build aborts
    proc = MagicMock()
    proc.wait.side_effect = KeyboardInterrupt()
    with pytest.raises(KeyboardInterrupt):
        pf._stop_child(proc)


def test_preinstall_failure_removes_torn_destination(tmp_path: Path) -> None:
    """A failed install removes whatever get_package can see so pio run
    genuinely reinstalls it; a cleanup failure warns."""
    m = _fake_manager(tmp_path)
    m._install.side_effect = RuntimeError("postinstall failed")
    # cleanup lookup first, then the dependency-wave lookup
    m.get_package.side_effect = [SimpleNamespace(path=str(tmp_path / "torn")), None]
    removed: list[str] = []
    with patch.object(pf, "rmtree", side_effect=removed.append):
        pf._preinstall(m, [("bad@1", _FakeSpec(name="bad"))])
    assert removed == [str(tmp_path / "torn")]


def test_preinstall_system_exit_still_cleans(tmp_path: Path) -> None:
    """A worker SystemExit runs the torn cleanup before propagating."""
    m = _fake_manager(tmp_path)
    m._install.side_effect = SystemExit(143)
    m.get_package.side_effect = [SimpleNamespace(path=str(tmp_path / "torn")), None]
    removed: list[str] = []
    with (
        patch.object(pf, "rmtree", side_effect=removed.append),
        pytest.raises(SystemExit),
    ):
        pf._preinstall(m, [("bad@1", _FakeSpec(name="bad"))])
    assert removed == [str(tmp_path / "torn")]


def test_preinstall_stuck_tree_drops_metadata(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unremovable torn tree loses its .piopm so pio run reinstalls
    it instead of trusting it forever."""
    m = _fake_manager(tmp_path)
    m._install.side_effect = RuntimeError("boom")
    torn = tmp_path / "torn"
    torn.mkdir()
    (torn / ".piopm").write_text("{}")
    m.get_package.side_effect = [SimpleNamespace(path=str(torn)), None]
    with patch.object(pf, "rmtree", side_effect=OSError("busy")):
        pf._preinstall(m, [("bad@1", _FakeSpec(name="bad"))])
    assert not (torn / ".piopm").exists()
    assert torn.exists()  # tidiness is best-effort; metadata is the invariant


def test_preinstall_cleanup_failure_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    m = _fake_manager(tmp_path)
    m._install.side_effect = RuntimeError("boom")
    m.get_package.side_effect = [OSError("scan failed"), None]
    pf._preinstall(m, [("bad@1", _FakeSpec(name="bad"))])
    assert "Could not remove the failed install of bad@1" in caplog.text


def test_dependency_entries_honor_compatibility(tmp_path: Path) -> None:
    """A dependency pio's install_dependency would skip as incompatible is
    not pre-installed either."""
    m = _fake_manager(tmp_path)
    m.compatibility = PackageCompatibility(platforms=["espressif32"])
    # only the top-level entry is installed; the deps are not
    m.get_package.side_effect = lambda spec: (
        SimpleNamespace(spec=spec) if getattr(spec, "name", "") == "top" else None
    )
    m.get_pkg_dependencies.return_value = [
        {"owner": "o", "name": "espdep", "version": "^1", "platforms": ["espressif32"]},
        {"owner": "o", "name": "avrdep", "version": "^1", "platforms": ["atmelavr"]},
    ]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    entries = pf._dependency_entries(m, [("top@1", _FakeSpec(name="top"))], set())
    assert [name for name, *_ in entries] == ["espdep"]


def test_dependency_entries_skip_builtin_libs(tmp_path: Path) -> None:
    """An owner-less versioned dep naming a framework builtin (the dict
    manifest form of SPI/Wire) is skipped like pio's install_dependency;
    a registry copy would shadow the bundled library."""
    m = _fake_manager(tmp_path)
    m.is_builtin_lib.side_effect = lambda name: name == "SPI"
    m.get_package.side_effect = lambda spec: (
        SimpleNamespace(spec=spec) if getattr(spec, "name", "") == "top" else None
    )
    m.get_pkg_dependencies.return_value = [
        {"name": "SPI", "version": "*"},
        {"name": "realdep", "version": "^1"},
    ]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    entries = pf._dependency_entries(m, [("top@1", _FakeSpec(name="top"))], set())
    assert [name for name, *_ in entries] == ["realdep"]


def test_prefetch_interrupt_stops_child_gracefully() -> None:
    """On Ctrl-C the stop sequence waits first; a child that exits on its
    own is never signalled, and the interrupt re-raises."""
    proc = MagicMock()
    proc.wait.side_effect = [KeyboardInterrupt(), 0]
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "Popen", return_value=proc),
        pytest.raises(KeyboardInterrupt),
    ):
        pf.prefetch_platformio_packages()
    # the stop sequence's first wait saw the child exit on its own
    proc.terminate.assert_not_called()
    proc.kill.assert_not_called()


def test_prefetch_child_handled_failure_is_quiet(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Exit _EXIT_HANDLED (3) means the child already warned with the
    reason; the parent adds no second warning."""
    proc = MagicMock()
    proc.wait.return_value = pf._EXIT_HANDLED
    with (
        patch("esphome.platformio.toolchain.heal_platformio_python_env"),
        patch.object(pf.subprocess, "Popen", return_value=proc),
    ):
        pf.prefetch_platformio_packages()
    assert "prefetch skipped" not in caplog.text


def test_main_guards_and_exits_nonzero(caplog: pytest.LogCaptureFixture) -> None:
    """A swallowed failure still reaches the parent as a nonzero exit; the
    parent warns and continues, never failing the build."""
    with patch.object(pf, "_prefetch", side_effect=RuntimeError("boom")):
        assert pf.main(["/b", "testenv"]) == pf._EXIT_HANDLED
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
    # A bare MagicMock's get_download_dir would fspath to '' and point the
    # sidecar sweep at the process cwd
    fake_pm.get_download_dir.return_value = str(tmp_path / "downloads")
    fake_pm.DOWNLOAD_CACHE_EXPIRE = 86400 * 30

    def fake_lib_manager(storage_dir, **kwargs):
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
                uri=None,
                name=kw.get("name") or (a[0] if a else None),
                owner=kw.get("owner")
                or (str(a[0]).split("/")[0] if a and "/" in str(a[0]) else None),
                external=bool(a and "://" in str(a[0])),
            ),
            PackageCompatibility=SimpleNamespace,
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
        patch.object(pf.subprocess, "Popen") as mock_popen,
    ):
        pf.prefetch_platformio_packages()
    mock_popen.assert_not_called()


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
            "framework": "arduino",
            # the bare built-in name and the interpolation are skipped;
            # only the owner-qualified library resolves
            "lib_deps": ["esphome/noise-c@1.0", "WiFi", "${common.lib_deps}"],
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
    spec = _FakeSpec(name="cachedpkg")
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
    assert mock_install.call_args[0][1] == [("cachedpkg@1", spec)]
    assert not (tmp_path / pf._SENTINEL_NAME).exists()


def test_preinstall_extracts_in_parallel_under_one_lock(tmp_path: Path) -> None:
    """The manager lock wraps the whole batch; per-thread managers share
    its package dir; one failing install leaves the rest alone."""
    m = _fake_manager(tmp_path)
    installed: list[str] = []

    def fake_install(spec, skip_dependencies, compatibility=None):
        # Dependencies must be skipped: a shared dep extracted from two
        # threads would race one destination dir
        assert skip_dependencies is True
        if spec.name == "bad":
            raise RuntimeError("corrupt archive")
        installed.append(spec.name)

    m._install.side_effect = fake_install
    entries = [
        ("a@1", _FakeSpec(name="a")),
        ("bad@1", _FakeSpec(name="bad")),
        ("b@1", _FakeSpec(name="b")),
    ]
    pf._preinstall(m, entries)
    assert sorted(installed) == ["a", "b"]
    m.lock.assert_called_once_with()
    m.unlock.assert_called_once_with()
    assert m.memcache_reset.call_count >= 1


def test_preinstall_all_failed_warns_once(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Every install failing is a systemic fault, not archive noise."""
    m = _fake_manager(tmp_path)
    m._install.side_effect = AttributeError("_install went away")
    pf._preinstall(
        m,
        [
            ("a@1", _FakeSpec(name="a")),
            ("b@1", _FakeSpec(name="b")),
        ],
    )
    assert "Could not pre-install a@1" in caplog.text
    assert "Could not pre-install any of 2" in caplog.text


def test_preinstall_dedupes_names_across_entries(tmp_path: Path) -> None:
    """Two entries with one name install once (one destination dir)."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    s1 = _FakeSpec(name="dup")
    s2 = _FakeSpec(name="dup")
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
    (entry,) = mock_install.call_args[0][1]
    assert entry[0] == "pkg@1"
    assert entry[1] is s2  # the dict comprehension keeps the last duplicate


def test_preinstall_runs_dependency_waves(tmp_path: Path) -> None:
    """Dependencies of installed packages install in a follow-up wave,
    deduped by name; name-only platform libs stay with pio run."""
    m = _fake_manager(tmp_path)
    installed: list[str] = []
    m._install.side_effect = lambda spec, skip_dependencies, compatibility=None: (
        installed.append(spec.name if hasattr(spec, "name") else str(spec))
    )
    pkg = SimpleNamespace(spec="noise-c")
    m.get_package.side_effect = lambda spec: (
        pkg if getattr(spec, "name", None) == "noise-c" else None
    )
    m.get_pkg_dependencies.return_value = [
        {"owner": "esphome", "name": "libsodium", "version": "^1.0"},
        {"owner": "esphome", "name": "libsodium", "version": "^1.0"},
        {"name": "SPI"},
    ]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    pf._preinstall(m, [("noise-c@0.1.21", _FakeSpec(name="noise-c"))])
    assert installed == ["noise-c", "libsodium"]  # dep deduped, SPI left out
    # The dep wave carries its compatibility so _install searches qualified
    dep_call = m._install.call_args_list[-1]
    assert dep_call.kwargs["compatibility"] is not None


def test_preinstall_dependency_wave_skips_seen_names(tmp_path: Path) -> None:
    """A dependency whose name matches an already-waved entry is not
    reinstalled."""
    m = _fake_manager(tmp_path)
    m.get_package.side_effect = lambda spec: SimpleNamespace(spec=spec)
    m.get_pkg_dependencies.return_value = [
        {"owner": "esphome", "name": "noise-c", "version": "^0.1"},
    ]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    installed: list[str] = []
    m._install.side_effect = lambda spec, skip_dependencies, compatibility=None: (
        installed.append(getattr(spec, "name", str(spec)))
    )
    pf._preinstall(m, [("noise-c@0.1.21", _FakeSpec(name="noise-c"))])
    assert installed == ["noise-c"]


def test_preinstall_uses_distinct_managers_in_parallel(tmp_path: Path) -> None:
    """Each worker thread gets its own pre-built manager and installs
    genuinely overlap (the barrier deadlocks a serial pool). The worker
    count is pinned so a 1-CPU host cannot serialize the pool."""
    barrier = threading.Barrier(2, timeout=5)
    used: set = set()

    class _WaveManager:
        package_dir = str(tmp_path)
        compatibility = None

        def __init__(self, package_dir, **kwargs) -> None:
            assert package_dir == str(tmp_path)

        def lock(self) -> None:
            pass

        def unlock(self) -> None:
            pass

        def memcache_reset(self) -> None:
            pass

        def get_tmp_dir(self) -> str:
            return str(tmp_path)

        def get_download_dir(self) -> str:
            return str(tmp_path)

        def get_package(self, spec):
            return None

        def get_pkg_dependencies(self, pkg):
            return None

        def _install(self, spec, skip_dependencies, compatibility=None) -> None:
            used.add(id(self))
            barrier.wait()

    seed = _WaveManager(str(tmp_path))
    with patch.object(pf, "get_usable_cpu_count", return_value=2):
        pf._preinstall(
            seed,
            [
                ("a@1", _FakeSpec(name="a")),
                ("b@1", _FakeSpec(name="b")),
            ],
        )
    assert len(used) == 2
    assert id(seed) not in used


def test_sibling_manager_and_sigterm() -> None:
    """Sibling managers inherit compatibility; SIGTERM raises SystemExit."""
    calls = []
    m = MagicMock(package_dir="p", compatibility="qual")
    m.__class__ = lambda package_dir, **kw: calls.append((package_dir, kw))
    pf._sibling_manager(m)
    m.compatibility = None
    pf._sibling_manager(m)
    assert calls == [("p", {"compatibility": "qual"}), ("p", {})]
    with pytest.raises(SystemExit):
        pf._sigterm(15, None)


def test_dependency_entries_skip_installed(tmp_path: Path) -> None:
    """A dependency a previous build installed stays off the destructive
    failure path."""
    m = _fake_manager(tmp_path)
    m.get_package.side_effect = lambda spec: SimpleNamespace(spec=spec)
    m.get_pkg_dependencies.return_value = [
        {"owner": "o", "name": "already", "version": "^1"},
    ]
    m.dependency_to_spec.side_effect = lambda dep: _FakeSpec(name=dep["name"])
    assert pf._dependency_entries(m, [("top@1", _FakeSpec(name="top"))], set()) == []


def test_group_failure_does_not_skip_other_groups(tmp_path: Path) -> None:
    """One group's pre-install failure degrades that group only."""
    _write_ini(tmp_path, "[env:testenv]\nplatform = fake/p@1\n")
    fake_platform = MagicMock()
    fake_platform.packages = {}
    config = _fake_config(tmp_path, {"platform": "fake/p@1"})
    modules = _pio_modules(tmp_path, fake_platform, MagicMock(), config)
    s1 = _FakeSpec(name="toolpkg")
    s2 = _FakeSpec(name="libpkg")
    with (
        patch.dict("sys.modules", modules),
        patch.object(
            pf,
            "_registry_jobs",
            side_effect=[
                ([], 0, [("toolpkg@1", s1)]),
                ([], 0, [("libpkg@1", s2)]),
            ],
        ),
        patch.object(pf, "_uri_jobs", return_value=([], 0, [])),
        patch.object(
            pf, "_preinstall", side_effect=[RuntimeError("group down"), None]
        ) as mock_install,
    ):
        pf._prefetch(tmp_path, "testenv")
    assert mock_install.call_count == 2


def test_preinstall_unlocks_even_when_pool_fails(tmp_path: Path) -> None:
    """A failure inside the pool cancels queued installs and releases the
    lock; a failing executor construction still releases it."""
    m = _fake_manager(tmp_path)
    boom = MagicMock()
    boom.__enter__.return_value = boom
    boom.map.side_effect = RuntimeError("no threads")
    with (
        patch.object(pf, "ThreadPoolExecutor", return_value=boom),
        pytest.raises(RuntimeError),
    ):
        pf._preinstall(m, [("a@1", _FakeSpec(name="a"))])
    m.unlock.assert_called_once_with()
    assert boom.shutdown.call_args_list[0][1].get("cancel_futures") is True
    m.reset_mock()
    # A failing executor construction still releases the lock
    with (
        patch.object(pf, "ThreadPoolExecutor", side_effect=RuntimeError("no")),
        pytest.raises(RuntimeError),
    ):
        pf._preinstall(m, [("a@1", _FakeSpec(name="a"))])
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

    Also load-bearing but unpinnable by introspection: pio's private
    _install must never re-acquire the manager's inter-process lock
    (locking lives in the public install()); a re-lock would hang the
    child for the full prefetch timeout, so re-check it on any bump.

    Everything else in this module mocks the managers, so this is the one
    test that fails loudly when a requirements bump changes the private
    surface instead of silently degrading the prefetch to a no-op.
    """
    params = inspect.signature(PackageManagerInstallMixin._install).parameters
    assert "spec" in params
    assert "skip_dependencies" in params
    assert "compatibility" in params
    for cls in (ToolPackageManager, LibraryPackageManager, PlatformPackageManager):
        assert "package_dir" in inspect.signature(cls.__init__).parameters
    for name in (
        "lock",
        "unlock",
        "memcache_reset",
        "get_package",
        "compute_download_path",
        "get_pkg_dependencies",
        "dependency_to_spec",
    ):
        assert callable(getattr(BasePackageManager, name))
    # The dependency wave mirrors install_dependency's builtin skip
    assert callable(LibraryPackageManager.is_builtin_lib)
    # The pre-install passes these positionally / by keyword
    assert "compatibility" in inspect.signature(BasePackageManager.__init__).parameters
    lib_params = inspect.signature(LibraryPackageManager.__init__).parameters
    # Capability, not implementation: an explicit compatibility= parameter
    # would serve the call site just as well as **kwargs forwarding
    assert "compatibility" in lib_params or any(
        p.kind is inspect.Parameter.VAR_KEYWORD for p in lib_params.values()
    )
    assert callable(PackageCompatibility.from_dependency)
    assert callable(PackageCompatibility.is_compatible)
    # Every URL spec derives a name from the URI; only a custom name
    # (Foo=https://...) is also the destination dir the wave installs into
    derived = PackageSpec("https://x/y/archive/master.zip")
    assert derived.name and not derived.has_custom_name()
    assert PackageSpec("Foo=https://x/y/archive/master.zip").has_custom_name()
