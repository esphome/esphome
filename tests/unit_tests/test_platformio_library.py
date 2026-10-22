"""Tests for the toolchain-agnostic PlatformIO library converter.

Covers the shared download/parse/resolve/dependency-walk paths in
``esphome.platformio.library`` directly (the ESP-IDF and Zephyr backends are
exercised in their own test modules)."""

from collections.abc import Collection
import json
import logging
from pathlib import Path
from unittest.mock import Mock

from platformio.package.exception import UnknownPackageError
import pytest

from esphome.core import CORE, EsphomeError, Library
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


def _backend(emit=lambda component: None, provides=None) -> LibraryBackend:
    return LibraryBackend(
        platform="espressif32",
        framework="espidf",
        emit=emit,
        cache_key="idf",
        provides=provides,
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


def test_make_registry_client_skips_private_package_probe(monkeypatch):
    """Our client answers the probe locally without patching PlatformIO's class."""
    from platformio.account.client import AccountClient
    from platformio.registry.client import RegistryClient

    pio_probe = RegistryClient.__dict__["allowed_private_packages"]
    monkeypatch.setattr(
        AccountClient,
        "get_account_info",
        Mock(side_effect=AssertionError("account probe must not run")),
    )

    client = lib._make_registry_client().get_registry_client_instance()

    assert client.allowed_private_packages() is False
    assert RegistryClient.__dict__["allowed_private_packages"] is pio_probe


def test_make_registry_client_creates_http_cache_dir(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """The HTTP cache dir exists before PlatformIO's non-exist_ok makedirs runs."""
    from platformio.project import helpers

    monkeypatch.setattr(helpers, "get_project_cache_dir", lambda: str(tmp_path))

    lib._make_registry_client()
    assert (tmp_path / "http").is_dir()
    # A second client (another build) must not trip over the existing dir
    lib._make_registry_client()


def _patch_registry_resolve(
    monkeypatch: pytest.MonkeyPatch,
    *,
    unknown: Collection[str] = (),
    owner: str | None = None,
) -> list[tuple[str | None, str]]:
    """Stub the registry lookup so tests never touch the network; returns the
    (owner, name) pairs looked up. ``unknown`` names miss; ``owner`` is what the
    registry answers for an owner-less name."""
    asked: list[tuple[str | None, str]] = []

    def resolve(requested_owner, pkgname, requirements):
        asked.append((requested_owner, pkgname))
        if pkgname in unknown:
            raise UnknownPackageError(pkgname)
        return _resolution(requested_owner or owner, pkgname)

    monkeypatch.setattr(lib, "_resolve_registry_version", resolve)
    return asked


def _resolution(owner, pkgname):
    """What the stubbed registry answers for a package."""
    return owner, pkgname, "1.0.0", f"http://x/{pkgname}.tar.gz", None


def _patch_download_with_manifests(
    monkeypatch, tmp_path, manifests, *, properties=(), unknown=(), owner=None
):
    """Fake ConvertedLibrary.download to materialize canned manifests on disk;
    returns the registry lookups made (see _patch_registry_resolve)."""

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_require_name()
        self.path.mkdir(parents=True, exist_ok=True)
        if self.name in properties:
            (self.path / "library.properties").write_text(manifests[self.name])
        else:
            (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(ConvertedLibrary, "download", fake_download)
    return _patch_registry_resolve(monkeypatch, unknown=unknown, owner=owner)


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


def test_convert_libraries_fetches_dependency_without_version(tmp_path, monkeypatch):
    # A dependency entry lacking a version resolves by name alone, as PlatformIO did.
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "C"}]},
            "C": {"name": "C"},
        },
    )

    # No version on the top-level spec exercises the "no requirement" path too.
    top = convert_libraries([Library("esphome/A", None, None)], _backend())

    assert [d.name for d in top[0].dependencies] == ["C"]
    assert asked == [("esphome", "A"), (None, "C")]


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


