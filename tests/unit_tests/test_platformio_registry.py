"""Tests for esphome.platformio.registry (PIO-registry package installs)."""

from __future__ import annotations

from contextlib import contextmanager
import json
import os
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.core import EsphomeError
from esphome.platformio import registry


def test_registry_download_resolves_once_per_process() -> None:
    """The prefetch and the install share one metadata resolve per package."""
    calls: list[dict] = []
    payload = {
        "versions": [
            {
                "name": "1.0.0",
                "files": [
                    {
                        "download_url": "http://x/pkg.tar.gz",
                        "checksum": {"sha256": "ab" * 32},
                        "size": 5,
                    }
                ],
            }
        ]
    }

    def fake_request(method, url, **kwargs):
        calls.append(url)
        return _http_response(json.dumps(payload))

    with patch.object(registry, "http_request", side_effect=fake_request):
        first = registry.registry_download("o/pkg", "1.0.0")
        second = registry.registry_download("o/pkg", "1.0.0")
    assert first == second
    assert len(calls) == 1


@pytest.fixture(autouse=True)
def _fresh_registry_cache():
    # registry_download memoizes per process; tests reuse package names
    registry.registry_download.cache_clear()
    yield
    registry.registry_download.cache_clear()


@pytest.mark.parametrize(
    ("system", "machine", "expected"),
    [
        ("Darwin", "arm64", "darwin_arm64"),
        ("Darwin", "x86_64", "darwin_x86_64"),
        ("Windows", "AMD64", "windows_amd64"),
        # Deviation from upstream: auto-mapped to the emulated-x86 packages
        ("Windows", "ARM64", "windows_amd64"),
        ("Windows", "x86", "windows_x86"),
        ("Linux", "x86_64", "linux_x86_64"),
        ("Linux", "aarch64", "linux_aarch64"),
        ("Linux", "i686", "linux_i686"),
        ("Linux", "armv7l", "linux_armv7l"),
        # Unknown hosts pass through like upstream; the registry lookup
        # then fails naming the tag
        ("FreeBSD", "amd64", "freebsd_amd64"),
    ],
)
def test_get_systype(system: str, machine: str, expected: str) -> None:
    with (
        patch("platform.system", return_value=system),
        patch("platform.machine", return_value=machine),
        patch("platform.architecture", return_value=("64bit", "")),
    ):
        assert registry.get_systype() == expected


def test_get_systype_env_override() -> None:
    """PLATFORMIO_SYSTEM_TYPE wins, exactly as in upstream get_systype()."""
    with patch.dict(os.environ, {"PLATFORMIO_SYSTEM_TYPE": "windows_amd64"}):
        assert registry.get_systype() == "windows_amd64"


def test_get_systype_aarch64_32bit_userland() -> None:
    """A 32-bit userland on a 64-bit arm kernel gets armv7l binaries."""
    with (
        patch("platform.system", return_value="Linux"),
        patch("platform.machine", return_value="aarch64"),
        patch("platform.architecture", return_value=("32bit", "")),
    ):
        assert registry.get_systype() == "linux_armv7l"


def test_get_systype_windows_empty_machine() -> None:
    """An empty machine string falls back to the architecture bits."""
    with (
        patch("platform.system", return_value="Windows"),
        patch("platform.machine", return_value=""),
        patch("platform.architecture", return_value=("64bit", "")),
    ):
        assert registry.get_systype() == "windows_amd64"


def _http_response(text: str) -> MagicMock:
    resp = MagicMock()
    resp.text = text
    resp.raise_for_status.return_value = None
    return resp


def _registry_response(files: list[dict]):
    """Patch the consolidated HTTP path to serve a canned registry response."""
    payload = {"versions": [{"name": "1.0.0", "files": files}]}
    return patch.object(
        registry, "http_request", return_value=_http_response(json.dumps(payload))
    )


