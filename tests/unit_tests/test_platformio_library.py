"""Tests for the toolchain-agnostic PlatformIO library converter.

Covers the shared download/parse/resolve/dependency-walk paths in
``esphome.platformio.library`` directly (the ESP-IDF and Zephyr backends are
exercised in their own test modules)."""

import json
import logging
from pathlib import Path

import pytest

from esphome.core import EsphomeError, Library
import esphome.platformio.library as lib
from esphome.platformio.library import (
    SOURCE_KIND_FOR_SUFFIX,
    ConvertedLibrary,
    GitSource,
    InvalidLibrary,
    LibraryBackend,
    LocalSource,
    Source,
    URLSource,
    _resolve_registry_version,
    check_library_data,
    convert_libraries,
    join_flag_args,
    split_flag_entry,
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


def test_source_root_defaults_to_build_dir() -> None:
    # Registry/git sources are read from where they were downloaded.
    build = Path("/some/build/dir")
    assert URLSource("http://x/y.tar.gz").source_root(build) == build
    assert GitSource("http://x/y.git", None).source_root(build) == build


def test_converted_library_source_dir_defaults_to_path() -> None:
    c = ConvertedLibrary("x", "1.0", source=None)
    c.path = Path("/build")
    assert c.source_dir == Path("/build")  # no source_path set -> build dir
    c.source_path = Path("/user/lib")
    assert c.source_dir == Path("/user/lib")


def test_convert_libraries_local_missing_manifest_is_esphome_error(
    setup_core: Path,
) -> None:
    # A local directory that has no library.json/library.properties is user
    # input, so it must surface as a clean EsphomeError (named at the user's dir).
    src = setup_core / "not_a_lib"
    src.mkdir()  # exists, but no manifest
    # match= is a regex; a Windows path has backslashes, so match a literal
    # fragment and check the directory is named separately.
    with pytest.raises(EsphomeError, match="missing library.json") as excinfo:
        convert_libraries([Library("Foo", None, src.as_uri())], _backend())
    assert str(src) in str(excinfo.value)


def test_localsource_download_missing_dir_raises(tmp_path: Path) -> None:
    # EsphomeError so the CLI prints it cleanly instead of a traceback.
    with pytest.raises(EsphomeError, match="does not exist"):
        LocalSource(str(tmp_path / "nope")).download("mylib")


def test_localsource_str() -> None:
    assert str(LocalSource("/tmp/lib")) == "file:///tmp/lib"
    # A relative path can't form a file:// URI; fall back rather than raise.
    assert str(LocalSource("rel/lib")) == "file://rel/lib"


def test_localsource_download_returns_empty_build_dir(setup_core: Path) -> None:
    # Nothing is copied: download() returns an empty build dir (for generated
    # files), and source_root() points back at the user's directory.
    src = setup_core / "lib_dev"
    (src / "src").mkdir(parents=True)
    (src / "library.json").write_text("{}")
    (src / "src" / "a.cpp").write_text("int a;")

    source = LocalSource(str(src))
    out = source.download("mylib", salt="s", namespace="ns")

    assert out.is_dir()
    assert list(out.iterdir()) == []  # no sources copied in
    assert out != src
    assert source.source_root(out) == src

    # salt/namespace change the cache path.
    plain = LocalSource(str(src)).download("mylib")
    assert plain != out


def test_urlsource_download_extracts_then_reuses_marker(
    setup_core, monkeypatch, caplog
):
    monkeypatch.setattr(lib, "rmdir", lambda path, msg="": None)
    dl_calls: list[list[str]] = []

    def fake_download_and_extract(urls, subs, archive_path, extract_dir, **kwargs):
        dl_calls.append(urls)
        Path(extract_dir).mkdir(parents=True, exist_ok=True)

    monkeypatch.setattr(lib, "download_and_extract", fake_download_and_extract)

    src = URLSource("http://example.test/lib.tar.gz")
    out = src.download("mylib")

    assert (out / ".esphome_extracted").is_file()
    assert dl_calls == [["http://example.test/lib.tar.gz"]]

    # The completion marker means a second download is skipped (cache hit).
    out2 = src.download("mylib")
    assert out2 == out
    assert len(dl_calls) == 1

    # A batch caller passes a tracker and owns the messaging; no per-file INFO
    caplog.set_level("INFO")
    src.download("mylib-batch", progress=lambda done: None)
    assert len(dl_calls) == 2
    assert "Downloading" not in caplog.text


def test_urlsource_downloads_to_sibling_archive_path(setup_core, monkeypatch):
    """The archive downloads to a deterministic path next to the cache dir
    (not a random temp file), so an interrupted download's .part file
    resumes on the next run."""
    monkeypatch.setattr(lib, "rmdir", lambda path, msg="": None)
    targets: list[Path] = []

    def fake_download_and_extract(urls, subs, archive_path, extract_dir, **kwargs):
        targets.append(Path(archive_path))
        Path(extract_dir).mkdir(parents=True, exist_ok=True)

    monkeypatch.setattr(lib, "download_and_extract", fake_download_and_extract)

    src = URLSource("http://example.test/lib.tar.gz")
    out = src.download("mylib")

    assert targets == [out.with_name(f"{out.name}.archive")]


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
            None,
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


def test_wave_requirement_growth_defers_the_superseded_download(tmp_path, monkeypatch):
    """A's manifest constrains B while B sits in the same wave: B's
    drain-time resolution is superseded, so its download defers to the
    next wave instead of fetching a version that is immediately replaced."""
    download_names: list[str] = []
    manifests = {
        "esphome/A": {
            "name": "A",
            "build": {},
            "dependencies": {"esphome/B": ">=1.0"},
        },
        "esphome/B": {"name": "B", "build": {}},
    }

    def fake_download(self, force=False, salt="", namespace="", progress=None):
        download_names.append(self.name)
        self.path = tmp_path / self.get_require_name()
        self.path.mkdir(parents=True, exist_ok=True)
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(ConvertedLibrary, "download", fake_download)
    # Hermetic: the stubbed registry reports no size, so no batch prefetch
    _patch_registry_resolve(monkeypatch)
    top = convert_libraries(
        [Library("esphome/A", "1.0.0", None), Library("esphome/B", None, None)],
        _backend(),
    )
    assert sorted(c.name for c in top) == ["esphome/A", "esphome/B"]
    # B downloads exactly once, after its requirement set stabilized
    assert download_names.count("esphome/B") == 1


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
        self: ConvertedLibrary,
        force: bool = False,
        salt: str = "",
        namespace: str = "",
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
    # after exactly one retry (no retry loop). The error must name the cache
    # directory so users can find the broken entry instead of guessing where
    # the library was unpacked.
    calls = _patch_download_without_manifest(
        monkeypatch, tmp_path, manifest_on_force=False
    )

    with pytest.raises(RuntimeError, match="Invalid PIO library") as excinfo:
        convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())

    assert calls == [False, True]
    assert str(tmp_path / "esphome__A") in str(excinfo.value)


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