def test_convert_libraries_manifest_optional_uses_default_layout(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A backend accepting manifest-less libraries (the host build, like
    PlatformIO's native platform) gets a default manifest instead of a
    re-download that would repeat every build."""
    calls = _patch_download_without_manifest(
        monkeypatch, tmp_path, manifest_on_force=True
    )
    emitted: list[ConvertedLibrary] = []
    backend = _backend(emit=emitted.append)
    backend.manifest_optional = True

    with caplog.at_level(logging.DEBUG, logger="esphome.platformio.library"):
        top = convert_libraries([Library("esphome/A", "1.0.0", None)], backend)

    assert calls == [False]
    assert top[0].data == {"name": "esphome/A"}
    assert emitted == top
    assert "has no manifest; using PlatformIO's default layout" in caplog.text


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
    local sources and failures are left to the sequential call."""
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
        ("l", ConvertedLibrary("l", "*", LocalSource("/some/lib"))),
    ]
    lib._prefetch_wave(wave, "", "idf")
    assert sorted(calls) == [
        "https://x/a.tar.gz",
        "https://x/b.tar.gz",
        "https://x/boom.tar.gz",
    ]
    # The failure surfaces at default verbosity, after the bar
    assert "Prefetch of c failed (retrying sequentially)" in caplog.text


def test_prefetch_wave_clones_git_sources_in_parallel(
    setup_core, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """Git sources join the same prefetch batch as the archives, deduped by
    clone target; a clone failure warns and is left to the sequential call."""
    caplog.set_level("INFO")
    calls: list[str] = []

    def fake_clone(self, dir_suffix, force=False, salt="", namespace=""):
        calls.append(f"{self}/{dir_suffix}")
        if "boom" in self.url:
            raise RuntimeError("boom")

    monkeypatch.setattr(GitSource, "download", fake_clone)
    wave = [
        ("a", ConvertedLibrary("a", "1.0", URLSource("https://x/a.tar.gz", 1))),
        ("g", ConvertedLibrary("g", "*", GitSource("https://x/g.git", "v1"))),
        # Same url@ref and target dir must clone once
        ("g2", ConvertedLibrary("g", "*", GitSource("https://x/g.git", "v1"))),
        ("h", ConvertedLibrary("h", "*", GitSource("https://x/boom.git", None))),
    ]
    monkeypatch.setattr(
        URLSource, "download", lambda self, dir_suffix, progress=None, **kw: None
    )
    lib._prefetch_wave(wave, "", "idf")
    assert sorted(calls) == ["https://x/boom.git/h", "https://x/g.git#v1/g"]
    assert "Cloning 2 library repo(s): g, h" in caplog.text
    assert "Prefetch of h failed (retrying sequentially)" in caplog.text


def test_source_base_prefetch_defaults() -> None:
    """The base Source is not prefetchable and reports cached (nothing to do)."""
    source = Source()
    assert source.prefetch_key("x") is None
    assert source.is_cached("x") is True


def test_prefetch_wave_single_clone_uses_the_batch(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A wave with only git sources still clones through the batch runner."""
    caplog.set_level("INFO")
    calls: list[str] = []
    monkeypatch.setattr(GitSource, "is_cached", lambda self, *a, **kw: False)
    monkeypatch.setattr(
        GitSource,
        "download",
        lambda self, dir_suffix, force=False, salt="", namespace="": calls.append(
            self.url
        ),
    )
    lib._prefetch_wave(
        [("g", ConvertedLibrary("g", "*", GitSource("https://x/g.git", None)))],
        "",
        "idf",
    )
    assert calls == ["https://x/g.git"]
    assert "Cloning 1 library repo(s): g" in caplog.text
    assert "Downloading" not in caplog.text


def test_prefetch_wave_warm_git_cache_is_silent(
    setup_core, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """An already-complete clone is neither re-fetched nor announced."""
    caplog.set_level("INFO")
    monkeypatch.setattr(
        GitSource,
        "download",
        lambda self, dir_suffix, **kw: (_ for _ in ()).throw(AssertionError("cloned")),
    )
    monkeypatch.setattr(GitSource, "is_cached", lambda self, *a, **kw: True)
    wave = [("g", ConvertedLibrary("g", "*", GitSource("https://x/g.git", None)))]
    lib._prefetch_wave(wave, "", "idf")
    assert "Cloning" not in caplog.text


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


def test_walk_resolves_properties_only_depends(tmp_path, monkeypatch) -> None:
    """A library.properties depends= line is a dependency list: a bundled name
    goes to the backend, the rest resolve by name."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": "name=A\nversion=1.0\ndepends=Wire, SPI\n",
            "SPI": {"name": "SPI"},
        },
        properties=("esphome/A",),
    )
    backend = _backend(provides=lambda name: name == "Wire")
    top = convert_libraries([Library("esphome/A", "1.0.0", None)], backend)
    assert [d.name for d in top[0].dependencies] == ["SPI"]
    assert "depends" not in top[0].data
    assert backend.provided_requests == {"Wire"}
    assert (None, "SPI") in asked


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
    # SCons's case-sensitive C++ suffixes: PIO compiles .C as C++
    assert SOURCE_KIND_FOR_SUFFIX[".C"] == "cxx"
    assert SOURCE_KIND_FOR_SUFFIX[".C++"] == "cxx"


def test_versionless_platform_filtered_dependency_is_not_fetched(
    tmp_path, monkeypatch
) -> None:
    """A version-less dependency the platform filter excludes is deliberately
    absent, not fetched by name."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "Hash", "platforms": "espressif8266"}],
            }
        },
    )
    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert top[0].dependencies == []
    assert (None, "Hash") not in asked


def test_versionless_ignored_dependency_is_not_fetched(tmp_path, monkeypatch) -> None:
    """A lib_ignore'd version-less dependency is deliberately excluded, not
    fetched by name."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {"esphome/A": {"name": "A", "dependencies": [{"name": "Hash"}]}},
    )
    CORE.platformio_options = {"lib_ignore": ["Hash"]}
    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert top[0].dependencies == []
    assert (None, "Hash") not in asked


