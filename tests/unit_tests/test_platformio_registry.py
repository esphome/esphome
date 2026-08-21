"""Tests for esphome.platformio.registry (PIO-registry package installs)."""

from __future__ import annotations

from contextlib import contextmanager
import json
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.core import EsphomeError
from esphome.platformio import registry


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


def _registry_response(files: list[dict]):
    """Patch the shared downloader to serve a canned registry response."""
    payload = {"versions": [{"name": "1.0.0", "files": files}]}

    def fake_download(mirrors: list[str], substitutions: dict, target) -> str:
        target.write(json.dumps(payload).encode())
        return mirrors[0].format(**substitutions)

    return patch.object(registry, "download_from_mirrors", side_effect=fake_download)


def test_registry_download_uses_shared_downloader() -> None:
    """The metadata fetch delegates its retries and error reporting to
    download_from_mirrors; failures surface unchanged."""
    with (
        patch.object(
            registry,
            "download_from_mirrors",
            side_effect=EsphomeError("Failed to download from all mirrors"),
        ) as mock_download,
        pytest.raises(EsphomeError, match="Failed to download from all mirrors"),
    ):
        registry.registry_download("pkg", "1.0.0")
    (mirrors, substitutions, _), _ = mock_download.call_args
    assert mirrors == [registry._REGISTRY_URL]
    assert substitutions == {"package": "pkg"}


def test_registry_download_invalid_json_is_clean() -> None:
    def fake_download(mirrors: list[str], substitutions: dict, target) -> str:
        target.write(b"<html>not json</html>")
        return "http://x"

    with (
        patch.object(registry, "download_from_mirrors", side_effect=fake_download),
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
    def fake_download(mirrors: list[str], substitutions: dict, target) -> str:
        target.write(
            json.dumps({"versions": [{"name": "2.0.0", "files": []}]}).encode()
        )
        return "http://x"

    with (
        patch.object(registry, "download_from_mirrors", side_effect=fake_download),
        pytest.raises(EsphomeError, match="not found"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_install_package_skips_when_marker_exists(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    dest.mkdir()
    (dest / ".esphome_extracted").touch()
    with patch.object(registry, "download_from_mirrors") as mock_download:
        registry.install_package(
            "pkg", "1.0.0", dest, [], tmp_path / "dl", expect=("payload",)
        )
    mock_download.assert_not_called()


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
        pytest.raises(EsphomeError, match="without the expected bin"),
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

    def fake_download(mirrors: list[str], substitutions: dict, target) -> str:
        target.write(json.dumps({"message": "rate limited"}).encode())
        return "http://x"

    with (
        patch.object(registry, "download_from_mirrors", side_effect=fake_download),
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

    def fake_download(mirrors: list[str], substitutions: dict, target) -> str:
        target.write(json.dumps({"versions": ["1.0.0", "2.0.0"]}).encode())
        return "http://x"

    with (
        patch.object(registry, "download_from_mirrors", side_effect=fake_download),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")


def test_registry_download_non_dict_file_entry_is_named() -> None:
    def fake_download(mirrors: list[str], substitutions: dict, target) -> str:
        target.write(
            json.dumps(
                {"versions": [{"name": "1.0.0", "files": ["a.tar.gz"]}]}
            ).encode()
        )
        return "http://x"

    with (
        patch.object(registry, "download_from_mirrors", side_effect=fake_download),
        pytest.raises(EsphomeError, match="Unexpected package registry response"),
    ):
        registry.registry_download("pkg", "1.0.0")