def test_registry_download_uses_shared_http_path() -> None:
    """The metadata fetch delegates to the consolidated http_request path;
    request failures surface as a named EsphomeError."""
    import requests as req

    with (
        patch.object(
            registry,
            "http_request",
            side_effect=req.exceptions.ConnectionError("registry down"),
        ) as mock_request,
        pytest.raises(EsphomeError, match="Could not fetch registry metadata"),
    ):
        registry.registry_download("pkg", "1.0.0")
    (method, url), _ = mock_request.call_args
    assert method == "GET"
    assert url == registry._REGISTRY_URL.format(package="pkg")


def test_registry_download_invalid_json_is_clean() -> None:
    with (
        patch.object(
            registry,
            "http_request",
            return_value=_http_response("<html>not json</html>"),
        ),
        pytest.raises(EsphomeError, match="invalid JSON"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_matches_system() -> None:
    with (
        _registry_response(
            [
                {"system": ["windows_amd64"], "download_url": "http://x/win"},
                {
                    "system": ["linux_x86_64"],
                    "download_url": "http://x/linux",
                    "checksum": {"sha256": "abc123"},
                    "size": 42,
                },
            ]
        ),
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
    ):
        assert registry.registry_download("pkg", "1.0.0") == (
            "http://x/linux",
            "abc123",
            42,
        )


def test_registry_download_bare_string_system() -> None:
    """A bare-string system tag is an exact match, not a substring test."""
    with (
        _registry_response(
            [
                {"system": "linux_x86", "download_url": "http://x/x86"},
                {
                    "system": "linux_x86_64",
                    "download_url": "http://x/x86_64",
                    "checksum": {"sha256": "abc"},
                },
            ]
        ),
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
    ):
        assert registry.registry_download("pkg", "1.0.0")[0] == "http://x/x86_64"


def test_registry_download_wildcard_system() -> None:
    with _registry_response(
        [
            {
                "system": "*",
                "download_url": "http://x/any",
                "checksum": {"sha256": "abc"},
                "size": 7,
            }
        ]
    ):
        assert registry.registry_download("pkg", "1.0.0") == (
            "http://x/any",
            "abc",
            7,
        )


def test_registry_download_missing_checksum_raises() -> None:
    """An unverifiable archive is refused, never silently extracted."""
    with (
        _registry_response([{"system": "*", "download_url": "http://x/any"}]),
        pytest.raises(EsphomeError, match="no sha256"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_no_system_match() -> None:
    with (
        _registry_response(
            [{"system": ["windows_amd64"], "download_url": "http://x/win"}]
        ),
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
        pytest.raises(EsphomeError, match="No pkg 1.0.0 build"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_version_not_found() -> None:
    with (
        patch.object(
            registry,
            "http_request",
            return_value=_http_response(
                json.dumps({"versions": [{"name": "2.0.0", "files": []}]})
            ),
        ),
        pytest.raises(EsphomeError, match="not found"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_install_package_skips_when_marker_exists(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    (dest / "payload").mkdir(parents=True)
    (dest / ".esphome_extracted").touch()
    with patch.object(registry, "download_from_mirrors") as mock_download:
        registry.install_package(
            "pkg", "1.0.0", dest, [], tmp_path / "dl", expect=("payload",)
        )
    mock_download.assert_not_called()


def test_install_package_marker_hit_rechecks_layout(tmp_path: Path) -> None:
    """A marked install that later lost files fails by name instead of
    surfacing as an opaque toolchain error."""
    dest = tmp_path / "pkg"
    dest.mkdir()
    (dest / ".esphome_extracted").touch()
    with pytest.raises(EsphomeError, match="missing the expected payload"):
        registry.install_package(
            "pkg", "1.0.0", dest, [], tmp_path / "dl", expect=("payload",)
        )


def test_install_package_downloads_via_mirrors(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    mirrors = ["http://mirror/{VERSION}/{SYSTEM}.tar.gz"]
    with (
        patch.object(registry, "download_from_mirrors") as mock_download,
        patch.object(registry, "archive_extract_all") as mock_extract,
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
    ):
        # Extraction is expected to create the directory
        mock_extract.side_effect = lambda *_a, **_kw: (dest / "payload").mkdir(
            parents=True
        )
        registry.install_package(
            "pkg", "1.0.0", dest, mirrors, tmp_path / "dl", expect=("payload",)
        )
    assert mock_download.call_args[0][0] is mirrors
    assert mock_download.call_args[0][1] == {
        "VERSION": "1.0.0",
        "SYSTEM": "linux_x86_64",
    }
    assert (dest / ".esphome_extracted").is_file()


def test_install_package_downloads_via_registry(tmp_path: Path) -> None:
    """The registry path downloads with the registry's sha256 and size."""
    dest = tmp_path / "pkg"
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(registry, "archive_extract_all") as mock_extract,
        patch.object(
            registry,
            "registry_download",
            return_value=("http://x/pkg.tar.gz", "abc123", 42),
        ),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: (dest / "payload").mkdir(
            parents=True
        )
        registry.install_package(
            "pkg", "1.0.0", dest, [], tmp_path / "dl", expect=("payload",)
        )
    assert mock_download.call_args[0][0] == "http://x/pkg.tar.gz"
    assert mock_download.call_args[1] == {"sha256": "abc123", "size": 42}


def test_install_package_validates_expected_layout(tmp_path: Path) -> None:
    """The success marker is only written when the extracted tree is usable."""
    dest = tmp_path / "pkg"
    with (
        patch.object(registry, "download_from_mirrors"),
        patch.object(registry, "archive_extract_all") as mock_extract,
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: (dest / "bin").mkdir(parents=True)
        registry.install_package(
            "pkg", "1.0.0", dest, ["http://m"], tmp_path / "dl", expect=("bin",)
        )
    assert (dest / ".esphome_extracted").is_file()


def test_install_package_unexpected_layout_raises(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    with (
        patch.object(registry, "download_from_mirrors"),
        patch.object(registry, "archive_extract_all") as mock_extract,
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
        pytest.raises(EsphomeError, match="missing the expected bin"),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: (dest / "payload").mkdir(
            parents=True
        )
        registry.install_package(
            "pkg", "1.0.0", dest, ["http://m"], tmp_path / "dl", expect=("bin",)
        )
    assert not (dest / ".esphome_extracted").exists()


def test_install_package_marker_rechecked_under_lock(tmp_path: Path) -> None:
    """A concurrent install finishing while we wait for the lock is detected."""
    dest = tmp_path / "pkg"
    marker = dest / ".esphome_extracted"

    @contextmanager
    def _fake_lock(*_a, **_kw):
        dest.mkdir(parents=True, exist_ok=True)
        marker.touch()
        yield

    with (
        patch("filelock.FileLock", _fake_lock),
        patch.object(registry, "download_from_mirrors") as mock_download,
        patch.object(registry, "rmdir") as mock_rmdir,
    ):
        registry.install_package(
            "pkg", "1.0.0", dest, ["http://m"], tmp_path / "dl", expect=("payload",)
        )
    mock_download.assert_not_called()
    mock_rmdir.assert_not_called()


def test_install_package_uses_hard_lock(tmp_path: Path) -> None:
    """The install lock must never degrade to a soft (existence) lock."""
    dest = tmp_path / "pkg"
    with (
        patch("filelock.FileLock") as mock_lock,
        patch.object(registry, "download_from_mirrors"),
        patch.object(registry, "archive_extract_all") as mock_extract,
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: (dest / "payload").mkdir(
            parents=True, exist_ok=True
        )
        registry.install_package(
            "pkg", "1.0.0", dest, ["http://m"], tmp_path / "dl", expect=("payload",)
        )
    assert mock_lock.call_args.kwargs["fallback_to_soft"] is False


def test_registry_download_empty_system_list_does_not_match() -> None:
    """An explicitly empty system list must not act as a wildcard."""
    with (
        _registry_response([{"system": [], "download_url": "http://x/any"}]),
        patch.object(registry, "get_systype", return_value="linux_x86_64"),
        pytest.raises(EsphomeError, match="No pkg 1.0.0 build"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_unexpected_payload_is_named() -> None:
    """An error envelope without a versions list is not 'version not found'."""

    with (
        patch.object(
            registry,
            "http_request",
            return_value=_http_response(json.dumps({"message": "rate limited"})),
        ),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_missing_system_key_matches_any() -> None:
    """A file with no system key at all serves every host."""
    with _registry_response(
        [{"download_url": "http://x/any", "checksum": {"sha256": "abc"}, "size": 1}]
    ):
        assert registry.registry_download("pkg", "1.0.0") == ("http://x/any", "abc", 1)


def test_registry_download_missing_files_list_is_named() -> None:
    """A version entry without a files list is an unexpected payload, not a
    missing platform build."""
    with (
        _registry_response(None),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_missing_download_url_is_named() -> None:
    with (
        _registry_response([{"system": "*", "checksum": {"sha256": "abc"}, "size": 1}]),
        pytest.raises(EsphomeError, match="no download URL"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_install_package_empty_expect_rejected(tmp_path: Path) -> None:
    """Layout validation is the only guard before marker.touch(), so an
    empty expect is a caller bug, not a lenient install."""
    with pytest.raises(ValueError, match="non-empty expect"):
        registry.install_package(
            "pkg", "1.0.0", tmp_path / "pkg", [], tmp_path / "dl", expect=()
        )


def test_registry_download_non_dict_version_entry_is_named() -> None:
    """A versions list of bare strings is an unexpected payload, not an
    AttributeError traceback."""

    with (
        patch.object(
            registry,
            "http_request",
            return_value=_http_response(json.dumps({"versions": ["1.0.0", "2.0.0"]})),
        ),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_non_dict_file_entry_is_named() -> None:
    with (
        patch.object(
            registry,
            "http_request",
            return_value=_http_response(
                json.dumps({"versions": [{"name": "1.0.0", "files": ["a.tar.gz"]}]})
            ),
        ),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_non_dict_payload_is_named() -> None:
    """A JSON array answer is an unexpected payload at the outermost level."""

    with (
        patch.object(
            registry,
            "http_request",
            return_value=_http_response(json.dumps(["1.0.0"])),
        ),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_non_list_system_is_named() -> None:
    """A system field that is neither missing, str, nor list is an
    unexpected payload, not a TypeError from the ``in`` test."""
    with (
        _registry_response([{"system": 5, "checksum": {"sha256": "abc"}, "size": 1}]),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def _resolve_for(sizes: dict[str, int | None]):
    def resolve(name: str, version: str):
        size = sizes[name]
        if size == -1:
            raise EsphomeError("registry down")
        return (f"http://x/{name}.tar.gz", "abc123", size)

    return resolve


def test_prefetch_packages_downloads_pending_in_parallel(tmp_path: Path) -> None:
    """Two uninstalled packages download together under one combined bar,
    with the registry's sha256 and size and a batch progress tracker."""
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": 10, "b": 20})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", []),
                ("b", "2.0", tmp_path / "b", []),
            ],
            tmp_path / "dl",
        )
    assert mock_download.call_count == 2
    # Locking makes worker completion order nondeterministic
    calls = sorted(mock_download.call_args_list, key=lambda c: c[0][0])
    for call, (name, version, size) in zip(
        calls, [("a", "1.0", 10), ("b", "2.0", 20)], strict=True
    ):
        assert call[0][0] == f"http://x/{name}.tar.gz"
        assert call[0][1] == tmp_path / "dl" / f"{name}-{version}"
        assert call[1]["sha256"] == "abc123"
        assert call[1]["size"] == size
        assert callable(call[1]["progress"])


def test_prefetch_packages_skips_freshly_installed_dest(tmp_path: Path) -> None:
    """A dest whose marker appeared while the worker waited on the lock is
    already installed; re-downloading would orphan an archive copy."""
    dest = tmp_path / "a"
    dest.mkdir()

    from contextlib import contextmanager

    @contextmanager
    def marker_appears_under_lock(path, **kwargs):
        # Simulates the concurrent build finishing while we waited
        (dest / ".esphome_extracted").touch()
        yield

    with (
        patch("filelock.FileLock", side_effect=marker_appears_under_lock),
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": 10})
        ),
    ):
        registry.prefetch_packages([("a", "1.0", dest, [])], tmp_path / "dl")
    mock_download.assert_not_called()


def test_already_installed_probe(tmp_path: Path) -> None:
    """Both arms of the marker probe the prefetch worker keys on."""
    dest = tmp_path / "pkg"
    dest.mkdir()
    assert registry._already_installed(dest) is False
    (dest / ".esphome_extracted").touch()
    assert registry._already_installed(dest) is True


def test_prefetch_packages_dedupes_duplicate_entries(tmp_path: Path) -> None:
    """Duplicate (name, version) entries would race each other between two
    workers; only one survives (and one is too few to parallelize)."""
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": 10})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", []),
                ("a", "1.0", tmp_path / "a", []),
            ],
            tmp_path / "dl",
        )
    mock_download.assert_not_called()


def test_prefetch_packages_single_pending_skips(tmp_path: Path) -> None:
    """One pending package has nothing to parallelize; the sequential
    install keeps its own bar."""
    marker_dest = tmp_path / "a"
    marker_dest.mkdir()
    (marker_dest / ".esphome_extracted").touch()
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"b": 20})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", marker_dest, []),
                ("b", "2.0", tmp_path / "b", []),
            ],
            tmp_path / "dl",
        )
    mock_download.assert_not_called()


def test_prefetch_packages_mirror_and_sizeless_stay_sequential(
    tmp_path: Path,
) -> None:
    """Mirror overrides and size-less registry entries are left to the
    sequential path so its per-file bars stay trustworthy."""
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry,
            "registry_download",
            side_effect=_resolve_for({"b": None, "c": 30}),
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", ["http://mirror/{VERSION}"]),
                ("b", "2.0", tmp_path / "b", []),
                ("c", "3.0", tmp_path / "c", []),
            ],
            tmp_path / "dl",
        )
    mock_download.assert_not_called()


def test_prefetch_packages_resolve_failure_defers_to_install(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A registry failure only skips the prefetch; install_package reports
    the real error with context."""
    caplog.set_level("DEBUG")
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": -1, "b": 20})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", []),
                ("b", "2.0", tmp_path / "b", []),
            ],
            tmp_path / "dl",
        )
    mock_download.assert_not_called()
    assert "Prefetch resolve for a failed" in caplog.text


def test_prefetch_packages_complete_archive_skipped(tmp_path: Path) -> None:
    """An archive already fully downloaded is not re-fetched."""
    dl = tmp_path / "dl"
    dl.mkdir()
    (dl / "a-1.0").write_bytes(b"x" * 10)
    with (
        patch.object(registry, "download_with_resume") as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": 10, "b": 20})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", []),
                ("b", "2.0", tmp_path / "b", []),
            ],
            dl,
        )
    mock_download.assert_not_called()


def test_prefetch_packages_download_failure_is_debug(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed prefetch download is logged and left for install_package."""
    caplog.set_level("DEBUG")
    with (
        patch.object(
            registry, "download_with_resume", side_effect=OSError("boom")
        ) as mock_download,
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": 10, "b": 20})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", []),
                ("b", "2.0", tmp_path / "b", []),
            ],
            tmp_path / "dl",
        )
    assert mock_download.call_count == 2
    assert "Prefetch of a failed" in caplog.text
    assert "Prefetch of b failed" in caplog.text


def test_prefetch_packages_unexpected_failure_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A programming error (not a download failure) surfaces at WARNING
    instead of becoming a permanent silent no-op."""
    with (
        patch.object(
            registry, "download_with_resume", side_effect=TypeError("bad call")
        ),
        patch.object(
            registry, "registry_download", side_effect=_resolve_for({"a": 10, "b": 20})
        ),
    ):
        registry.prefetch_packages(
            [
                ("a", "1.0", tmp_path / "a", []),
                ("b", "2.0", tmp_path / "b", []),
            ],
            tmp_path / "dl",
        )
    assert "TypeError" in caplog.text