def test_versionless_dependency_registry_miss_is_skipped(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A name-only dependency the registry does not know is skipped with one
    warning naming the requester, and the requester still converts; a listed
    library the registry does not know still fails the build."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                # The duplicate entry is one node, so it warns once
                "dependencies": [{"name": "Hash"}, {"name": "Hash"}],
            }
        },
        unknown={"Hash"},
    )

    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert top[0].dependencies == []
    assert (
        caplog.text.count("Dependency Hash of esphome/A was not resolved; skipping")
        == 1
    )
    with pytest.raises(UnknownPackageError):
        convert_libraries([Library("Hash", None, None)], _backend())


def test_url_version_dependency_is_not_substituted_by_provides(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A URL-valued version names one specific source; the backend-provided
    skip must not replace it with the bundled copy."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [
                    {"name": "Hash", "version": "https://github.com/o/Hash.git"}
                ],
            },
            "o/Hash": {"name": "Hash"},
        },
    )
    emitted: list[str] = []
    convert_libraries(
        [Library("esphome/A", "1.0.0", None)],
        _backend(emit=lambda c: emitted.append(c.name), provides=lambda name: True),
    )
    assert "Skip backend-provided" not in caplog.text
    assert "using the library bundled" not in caplog.text
    assert any("o/hash" in n.lower() for n in emitted)


