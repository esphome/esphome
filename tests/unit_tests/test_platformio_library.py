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
    return LibraryBackend(
        platform="espressif32", framework="espidf", emit=emit, cache_key="idf"
    )


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


def _patch_registry_resolve(monkeypatch: pytest.MonkeyPatch) -> None:
    """Stub the registry lookup so tests never touch the network."""
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


def _patch_download_with_manifests(monkeypatch, tmp_path, manifests, *, properties=()):
    """Fake ConvertedLibrary.download to materialize canned manifests on disk."""

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_require_name()
        self.path.mkdir(parents=True, exist_ok=True)
        if self.name in properties:
            (self.path / "library.properties").write_text(manifests[self.name])
        else:
            (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(ConvertedLibrary, "download", fake_download)
    _patch_registry_resolve(monkeypatch)


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


def _patch_download_without_manifest(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, *, manifest_on_force: bool
) -> list[bool]:
    """Fake ConvertedLibrary.download that leaves the manifest missing.

    When ``manifest_on_force`` is set, a forced re-download writes a valid
    library.json, simulating a broken cache entry that heals on retry.
    Returns the list of ``force`` values download was called with.
    """
    calls: list[bool] = []

    def fake_download(
        self: ConvertedLibrary, force: bool = False, salt: str = "", namespace: str = ""
    ) -> None:
        calls.append(force)
        self.path = tmp_path / self.get_require_name()
        self.path.mkdir(parents=True, exist_ok=True)
        if force and manifest_on_force:
            (self.path / "library.json").write_text(json.dumps({"name": "A"}))

    monkeypatch.setattr(ConvertedLibrary, "download", fake_download)
    _patch_registry_resolve(monkeypatch)
    return calls


def test_convert_libraries_redownloads_when_manifest_missing(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # A cached copy without any manifest (e.g. an interrupted clone or
    # extraction) triggers exactly one forced re-download and then succeeds.
    calls = _patch_download_without_manifest(
        monkeypatch, tmp_path, manifest_on_force=True
    )

    top = convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())

    assert calls == [False, True]
    assert top[0].data["name"] == "A"


def test_convert_libraries_raises_when_manifest_missing_after_retry(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # If the forced re-download still yields no manifest, the error is raised
    # after exactly one retry (no retry loop).
    calls = _patch_download_without_manifest(
        monkeypatch, tmp_path, manifest_on_force=False
    )

    with pytest.raises(RuntimeError, match="Invalid PIO library"):
        convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())

    assert calls == [False, True]


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (None, None),
        ("", None),
        ("http://[::1", None),  # malformed IPv6 makes urlsplit raise ValueError
        ("foo/bar", None),
        ("file:///no/host", None),
        ("https://github.com/x/y", "https://github.com/x/y"),
    ],
)
def test_url_or_none(value: str | None, expected: str | None) -> None:
    assert lib._url_or_none(value) == expected


def test_convert_libraries_url_in_name_resolves_as_git(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # add_library("https://github.com/x/y", None) puts a git URL in the name
    # position; it must resolve as a git source and never hit the registry.
    _patch_download_with_manifests(
        monkeypatch, tmp_path, {"pstolarz/OneWireNg": {"name": "OneWireNg"}}
    )

    def fail_registry(owner: str, pkgname: str, requirements: set[str]) -> None:
        raise AssertionError(f"registry consulted for {owner}/{pkgname}")

    # After the helper so this stub wins over the helper's benign one
    monkeypatch.setattr(lib, "_resolve_registry_version", fail_registry)

    top = convert_libraries(
        [Library("https://github.com/pstolarz/OneWireNg", None, None)], _backend()
    )

    assert [c.name for c in top] == ["pstolarz/OneWireNg"]
    assert top[0].data["name"] == "OneWireNg"
    source = top[0].source
    assert isinstance(source, GitSource)
    assert source.url == "https://github.com/pstolarz/OneWireNg"
    assert source.ref is None


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