def test_convert_libraries_file_url_resolves_as_local(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # A "Name=file://<dir>" library points at an on-disk folder: it resolves as a
    # local source read in place (no copy), and the registry is never consulted.
    src = setup_core / "lib_dev"
    (src / "src").mkdir(parents=True)
    (src / "library.json").write_text(json.dumps({"name": "TeslaBLE"}))
    (src / "src" / "tesla.cpp").write_text("int foo() { return 1; }")

    def fail_registry(owner: str, pkgname: str, requirements: set[str]) -> None:
        raise AssertionError(f"registry consulted for {owner}/{pkgname}")

    monkeypatch.setattr(lib, "_resolve_registry_version", fail_registry)

    # as_uri() produces a valid file:// URL on every platform (file:///tmp/... on
    # POSIX, file:///C:/... on Windows).
    top = convert_libraries([Library("TeslaBLE", None, src.as_uri())], _backend())

    assert [c.name for c in top] == ["TeslaBLE"]
    assert top[0].data["name"] == "TeslaBLE"
    assert isinstance(top[0].source, LocalSource)
    # Sources are read in place from the user's dir; the build dir stays separate
    # and holds no copied sources.
    assert top[0].source_path == src
    assert top[0].path != src
    assert not (top[0].path / "src").exists()


def test_convert_libraries_local_overrides_registry_version(
    setup_core: Path,
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    # The same library requested both from the registry (with a version) and as
    # a local directory resolves to the local source, with a warning that the
    # registry version was dropped.
    src = setup_core / "lib_dev"
    (src / "src").mkdir(parents=True)
    (src / "library.json").write_text(json.dumps({"name": "TeslaBLE"}))

    def fail_registry(owner: str, pkgname: str, requirements: set[str]) -> None:
        raise AssertionError(f"registry consulted for {owner}/{pkgname}")

    monkeypatch.setattr(lib, "_resolve_registry_version", fail_registry)

    with caplog.at_level(logging.WARNING, logger="esphome.platformio.library"):
        top = convert_libraries(
            [
                Library("TeslaBLE", "1.0.0", None),
                Library("TeslaBLE", None, src.as_uri()),
            ],
            _backend(),
        )

    assert isinstance(top[0].source, LocalSource)
    assert "local source" in caplog.text


def test_convert_libraries_versionless_registry_and_local_warns(
    setup_core: Path,
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    # A bare cg.add_library("Foo") (versionless registry, the common case) that
    # collides with a local directory of the same key must still warn -- the
    # registry spec is dropped and the local folder silently takes over.
    src = setup_core / "foo"
    src.mkdir()
    (src / "library.json").write_text(json.dumps({"name": "Foo"}))

    def fail_registry(owner: str, pkgname: str, requirements: set[str]) -> None:
        raise AssertionError(f"registry consulted for {owner}/{pkgname}")

    monkeypatch.setattr(lib, "_resolve_registry_version", fail_registry)

    with caplog.at_level(logging.WARNING, logger="esphome.platformio.library"):
        top = convert_libraries(
            [Library("Foo", None, None), Library("Foo", None, src.as_uri())],
            _backend(),
        )

    assert isinstance(top[0].source, LocalSource)
    assert "a registry package" in caplog.text


def test_convert_libraries_two_local_dirs_warns(
    setup_core: Path,
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    # The same key pointed at two local directories warns and uses the last one.
    dir_a = setup_core / "a"
    dir_b = setup_core / "b"
    for d in (dir_a, dir_b):
        d.mkdir()
        (d / "library.json").write_text(json.dumps({"name": "Foo"}))

    with caplog.at_level(logging.WARNING, logger="esphome.platformio.library"):
        top = convert_libraries(
            [
                Library("Foo", None, dir_a.as_uri()),
                Library("Foo", None, dir_b.as_uri()),
            ],
            _backend(),
        )

    assert isinstance(top[0].source, LocalSource)
    assert top[0].source_path == dir_b  # the last one wins
    assert "two local directories" in caplog.text


@pytest.mark.parametrize("local_first", [True, False])
def test_convert_libraries_git_and_local_same_key_warns(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
    local_first: bool,
) -> None:
    # A key requested as both a git source and a local directory warns and uses
    # git, whichever order they appear in. The git URL basename matches the local
    # custom name so both map to the key "Foo".
    _patch_download_with_manifests(monkeypatch, tmp_path, {"Foo": {"name": "Foo"}})
    git = Library("X", None, "https://host/Foo")
    local = Library("Foo", None, "file:///abs/foo")
    libs = [local, git] if local_first else [git, local]

    with caplog.at_level(logging.WARNING, logger="esphome.platformio.library"):
        top = convert_libraries(libs, _backend())

    assert isinstance(top[0].source, GitSource)
    assert "using the git source" in caplog.text


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


def test_split_flag_entry_unbalanced_quote_is_clean() -> None:
    """A malformed flags entry raises EsphomeError, not a raw ValueError."""

    assert split_flag_entry('-DX="a b"', "library x") == ["-DX=a b"]
    with pytest.raises(EsphomeError, match=r"Malformed build flag.*library x"):
        split_flag_entry('-DX="unclosed', "library x")


def test_join_flag_args_reglues_spaced_define() -> None:
    """A spaced -D re-glues to its argument, as ParseFlags does."""

    assert join_flag_args(["-D", "FOO=1", "-Os"], "x") == ["-DFOO=1", "-Os"]


def test_join_flag_args_trailing_bare_flag_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:

    assert join_flag_args(["-Os", "-l"], "library x") == ["-Os"]
    assert "Ignoring trailing '-l'" in caplog.text


def test_lex_build_flags_dangling_flag_does_not_cross_entries(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Each entry is lexed independently, as ParseFlags does: a dangling -I
    ending one entry warns instead of absorbing the next entry's first token."""
    from esphome.platformio.library import lex_build_flags

    assert lex_build_flags(["-Wall -I", "-DFOO=1"], "lib x") == ["-Wall", "-DFOO=1"]
    assert "Ignoring trailing '-I'" in caplog.text


def test_prefetch_wave_downloads_registry_archives_in_parallel(
    setup_core, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """Registry archives in one wave download concurrently, deduped by URL;
    git/local sources and failures are left to the sequential call."""
    calls: list[str] = []

    def fake_download(
        self, dir_suffix, force=False, salt="", namespace="", progress=None
    ):
        calls.append(self.url)
        if progress is not None:
            progress(0)
        if "boom" in self.url:
            raise RuntimeError("boom")

    monkeypatch.setattr(URLSource, "download", fake_download)
    wave = [
        ("a", ConvertedLibrary("a", "1.0", URLSource("https://x/a.tar.gz", 1))),
        ("b", ConvertedLibrary("b", "1.0", URLSource("https://x/b.tar.gz", 1))),
        # Duplicate URL must prefetch once (two threads must never extract
        # into the same cache directory)
        ("b2", ConvertedLibrary("b2", "1.0", URLSource("https://x/b.tar.gz", 1))),
        ("c", ConvertedLibrary("c", "1.0", URLSource("https://x/boom.tar.gz", 1))),
        ("g", ConvertedLibrary("g", "*", lib.GitSource("https://x/g.git", None))),
    ]
    lib._prefetch_wave(wave, "", "idf")
    assert sorted(calls) == [
        "https://x/a.tar.gz",
        "https://x/b.tar.gz",
        "https://x/boom.tar.gz",
    ]
    # The failure surfaces at default verbosity, after the bar
    assert "Prefetch of c failed (retrying sequentially)" in caplog.text


def test_prefetch_wave_unknown_size_left_to_sequential(
    setup_core, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Archives without a registry-reported size skip the batch (their
    sequential per-file bars don't interleave); the known subset still
    prefetches."""
    calls: list[str] = []
    monkeypatch.setattr(
        URLSource,
        "download",
        lambda self, dir_suffix, force=False, salt="", namespace="", progress=None: (
            calls.append(self.url)
        ),
    )
    wave = [
        ("a", ConvertedLibrary("a", "1.0", URLSource("https://x/a.tar.gz", 1))),
        ("b", ConvertedLibrary("b", "1.0", URLSource("https://x/b.tar.gz", 1))),
        ("u", ConvertedLibrary("u", "1.0", URLSource("https://x/u.tar.gz"))),
    ]
    lib._prefetch_wave(wave, "", "idf")
    assert sorted(calls) == ["https://x/a.tar.gz", "https://x/b.tar.gz"]


def test_join_flag_args_empty_argument_warns_and_drops(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An empty glued argument is dropped: a bare -D would eat the next flag."""
    assert lib.lex_build_flags('-D "" -DFOO', "build_flags") == ["-DFOO"]
    assert "Ignoring '-D' with empty argument in build_flags" in caplog.text


def test_prefetch_wave_cache_probe_failure_still_prefetches(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A filesystem probe failure warns (a systematic one re-downloads
    everything) but still prefetches; a programming error is NOT swallowed
    here, it reaches the outer blanket guard."""
    calls: list[str] = []
    monkeypatch.setattr(
        URLSource,
        "download",
        lambda self, dir_suffix, **kw: calls.append(self.url),
    )
    monkeypatch.setattr(
        URLSource,
        "is_cached",
        lambda self, *a, **kw: (_ for _ in ()).throw(OSError("cache root denied")),
    )
    wave = [
        ("a", ConvertedLibrary("a", "1.0", URLSource("https://x/a.tar.gz", 1))),
        ("b", ConvertedLibrary("b", "1.0", URLSource("https://x/b.tar.gz", 1))),
    ]
    lib._prefetch_wave(wave, "", "idf")
    assert sorted(calls) == ["https://x/a.tar.gz", "https://x/b.tar.gz"]
    assert "Cache probe for a failed: cache root denied" in caplog.text


def test_prefetch_wave_internal_error_never_fails_the_build(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """The blanket guard keeps a prefetch bug from failing the walk."""
    monkeypatch.setattr(URLSource, "is_cached", lambda self, *a, **kw: False)
    monkeypatch.setattr(
        lib,
        "run_batch_downloads",
        lambda *a, **kw: (_ for _ in ()).throw(RuntimeError("bug")),
    )
    wave = [
        ("a", ConvertedLibrary("a", "1.0", URLSource("https://x/a.tar.gz", 1))),
        ("b", ConvertedLibrary("b", "1.0", URLSource("https://x/b.tar.gz", 1))),
    ]
    lib._prefetch_wave(wave, "", "idf")
    assert "Library prefetch failed: bug" in caplog.text


def test_prefetch_wave_warm_cache_is_silent(
    setup_core, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """Already-extracted archives download nothing; a warm build must not
    print a Downloading line or draw a bar."""
    monkeypatch.setattr(
        URLSource,
        "download",
        lambda self, dir_suffix, **kw: (_ for _ in ()).throw(
            AssertionError("downloaded")
        ),
    )
    wave = []
    for name in ("a", "b", "c"):
        comp = ConvertedLibrary(name, "1.0", URLSource(f"https://x/{name}.tar.gz", 1))
        marker_dir = comp.source._cache_dir(comp.get_sanitized_name(), "", "idf")
        marker_dir.mkdir(parents=True)
        (marker_dir / ".esphome_extracted").touch()
        wave.append((name, comp))
    lib._prefetch_wave(wave, "", "idf")
    assert "Downloading" not in caplog.text


def test_prefetch_wave_single_archive_uses_the_batch(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A dependency chain discovers one archive per wave; it downloads
    through the same runner so there is one download method and one bar."""
    caplog.set_level("INFO")
    calls: list[str] = []
    monkeypatch.setattr(URLSource, "is_cached", lambda self, *a, **kw: False)
    monkeypatch.setattr(
        URLSource,
        "download",
        lambda self, dir_suffix, force=False, salt="", namespace="", progress=None: (
            calls.append(self.url)
        ),
    )
    lib._prefetch_wave(
        [("a", ConvertedLibrary("a", "1.0", URLSource("https://x/a.tar.gz", 1)))],
        "",
        "idf",
    )
    assert calls == ["https://x/a.tar.gz"]
    assert "Downloading 1 library archive(s): a" in caplog.text


def test_normalize_dependencies_forms(caplog) -> None:
    """Every PIO-legal spelling normalizes; unrecognizable entries warn."""
    from esphome.platformio.library import normalize_dependencies

    assert normalize_dependencies(
        ["Wire", {"name": "SPI"}, 5, "", {"version": "1.0"}], "libx"
    ) == [
        {"name": "Wire"},
        {"name": "SPI"},
    ]
    # The int, the empty string, and the nameless dict all warn
    assert caplog.text.count("unrecognized dependency entry") == 3
    # A plain string is names, never iterated into characters
    assert normalize_dependencies("Wire, SPI") == [
        {"name": "Wire"},
        {"name": "SPI"},
    ]
    assert normalize_dependencies("Wire") == [{"name": "Wire"}]
    # A non-iterable value fails by manifest name, never a bare TypeError
    assert normalize_dependencies(5, "libx") == []
    assert "Ignoring unrecognized dependencies 5 of libx" in caplog.text
    # The dict-shorthand form validates names like the list form: an empty
    # key and a spec overriding name with a non-string both warn and drop
    assert normalize_dependencies(
        {"": "1.0", "Wire": {"name": 123, "version": "1.0"}, "SPI": "*"}, "libx"
    ) == [{"name": "SPI", "owner": None, "version": "*"}]
    assert caplog.text.count("unrecognized dependency entry") == 5
    # A container or numeric version would raise from set.add() or fail
    # opaquely in the registry; both spellings warn and drop
    assert normalize_dependencies({"Foo": ["1.0", "2.0"]}, "libx") == []
    assert normalize_dependencies([{"name": "Foo", "version": 1}], "libx") == []
    assert caplog.text.count("unrecognized dependency entry") == 7
    # A non-string owner would stringify into a malformed registry name
    assert (
        normalize_dependencies(
            [{"name": "Foo", "owner": {"bad": 1}, "version": "1.0"}], "libx"
        )
        == []
    )
    # A falsey scalar (0, false) is malformed, not an empty list
    assert normalize_dependencies(0, "libx") == []
    assert "Ignoring unrecognized dependencies 0 of libx" in caplog.text


@pytest.mark.parametrize(
    "manifest",
    [
        ["not", "a", "manifest"],
        {"name": "A", "build": "src"},
        {"name": "A", "ESPHOME": "yes"},
        {"name": "A", "build": {"srcDir": 123}},
        {"name": "A", "build": {"includeDir": ["inc"]}},
        {"name": "A", "build": {"srcFilter": {"+": "src"}}},
        {"name": "A", "ESPHOME": {"LINK_FLAGS": "-Wl,-x"}},
    ],
)
def test_convert_libraries_malformed_manifest_raises(
    tmp_path, monkeypatch, manifest
) -> None:
    """A manifest without the expected dict shape fails by library name
    before any backend dereferences data/build."""
    _patch_download_with_manifests(monkeypatch, tmp_path, {"esphome/A": manifest})
    with pytest.raises(EsphomeError, match="has a malformed manifest"):
        convert_libraries([Library("esphome/A", None, None)], _backend())


def test_convert_libraries_malformed_transitive_dep_skips(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A malformed manifest on a dependency the user never asked for warns
    and skips; only a top-level library fails the build."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "B", "owner": "esphome", "version": "1.0"}],
            },
            "esphome/B": {"name": "B", "build": {"srcDir": 123}},
        },
    )
    components = convert_libraries([Library("esphome/A", None, None)], _backend())
    names = [c.name for c in components]
    assert "esphome/A" in names
    assert "esphome/B" not in names
    assert "Skipping dependency esphome/B: malformed manifest" in caplog.text


def test_walk_warns_for_properties_only_depends(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A manifest declaring dependencies only as library.properties depends=
    warns in the shared walk, so every backend reports the drop."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {"esphome/A": "name=A\nversion=1.0\ndepends=Wire, SPI\n"},
        properties=("esphome/A",),
    )
    caplog.set_level("INFO")
    convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())
    assert "declares dependencies via library.properties" in caplog.text


def test_walk_warns_for_nonplatform_invalid_library(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A dependency dropped for any cause other than the routine platform
    filter is visible in every backend."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "B", "version": "1.0", "platforms": [123]}],
            }
        },
    )
    convert_libraries([Library("esphome/A", None, None)], _backend())
    assert "Skipping dependency B of esphome/A: Malformed platforms" in caplog.text


def test_convert_libraries_warns_for_nonplatform_invalid_dependency_component(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A dependency component dropped for any cause other than the platform
    filter warns; only the routine cross-platform skip stays at debug."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "C", "owner": "esphome", "version": "1.0"}],
            },
            "esphome/C": {"name": "C", "frameworks": [None]},
        },
    )
    convert_libraries([Library("esphome/A", "1.0.0", None)], _backend())
    assert "Malformed frameworks" in caplog.text
    assert "Skipping dependency" in caplog.text


def test_split_flag_entry_non_string_is_clean() -> None:
    """A dict or number from a third-party manifest fails naming the entry,
    not with an opaque shlex traceback."""

    with pytest.raises(EsphomeError, match="Malformed build flag"):
        split_flag_entry({"esp32": ["-DX"]}, "lib x")
    with pytest.raises(EsphomeError, match="Malformed build flag 5"):
        split_flag_entry(5, "lib x")


def test_source_kind_map_shape() -> None:
    """The kind values the native compile rules key on; the AS/ASPP split
    matches SCons (.S preprocessed, .s plain assembler)."""

    assert set(SOURCE_KIND_FOR_SUFFIX.values()) == {"c", "cxx", "asm", "aspp"}
    assert SOURCE_KIND_FOR_SUFFIX[".s"] == "asm"
    assert SOURCE_KIND_FOR_SUFFIX[".S"] == "aspp"
    assert SOURCE_KIND_FOR_SUFFIX[".c"] == "c"
    assert SOURCE_KIND_FOR_SUFFIX[".cpp"] == "cxx"