def test_versionless_owner_qualified_dependency_is_fetched_despite_provides(
    tmp_path, monkeypatch
) -> None:
    """An owner-qualified version-less dependency names a registry package,
    so provides() does not stand in for it."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "Wire", "owner": "Foo"}],
            },
            "Foo/Wire": {"name": "Wire"},
        },
    )
    backend = _backend(provides=lambda name: name == "Wire")
    top = convert_libraries([Library("esphome/A", None, None)], backend)
    assert [d.name for d in top[0].dependencies] == ["Foo/Wire"]
    assert ("Foo", "Wire") in asked
    assert not backend.provided_requests


def test_versionless_url_ish_dependency_name_warns_cleanly(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A malformed URL-ish dependency name is skipped with a warning, never
    a RuntimeError out of the key parser."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {"esphome/A": {"name": "A", "dependencies": [{"name": "file://"}]}},
    )
    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert top[0].dependencies == []
    assert "Skipping dependency file:// of esphome/A" in caplog.text


def test_versionless_dependency_matching_resolved_manifest_name_is_wired(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A bare name no package name matches is satisfied by a listed component's
    manifest name (a fork published under another package name)."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "B"}]},
            "esphome/B-fork": {"name": "B"},
        },
    )
    with caplog.at_level(logging.INFO):
        top = convert_libraries(
            [Library("esphome/A", None, None), Library("esphome/B-fork", None, None)],
            _backend(),
        )
    assert [d.name for d in top[0].dependencies] == ["esphome/B-fork"]
    assert asked == [("esphome", "A"), ("esphome", "B-fork")]
    assert (
        "Dependency B of esphome/A satisfied by esphome/B-fork through its manifest name"
        in caplog.text
    )


def test_dependency_naming_the_requester_itself_is_ignored(
    tmp_path, monkeypatch
) -> None:
    """A manifest that lists its own name as a dependency neither fetches a
    second copy of itself nor gains a self edge."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": "name=A\ndepends=a, esphome/A\n",
            # No manifest name: the requested and resolved package names still
            # identify it, with or without the owner
            "esphome/C": {
                "dependencies": [{"name": "C"}, {"name": "C", "owner": "esphome"}]
            },
        },
        properties=("esphome/A",),
    )
    top = convert_libraries(
        [Library("esphome/A", "1.0.0", None), Library("esphome/C", "1.0.0", None)],
        _backend(),
    )
    assert [c.dependencies for c in top] == [[], []]
    assert asked == [("esphome", "A"), ("esphome", "C")]


