"""Tests for the toolchain-agnostic PlatformIO library converter.

Covers the shared download/parse/resolve/dependency-walk paths in
``esphome.platformio.library`` directly (the ESP-IDF and Zephyr backends are
exercised in their own test modules)."""

import json
import logging
from pathlib import Path

import pytest

from esphome.core import Library
import esphome.platformio.library as lib
from esphome.platformio.library import (
    ConvertedLibrary,
    GitSource,
    InvalidLibrary,
    LibraryBackend,
    Source,
    URLSource,
    _resolve_registry_version,
    check_library_data,
    convert_libraries,
)


def _backend(emit=lambda component: None) -> LibraryBackend:
    return LibraryBackend(platform="espressif32", framework="espidf", emit=emit)


def test_check_library_data_accepts_wildcards():
    check_library_data({"platforms": "*", "frameworks": "*"}, "espressif32", "espidf")


def test_check_library_data_accepts_missing_frameworks():
    check_library_data({"platforms": "*"}, "espressif32", "espidf")


def test_check_library_data_accepts_empty_manifest():
    check_library_data({}, "espressif32", "espidf")


def test_check_library_data_accepts_matching_platform():
    check_library_data(
        {"platforms": "espressif32", "frameworks": "*"}, "espressif32", "espidf"
    )


def test_check_library_data_accepts_matching_framework():
    check_library_data(
        {"platforms": "*", "frameworks": "espidf"}, "espressif32", "espidf"
    )


def test_check_library_data_rejects_unsupported_platform():
    with pytest.raises(InvalidLibrary):
        check_library_data(
            {"platforms": ["other"], "frameworks": "*"}, "espressif32", "espidf"
        )


def test_check_library_data_warns_on_framework_mismatch(
    caplog: pytest.LogCaptureFixture,
):
    # Framework mismatch is a warning, not a hard skip: the library is still
    # included so manifests that only list "arduino" (but compile fine under the
    # target framework) can be used without forking them.
    with caplog.at_level(logging.WARNING, logger="esphome.platformio.library"):
        check_library_data(
            {"name": "lib", "platforms": "*", "frameworks": ["other"]},
            "espressif32",
            "espidf",
        )
    assert "do not include 'espidf'" in caplog.text


def test_source_download_not_implemented():
    with pytest.raises(NotImplementedError):
        Source().download("x")


def test_gitsource_str_includes_ref_when_present():
    assert str(GitSource("http://git/repo.git", "main")) == "http://git/repo.git#main"
    assert str(GitSource("http://git/repo.git", None)) == "http://git/repo.git"


def test_urlsource_download_extracts_then_reuses_marker(setup_core, monkeypatch):
    monkeypatch.setattr(lib, "rmdir", lambda path, msg="": None)
    dl_calls: list[list[str]] = []
    monkeypatch.setattr(
        lib, "download_from_mirrors", lambda urls, headers, f: dl_calls.append(urls)
    )

    def fake_extract(fileobj, path):
        Path(path).mkdir(parents=True, exist_ok=True)

    monkeypatch.setattr(lib, "archive_extract_all", fake_extract)

    src = URLSource("http://example.test/lib.tar.gz")
    out = src.download("mylib")

    assert (out / ".esphome_extracted").is_file()
    assert dl_calls == [["http://example.test/lib.tar.gz"]]

    # The completion marker means a second download is skipped (cache hit).
    out2 = src.download("mylib")
    assert out2 == out
    assert len(dl_calls) == 1


def test_resolve_registry_version_raises_without_pkg_file(monkeypatch):
    registry = lib._make_registry_client()
    monkeypatch.setattr(
        registry,
        "fetch_registry_package",
        lambda spec: {
            "owner": {"username": spec.owner or "owner"},
            "name": spec.name,
            "versions": [{"name": "1.0.0", "files": [{}]}],
        },
    )
    # A best version exists but none of its files is a compatible package.
    monkeypatch.setattr(
        registry, "pick_best_registry_version", lambda versions: versions[0]
    )
    monkeypatch.setattr(registry, "pick_compatible_pkg_file", lambda files: None)
    monkeypatch.setattr(lib, "_make_registry_client", lambda: registry)

    with pytest.raises(RuntimeError, match="No package file"):
        _resolve_registry_version("owner", "pkg", set())


def _patch_download_with_manifests(monkeypatch, tmp_path, manifests, *, properties=()):
    """Fake ConvertedLibrary.download to materialize canned manifests on disk."""

    def fake_download(self, force=False, salt=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        self.path.mkdir(parents=True, exist_ok=True)
        if self.name in properties:
            (self.path / "library.properties").write_text(manifests[self.name])
        else:
            (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(ConvertedLibrary, "download", fake_download)
    monkeypatch.setattr(
        lib,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner,
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
        ),
    )


def test_convert_libraries_parses_library_properties(tmp_path, monkeypatch):
    # A manifest provided as library.properties (Arduino style) instead of
    # library.json must still be parsed and converted.
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {"esphome/A": "name=A\nversion=1.0\n"},
        properties=("esphome/A",),
    )

    emitted: list[ConvertedLibrary] = []
    top = convert_libraries(
        [Library("esphome/A", "1.0.0", None)], _backend(emitted.append)
    )

    assert [c.name for c in top] == ["esphome/A"]
    assert top[0].data["name"] == "A"
    assert emitted[0].data["version"] == "1.0"


def test_convert_libraries_skips_dependency_without_version(tmp_path, monkeypatch):
    # A dependency entry lacking a version is malformed and silently skipped.
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {"esphome/A": {"name": "A", "dependencies": [{"name": "C"}]}},
    )

    # No version on the top-level spec exercises the "no requirement" path too.
    top = convert_libraries([Library("esphome/A", None, None)], _backend())

    assert top[0].dependencies == []


def test_convert_libraries_handles_unparsable_dependency_version(tmp_path, monkeypatch):
    # If the git/archive URL probe (urlparse) raises on a malformed value, the
    # dependency is still kept and treated as a plain version spec.
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                # An unterminated IPv6 URL makes urlparse raise ValueError.
                "dependencies": [{"name": "C", "version": "http://[::1"}],
            },
            "C": {"name": "C"},
        },
    )

    top = convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())

    assert [d.name for d in top[0].dependencies] == ["C"]


def test_convert_libraries_skips_incompatible_dependency(tmp_path, monkeypatch):
    # A dependency that declares an incompatible platform is skipped (the
    # top-level library still builds).
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "C", "version": "1.0", "platforms": ["avr"]}],
            }
        },
    )

    top = convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())

    assert top[0].dependencies == []