def test_versionless_dependency_settles_after_the_graph(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """The issue 21308 shape: a library.properties depends= naming a library the
    config lists owner-qualified is wired to that one package, case-insensitively
    and whatever constraint it carries, without a second node for the bare name
    and ahead of a same-named bundled copy, as on the Arduino backend."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "adafruit/Adafruit GFX Library": (
                "name=Adafruit GFX Library\ndepends=adafruit busio (>=1.0)\n"
            ),
            "adafruit/Adafruit BusIO": {"name": "Adafruit BusIO"},
        },
        properties=("adafruit/Adafruit GFX Library",),
    )
    backend = _backend(provides=lambda name: name == "adafruit busio")
    top = convert_libraries(
        [
            Library("adafruit/Adafruit GFX Library", "^1.13.0", None),
            Library("adafruit/Adafruit BusIO", "^1.17.4", None),
        ],
        backend,
    )
    assert [d.name for d in top[0].dependencies] == ["adafruit/Adafruit BusIO"]
    assert len(asked) == 2
    assert not backend.provided_requests
    assert "multiple names" not in caplog.text


def test_by_name_node_folds_into_a_later_pin_of_the_same_package(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A library fetched by name alone may pull in a manifest that pins the same
    package owner-qualified; the by-name node folds into that one instead of
    converting the package twice."""
    # Manifests are keyed by the resolved name, so both B nodes read the same one:
    # B pins C, and C pins owner/B, which arrives after B was fetched by name
    manifests = {
        "esphome/A": {"name": "A", "dependencies": [{"name": "B"}]},
        "esphome/B": {"name": "B", "dependencies": {"esphome/C": "1.0.0"}},
        "esphome/C": {"name": "C", "dependencies": {"esphome/B": "1.0.0"}},
    }
    _patch_download_with_manifests(monkeypatch, tmp_path, manifests, owner="esphome")

    emitted: list[str] = []
    top = convert_libraries(
        [Library("esphome/A", None, None)],
        _backend(emit=lambda c: emitted.append(c.name)),
    )
    assert [d.name for d in top[0].dependencies] == ["esphome/B"]
    assert emitted.count("esphome/B") == 1
    assert "multiple names" not in caplog.text


def test_two_spellings_of_a_name_in_one_round_share_a_node(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """Requesters spelling one unlisted name differently get a single by-name
    fetch and the same node."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "Foo"}]},
            "esphome/B": {"name": "B", "dependencies": [{"name": "foo"}]},
            "Foo": {"name": "Foo"},
        },
    )
    top = convert_libraries(
        [Library("esphome/A", None, None), Library("esphome/B", None, None)],
        _backend(),
    )
    assert [d.name for d in top[0].dependencies] == ["Foo"]
    assert [d.name for d in top[1].dependencies] == ["Foo"]
    assert asked.count((None, "Foo")) + asked.count((None, "foo")) == 1
    assert "multiple names" not in caplog.text


def test_parse_properties_depends_reads_names_and_drops_constraints(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Arduino constraints are not PlatformIO syntax, so only the names survive;
    an unclosed parenthesis loses nothing more than the constraint."""
    with caplog.at_level(logging.DEBUG):
        entries = lib.parse_properties_depends(
            "Adafruit BusIO, Foo (>=1.2), ,Bar( <2 ), Baz (=1.0, Qux ()"
        )
    levels = {
        r.message.removeprefix("Ignoring version constraint on depends="): r.levelno
        for r in caplog.records
        if "Ignoring version constraint" in r.message
    }
    # A lower bound is met by the current version; a cap or pin may not be
    assert levels == {
        "Foo (>=1.2)": logging.DEBUG,
        "Bar( <2 )": logging.INFO,
        "Baz (=1.0": logging.INFO,
        "Qux ()": logging.INFO,
    }
    assert entries == [
        {"name": "Adafruit BusIO"},
        {"name": "Foo"},
        {"name": "Bar"},
        {"name": "Baz"},
        {"name": "Qux"},
    ]
    assert "Ignoring version constraint on depends=Foo (>=1.2)" in caplog.text


def test_by_name_node_pinned_later_reports_a_requirement_conflict(
    tmp_path, monkeypatch
) -> None:
    """A node fetched by name that a later manifest pins under the same spelling
    is resolved again against the pin; a conflict is an error, not a miss."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "B"}]},
            "B": {"name": "B", "dependencies": {"C": "1.0.0"}},
            "C": {"name": "C", "dependencies": {"B": "^2.0"}},
        },
    )

    def resolve(owner, pkgname, requirements):
        if pkgname == "B" and requirements:
            raise RuntimeError(f"No version of {pkgname} satisfies {requirements}")
        return _resolution(owner, pkgname)

    monkeypatch.setattr(lib, "_resolve_registry_version", resolve)
    with pytest.raises(RuntimeError, match="No version of B satisfies"):
        convert_libraries([Library("esphome/A", None, None)], _backend())


def test_by_name_fetch_yields_to_a_fork_pulled_in_later(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A name fetched from the registry yields to a library another round
    pins explicitly under the same manifest name, so only the fork is built."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "B"}]},
            "esphome/X": {"name": "X", "dependencies": [{"name": "C"}]},
            # The upstream B pulls in D, which nothing else needs; D asks for a
            # name the registry does not know
            "B": {"name": "B", "dependencies": {"esphome/D": "1.0.0"}},
            "esphome/D": {"name": "D", "dependencies": [{"name": "Zed"}]},
            "C": {"name": "C", "dependencies": {"custom/B-fork": "1.0.0"}},
            "custom/B-fork": {"name": "B"},
        },
        unknown={"Zed"},
    )

    emitted: list[str] = []
    top = convert_libraries(
        [Library("esphome/A", None, None), Library("esphome/X", None, None)],
        _backend(emit=lambda c: emitted.append(c.name)),
    )
    assert [d.name for d in top[0].dependencies] == ["custom/B-fork"]
    assert sorted(emitted) == ["C", "custom/B-fork", "esphome/A", "esphome/X"]
    # The miss belongs to a library that is not built, so it is not reported
    assert "Zed" not in caplog.text


def test_git_source_replacing_a_registry_key_in_the_same_wave(
    tmp_path, monkeypatch
) -> None:
    """A manifest in one wave naming a key of the same wave as a git source
    defers that key to the next wave instead of tripping over its stale record."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "o/A": {"name": "A", "dependencies": {"B": "https://github.com/o/B.git"}},
            "o/B": {"name": "B"},
        },
    )
    emitted: dict[str, ConvertedLibrary] = {}
    top = convert_libraries(
        [Library("o/A", "1.0.0", None), Library("o/B", "1.0.0", None)],
        _backend(emit=lambda c: emitted.__setitem__(c.name, c)),
    )
    assert [c.name for c in top] == ["o/A", "o/B"]
    assert isinstance(emitted["o/B"].source, GitSource)
    assert [d.name for d in top[0].dependencies] == ["o/B"]


def test_owner_qualified_name_matches_the_resolved_owner(tmp_path, monkeypatch) -> None:
    """A version-less ``o/B`` is satisfied by a bare ``B`` pin the registry
    resolved to owner ``o``, so the package is not fetched a second time."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [
                    {"name": "B", "version": "1.0.0"},
                    {"name": "B", "owner": "o"},
                ],
            },
            "o/B": {"name": "B"},
        },
        owner="o",
    )
    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert [d.name for d in top[0].dependencies] == ["o/B"]
    assert asked.count((None, "B")) == 1
    assert ("o", "B") not in asked


def test_two_spellings_fetched_by_name_fold_into_one_node(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """Version-less ``B`` and ``o/B`` from two requesters resolve to one
    package; only one copy is built and both requesters depend on it."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "B"}]},
            "esphome/X": {"name": "X", "dependencies": [{"name": "B", "owner": "o"}]},
            "o/B": {"name": "B"},
        },
        owner="o",
    )
    emitted: list[str] = []
    top = convert_libraries(
        [Library("esphome/A", None, None), Library("esphome/X", None, None)],
        _backend(emit=lambda c: emitted.append(c.name)),
    )
    assert emitted.count("o/B") == 1
    kept = top[0].dependencies[0]
    assert top[1].dependencies == [kept]
    assert "multiple names" not in caplog.text


def test_registry_miss_resolves_again_as_a_git_source(tmp_path, monkeypatch) -> None:
    """A key the registry missed is resolved again when a later manifest names
    the same key as a git source."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "B", "owner": "o"}]},
            "esphome/X": {"name": "X", "dependencies": [{"name": "C"}]},
            "C": {"name": "C", "dependencies": {"B": "https://github.com/o/B.git"}},
            "o/B": {"name": "B"},
        },
        unknown={"B"},
    )

    emitted: dict[str, ConvertedLibrary] = {}
    top = convert_libraries(
        [Library("esphome/A", None, None), Library("esphome/X", None, None)],
        _backend(emit=lambda c: emitted.__setitem__(c.name, c)),
    )
    assert [d.name for d in top[0].dependencies] == ["o/B"]
    assert isinstance(emitted["o/B"].source, GitSource)
    assert [d.name for d in emitted["C"].dependencies] == ["o/B"]


def test_empty_version_is_settled_like_a_missing_one(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """``{"Foo": ""}`` carries no version either, so it is matched, fetched by
    name or reported like a version-less entry rather than failing or vanishing."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": {"Foo": "", "Bar": ""}},
            "Bar": {"name": "Bar"},
        },
        unknown={"Foo"},
    )
    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert [d.name for d in top[0].dependencies] == ["Bar"]
    assert (None, "Foo") in asked
    assert "Dependency Foo of esphome/A was not resolved; skipping" in caplog.text


def test_bundled_name_is_satisfied_by_a_replacement_pulled_in_later(
    tmp_path, monkeypatch
) -> None:
    """A name the framework bundles is still wired to an explicit replacement
    of the same manifest name that a later round pulls in, as it would be had
    the replacement been listed up front."""
    _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "Ethernet"}]},
            "esphome/X": {"name": "X", "dependencies": [{"name": "C"}]},
            "C": {"name": "C", "dependencies": {"vendor/Ethernet": "1.0.0"}},
            "vendor/Ethernet": {"name": "Ethernet"},
        },
    )
    backend = _backend(provides=lambda name: name == "Ethernet")
    top = convert_libraries(
        [Library("esphome/A", None, None), Library("esphome/X", None, None)],
        backend,
    )
    assert [d.name for d in top[0].dependencies] == ["vendor/Ethernet"]


def test_missed_name_is_satisfied_by_a_library_pulled_in_later(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A bare name the registry does not know is wired to a library a later
    round pulls in under another package name but the same manifest name."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "B"}]},
            "esphome/X": {"name": "X", "dependencies": [{"name": "C"}]},
            # Fetched by name in the same round as the miss; pins the fork
            "C": {"name": "C", "dependencies": {"owner/B-fork": "1.0.0"}},
            "owner/B-fork": {"name": "B"},
        },
        unknown={"B"},
    )
    with caplog.at_level(logging.INFO):
        top = convert_libraries(
            [Library("esphome/A", None, None), Library("esphome/X", None, None)],
            _backend(),
        )
    assert [d.name for d in top[0].dependencies] == ["owner/B-fork"]
    assert asked.count((None, "B")) == 1
    assert (
        "Dependency B of esphome/A satisfied by owner/B-fork through its manifest name"
        in caplog.text
    )
    assert "skipping" not in caplog.text


def test_by_name_request_does_not_make_an_existing_node_optional(
    tmp_path, monkeypatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A version-less request for a node the walk already created (here one
    skipped for its platform) neither re-fetches it nor turns a later miss
    into a warning."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {
                "name": "A",
                "dependencies": [{"name": "X", "version": "1.0"}, {"name": "X"}],
            },
            "X": {"name": "X", "platforms": "espressif8266"},
        },
    )
    top = convert_libraries([Library("esphome/A", None, None)], _backend())
    assert top[0].dependencies == []
    assert asked == [("esphome", "A"), (None, "X")]
    assert "not in the registry" not in caplog.text


def test_provided_dependency_asks_the_backend_once_per_name(
    tmp_path, monkeypatch
) -> None:
    """A bundled name is recorded for the backend and never fetched; two
    requesters cost one provides() call, so a backend warning there is not
    repeated. A pinned request on the same name shares the memo."""
    asked = _patch_download_with_manifests(
        monkeypatch,
        tmp_path,
        {
            "esphome/A": {"name": "A", "dependencies": [{"name": "Wire"}]},
            "esphome/B": {"name": "B", "dependencies": {"Wire": "*"}},
        },
    )
    calls: list[str] = []

    def provides(name: str) -> bool:
        calls.append(name)
        return name == "Wire"

    backend = _backend(provides=provides)
    convert_libraries(
        [Library("esphome/A", None, None), Library("esphome/B", None, None)],
        backend,
    )
    assert calls == ["Wire"]
    assert backend.provided_requests == {"Wire"}
    assert (None, "Wire") not in asked


def test_convert_libraries_symlink_url_resolves_as_local(setup_core: Path) -> None:
    """symlink:// is PlatformIO's other spelling for a local library folder."""
    src = setup_core / "lib_dev"
    (src / "src").mkdir(parents=True)
    (src / "library.json").write_text(json.dumps({"name": "benchmark"}))
    url = src.as_uri().replace("file://", "symlink://", 1)

    # Both the name=URL form and an explicit repository take the scheme
    for library in (
        Library(f"benchmark={url}", None, None),
        Library("benchmark", None, url),
    ):
        top = convert_libraries([library], _backend())
        assert isinstance(top[0].source, LocalSource)
        assert top[0].source_path == src


def test_convert_libraries_incompatible_names_the_platform_without_a_framework(
    setup_core: Path,
) -> None:
    """The host has no framework; the error must not read 'compatible with None'."""
    src = setup_core / "lib_dev"
    src.mkdir()
    (src / "library.json").write_text(
        json.dumps({"name": "Only32", "platforms": "espressif32"})
    )
    backend = _backend()
    backend.platform = "native"
    backend.framework = None
    with pytest.raises(RuntimeError, match="Only32 is not compatible with native"):
        convert_libraries([Library("Only32", None, src.as_uri())], backend)


@pytest.mark.parametrize(
    ("url", "message"),
    [
        ("symlink://host/lib", "Unsupported host in symlink:// library URL"),
        ("symlink:lib_dev", "symlink:// library URL .* must be an absolute"),
    ],
)
def test_convert_libraries_symlink_url_errors_name_the_scheme(
    setup_core: Path, url: str, message: str
) -> None:
    with pytest.raises(RuntimeError, match=message):
        convert_libraries([Library("benchmark", None, url)], _backend())
