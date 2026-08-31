import glob
import hashlib
import json
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from esphome.components import esp32 as esp32_module
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    Framework,
    Platform,
)
from esphome.core import CORE, Library
from esphome.espidf.component import (
    _emit_idf_component,
    generate_cmakelists_txt,
    generate_idf_component_yml,
    generate_idf_components,
)
import esphome.platformio.library
from esphome.platformio.library import (
    ESPHOME_DATA_KEY,
    ESPHOME_DATA_LINK_FLAGS_KEY,
    ConvertedLibrary as IDFComponent,
    GitSource,
    URLSource,
    _node_key,
    _resolve_registry_version,
    collect_filtered_files,
    normalize_dependencies,
    parse_library_json,
    parse_library_properties,
    split_list_by_condition,
)


@pytest.fixture(name="tmp_component")
def fixture_tmp_component(tmp_path):
    c = IDFComponent("owner/name", "1.0.0", source=MagicMock())
    c.path = tmp_path
    return c


@pytest.fixture(name="esp32_idf_core")
def fixture_esp32_idf_core():
    CORE.data[KEY_CORE] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = str(Platform.ESP32)
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = str(Framework.ESP_IDF)


def test_idf_component_str():
    c = IDFComponent("foo/bar", "1.0", source=URLSource("http://dummy.com"))
    assert str(c) == "foo/bar@1.0=http://dummy.com"


def test_idf_component_sanitized_name():
    c = IDFComponent("foo/bar bar-bar", "1.0", source=URLSource("http://dummy.com"))
    assert c.get_sanitized_name() == "foo/bar_bar-bar"


def test_idf_component_require_name():
    c = IDFComponent("foo/bar", "1.0", source=URLSource("http://dummy.com"))
    assert c.get_require_name() == "foo__bar"


def test_collect_filtered_files_basic(tmp_path):
    f1 = tmp_path / "a.c"
    f2 = tmp_path / "b" / "b.cpp"
    f1.write_text("int a;")
    f2.parent.mkdir(parents=True)
    f2.write_text("int b;")

    result = collect_filtered_files(tmp_path, ["+<*>"])
    assert str(f1) in result
    assert str(f2) in result


def test_collect_filtered_files_exclude(tmp_path):
    f1 = tmp_path / "a.c"
    f2 = tmp_path / "b.cpp"
    f1.write_text("int a;")
    f2.write_text("int b;")

    result = collect_filtered_files(tmp_path, ["+<*> -<*.cpp>"])
    assert str(f1) in result
    assert str(f2) not in result


def test_collect_filtered_files_exclude_pattern_in_subdir(tmp_path):
    src = tmp_path / "lib" / "src"
    src.mkdir(parents=True)
    kept = src / "a.c"
    excluded = src / "hasty.c"
    kept.write_text("int a;")
    excluded.write_text("int b;")

    result = collect_filtered_files(tmp_path, ["+<lib/src/*.c>", "-<lib/src/hasty.c>"])
    assert str(kept) in result
    assert str(excluded) not in result


def test_collect_filtered_files_exclude_unnormalized_glob_output(tmp_path, monkeypatch):
    # On Windows, glob keeps the pattern's literal separators for non-wildcard
    # path components, so the "+" wildcard pattern and the "-" literal pattern
    # yield the same file spelled differently and the exclude set difference
    # misses it. Backslash is a regular filename character on POSIX (such paths
    # fail the final is_file filter), so reproduce the unnormalized-output
    # mismatch portably with dot segments, which normpath also collapses.
    src = tmp_path / "lib" / "src"
    src.mkdir(parents=True)
    kept = src / "a.c"
    excluded = src / "hasty.c"
    kept.write_text("int a;")
    excluded.write_text("int b;")

    real_glob = glob.glob

    def unnormalized_glob(pattern, recursive=False):
        if "*" in pattern:
            base = str(tmp_path)
            return [base + "/lib/./src/a.c", base + "/lib/./src/hasty.c"]
        return real_glob(pattern, recursive=recursive)

    monkeypatch.setattr(glob, "glob", unnormalized_glob)

    result = collect_filtered_files(tmp_path, ["+<lib/src/*.c>", "-<lib/src/hasty.c>"])
    assert [Path(r).name for r in result] == ["a.c"]
    assert str(kept) in result


def test_split_list_by_condition():
    items = ["-Iinclude", "-Llib", "-Wall"]

    matched, rest = split_list_by_condition(
        items, lambda x: x[2:] if x.startswith("-I") else None
    )

    assert matched == ["include"]
    assert "-Llib" in rest
    assert "-Wall" in rest


def test_generate_cmakelists_txt_basic(tmp_component):
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    f = src_dir / "main.c"
    f.write_text("int main() {}")

    tmp_component.data = {}

    content = generate_cmakelists_txt(tmp_component)

    assert "idf_component_register" in content
    assert "main.c" in content


def test_generate_cmakelists_txt_external_source_uses_absolute_paths(
    tmp_component, tmp_path
):
    # A local library's sources live outside the component dir (source_path),
    # so SRCS and INCLUDE_DIRS must be emitted as absolute paths into it.
    source = tmp_path / "user_lib"
    (source / "src").mkdir(parents=True)
    (source / "include").mkdir()
    (source / "src" / "thing.cpp").write_text("int t;")
    tmp_component.source_path = source
    tmp_component.data = {}

    content = generate_cmakelists_txt(tmp_component)

    abs_src = str((source / "src" / "thing.cpp").resolve()).replace("\\", "/")
    abs_inc = str((source / "include").resolve()).replace("\\", "/")
    assert abs_src in content
    assert abs_inc in content
    # Nothing was copied into the component dir.
    assert not (tmp_component.path / "src").exists()


def test_generate_cmakelists_txt_external_source_absolutises_link_dirs(
    tmp_component, tmp_path
):
    # A local library's relative -L path must be made absolute against its own
    # directory so it resolves from the component cache dir.
    source = tmp_path / "user_lib"
    (source / "src").mkdir(parents=True)
    (source / "src" / "thing.cpp").write_text("int t;")
    (source / "libs").mkdir()
    tmp_component.source_path = source
    tmp_component.data = {"build": {"flags": ["-Llibs"]}}

    content = generate_cmakelists_txt(tmp_component)

    abs_lib = str((source / "libs").resolve()).replace("\\", "/")
    assert "target_link_directories" in content
    assert abs_lib in content


def test_generate_cmakelists_txt_external_source_root_srcdir(tmp_component, tmp_path):
    # An external source with files at its root (no src/ or include/ dir):
    # the src-dir search falls through to "." and the missing include dirs are
    # filtered out.
    source = tmp_path / "flat_lib"
    source.mkdir()
    (source / "thing.cpp").write_text("int t;")
    tmp_component.source_path = source
    tmp_component.data = {}

    content = generate_cmakelists_txt(tmp_component)

    assert str((source / "thing.cpp").resolve()).replace("\\", "/") in content


def test_generate_cmakelists_txt_with_flags(tmp_component, tmp_path):
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    (src_dir / "main.c").write_text("int main() {}")

    dep = IDFComponent("dep", "1.0", source=URLSource("http://dummy.com"))
    dep.path = tmp_path / "dep"
    tmp_component.dependencies = [dep]

    tmp_component.data = {
        "build": {"flags": ["-Iinclude", "-Llib", "-lmylib", "-Wall", "-DTEST"]}
    }

    content = generate_cmakelists_txt(tmp_component)
    # Paths are always emitted with forward slashes so the CMakeLists is
    # portable; on Windows os.path.relpath would otherwise yield backslashes
    # that break CMake's list re-parsing.
    assert (
        content
        == """idf_component_register(
  SRCS "src/main.c"
  INCLUDE_DIRS "src"
  REQUIRES dep ${ESPHOME_PROJECT_MANAGED_COMPONENTS} ${ESPHOME_PROJECT_BUILTIN_COMPONENTS}
)
target_compile_options(${COMPONENT_LIB} PUBLIC
  "-DTEST"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
  "-Wall"
)
target_link_directories(${COMPONENT_LIB} INTERFACE
  "lib"
)
target_link_libraries(${COMPONENT_LIB} INTERFACE
  "mylib"
)
"""
    )


def test_generate_cmakelists_txt_uses_forward_slashes_on_windows(
    tmp_component, monkeypatch: pytest.MonkeyPatch
) -> None:
    # os.path.relpath yields backslash paths on Windows, which CMake rejects
    # when it re-parses the SRCS list (e.g. "\b" in "src\backend" is an invalid
    # character escape). Simulate that output and confirm the generated
    # CMakeLists normalizes the separators to forward slashes.
    src_dir = tmp_component.path / "src" / "backend"
    src_dir.mkdir(parents=True)
    (src_dir / "cipher.c").write_text("int f() {}")

    tmp_component.data = {}

    monkeypatch.setattr("esphome.espidf.component.os.sep", "\\")
    monkeypatch.setattr(
        "esphome.espidf.component.os.path.relpath",
        lambda *args, **kwargs: "src\\backend\\cipher.c",
    )

    content = generate_cmakelists_txt(tmp_component)

    assert 'SRCS "src/backend/cipher.c"' in content
    assert "\\" not in content


def test_generate_cmakelists_txt_multi_token_flag(tmp_component):
    # PlatformIO shell-lexes each build.flags entry, so a single entry can
    # carry a flag and its argument. The generated CMakeLists must emit them
    # as separate compile options, not one argument with an embedded space.
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    (src_dir / "main.c").write_text("int main() {}")

    tmp_component.data = {"build": {"flags": ["-include cp_custom_alloc.h", "-DTEST"]}}

    content = generate_cmakelists_txt(tmp_component)
    assert '"-include cp_custom_alloc.h"' not in content
    assert '  "-include"\n  "cp_custom_alloc.h"\n' in content


def test_generate_cmakelists_txt_escapes_embedded_quotes(tmp_component):
    """A define value carrying a literal quote survives into CMake as an
    escaped quote, not a prematurely-terminated string."""
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    (src_dir / "main.c").write_text("int main() {}")
    # shlex keeps the backslash-escaped quotes as literal characters
    tmp_component.data = {"build": {"flags": ['-DMSG=\\"hi\\"']}}

    content = generate_cmakelists_txt(tmp_component)
    assert '"-DMSG=\\"hi\\""' in content


def test_generate_cmakelists_txt_extra_script_link_flags(tmp_component):
    """Captured extra-script LINKFLAGS come out as target_link_options, not
    compile options where they would be silently ineffective."""
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    (src_dir / "main.c").write_text("int main() {}")

    tmp_component.data = {
        ESPHOME_DATA_KEY: {ESPHOME_DATA_LINK_FLAGS_KEY: ["-Wl,--gc-sections"]}
    }

    content = generate_cmakelists_txt(tmp_component)
    assert (
        'target_link_options(${COMPONENT_LIB} INTERFACE\n  "-Wl,--gc-sections"\n)'
        in content
    )
    assert "target_compile_options" not in content


def test_generate_cmakelists_txt_space_separated_classified_flags(tmp_component):
    # Space-separated -I/-L/-l entries routed to INCLUDE_DIRS and the link
    # handling before the shlex split was added; splitting must not leak
    # them into raw compile options.
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    (src_dir / "main.c").write_text("int main() {}")
    (tmp_component.path / "extra_inc").mkdir()

    tmp_component.data = {
        "build": {"flags": ["-I extra_inc", "-L extra_lib", "-l extralib", "-DTEST"]}
    }

    content = generate_cmakelists_txt(tmp_component)
    assert 'INCLUDE_DIRS "src" "extra_inc"' in content
    assert 'target_link_directories(${COMPONENT_LIB} INTERFACE\n  "extra_lib"\n)' in (
        content
    )
    assert 'target_link_libraries(${COMPONENT_LIB} INTERFACE\n  "extralib"\n)' in (
        content
    )
    assert '"-I"' not in content
    assert '"-L"' not in content
    assert '"-l"' not in content


def test_generate_cmakelists_txt_references_project_managed_components_variable(
    tmp_component: IDFComponent,
) -> None:
    # The CMakeLists is cached under pio_components/<hash>/ and shared
    # across projects, so the project-managed REQUIRES list is exposed via
    # a CMake variable expanded at configure time rather than baked here.
    src_dir = tmp_component.path / "src"
    src_dir.mkdir()
    (src_dir / "main.c").write_text("int main() {}")
    tmp_component.data = {}

    content = generate_cmakelists_txt(tmp_component)
    assert "${ESPHOME_PROJECT_MANAGED_COMPONENTS}" in content


def test_generate_idf_component_yml_basic(tmp_component):
    tmp_component.data = {"description": "test", "repository": {"url": "http://aaa"}}
    result = generate_idf_component_yml(tmp_component)

    assert result == "description: test\nrepository: http://aaa\n"


def test_generate_idf_component_yml_tolerates_malformed_metadata(tmp_component):
    """A string repository is the URL itself; junk shapes drop instead of
    crashing on a third-party manifest."""
    tmp_component.data = {"description": "test", "repository": "http://aaa"}
    assert (
        generate_idf_component_yml(tmp_component)
        == "description: test\nrepository: http://aaa\n"
    )
    tmp_component.data = {"description": {"en": "x"}, "repository": 123}
    assert generate_idf_component_yml(tmp_component) == "{}\n"


def test_generate_idf_component_yml_with_dependencies(tmp_component, tmp_path):
    dep = IDFComponent("dep", "1.0", source=URLSource("http://dummy.com"))
    dep.path = tmp_path / "dep"

    tmp_component.dependencies = [dep]
    tmp_component.data = {}

    result = generate_idf_component_yml(tmp_component)

    assert (
        result
        == f"""dependencies:
  dep:
    override_path: {dep.path}
"""
    )


def test_generate_idf_component_yml_missing_path_raises(tmp_component):
    # A dep without a path is a contract violation — every dep is expected
    # to have been downloaded before YAML generation. Raise loudly.
    dep = IDFComponent("foo/bar", "1.0", source=None)

    tmp_component.dependencies = [dep]
    tmp_component.data = {}

    with pytest.raises(RuntimeError):
        generate_idf_component_yml(tmp_component)


def test_parse_library_json(tmp_path):
    f = tmp_path / "library.json"
    f.write_text(json.dumps({"name": "test"}))

    result = parse_library_json(f)
    assert result["name"] == "test"


def test_parse_library_properties(tmp_path):
    f = tmp_path / "library.properties"
    f.write_text(
        """
name=Test
version=1.0
# description=ABCD
empty=
"""
    )

    result = parse_library_properties(f)

    assert result["name"] == "Test"
    assert result["version"] == "1.0"
    assert "empty" not in result


def test_node_key_git_with_ref():
    key, kind, locator = _node_key(
        "name", None, "https://github.com/foo/bar.git#v1.2.3"
    )
    assert key == "foo/bar"
    assert kind == "git"
    assert locator == ("https://github.com/foo/bar.git", "v1.2.3")


def test_node_key_git_branch_ref():
    key, kind, locator = _node_key(
        "name", None, "https://github.com/foo/bar.git#some-branch"
    )
    assert (key, kind, locator[1]) == ("foo/bar", "git", "some-branch")


def test_node_key_git_no_ref():
    _key, kind, locator = _node_key("name", None, "https://github.com/foo/bar.git")
    assert kind == "git"
    assert locator == ("https://github.com/foo/bar.git", None)


def test_node_key_url_in_name_is_git():
    # add_library("https://github.com/x/y", None): PlatformIO accepted a bare
    # git URL as the library name, so the converter must too.
    key, kind, locator = _node_key("https://github.com/pstolarz/OneWireNg", None, None)
    assert key == "pstolarz/OneWireNg"
    assert kind == "git"
    assert locator == ("https://github.com/pstolarz/OneWireNg", None)


def test_node_key_url_in_name_with_ref():
    key, kind, locator = _node_key("https://github.com/foo/bar.git#v1.2.3", None, None)
    assert (key, kind, locator) == (
        "foo/bar",
        "git",
        ("https://github.com/foo/bar.git", "v1.2.3"),
    )


def test_node_key_url_in_name_git_plus_prefix():
    key, kind, locator = _node_key("git+https://github.com/foo/bar", None, None)
    assert (key, kind, locator) == (
        "foo/bar",
        "git",
        ("https://github.com/foo/bar", None),
    )


def test_node_key_git_plus_prefix_in_repository():
    _key, kind, locator = _node_key("name", None, "git+https://github.com/foo/bar")
    assert (kind, locator) == ("git", ("https://github.com/foo/bar", None))


def test_node_key_custom_name_equals_url_is_git():
    key, kind, locator = _node_key(
        "OneWireNg=https://github.com/pstolarz/OneWireNg", None, None
    )
    assert (key, kind, locator) == (
        "pstolarz/OneWireNg",
        "git",
        ("https://github.com/pstolarz/OneWireNg", None),
    )


def test_node_key_url_in_name_with_query_containing_equals():
    # A bare URL whose query string contains ``=`` must not be split by the
    # CustomName=URL handling.
    key, kind, locator = _node_key("https://host/x/y.git?ref=main", None, None)
    assert (key, kind, locator) == (
        "x/y",
        "git",
        ("https://host/x/y.git?ref=main", None),
    )


def test_node_key_file_url_in_repository_is_local():
    # A plain file:// entry (PlatformIO's spelling for a local library folder)
    # resolves as a local directory, keeping the custom name as the key. The
    # path is the OS-native form of the URL (backslashes on Windows).
    key, kind, (path, ref) = _node_key(
        "TeslaBLE", None, "file:///config/esphome/lib_dev"
    )
    assert (key, kind, ref) == ("TeslaBLE", "local", None)
    assert Path(path) == Path("/config/esphome/lib_dev")


def test_node_key_bare_file_url_is_local_named_for_dir():
    # Without a custom name the directory's own name becomes the key.
    key, kind, (path, ref) = _node_key(None, None, "file:///opt/mylib")
    assert (key, kind, ref) == ("mylib", "local", None)
    assert Path(path) == Path("/opt/mylib")


def test_node_key_custom_name_equals_file_url_is_local():
    key, kind, (path, ref) = _node_key("Foo=file:///opt/mylib", None, None)
    assert (key, kind, ref) == ("Foo", "local", None)
    assert Path(path) == Path("/opt/mylib")


def test_node_key_file_url_localhost_host_is_local():
    # A localhost host is ignored; only the path identifies the directory.
    key, kind, (path, ref) = _node_key(None, None, "file://localhost/opt/mylib")
    assert (key, kind, ref) == ("mylib", "local", None)
    assert Path(path) == Path("/opt/mylib")


@pytest.mark.parametrize(
    "url", ["file://server/share/lib", "file://lib_dev", "file://../mylib"]
)
def test_node_key_file_url_with_host_rejected(url: str) -> None:
    # A real host, or a relative path whose first segment parses as the host,
    # is rejected rather than silently resolved to the wrong directory.
    with pytest.raises(RuntimeError, match="Unsupported host in file://"):
        _node_key(None, None, url)


@pytest.mark.parametrize("url", ["file:lib_dev", "file:./lib", "file:///"])
def test_node_key_file_url_must_be_absolute(url: str) -> None:
    # A relative path (no host, e.g. file:lib_dev) or a bare root (file:///)
    # is rejected rather than resolved against the cwd or yielding an empty name.
    with pytest.raises(RuntimeError, match="must be an absolute"):
        _node_key(None, None, url)


def test_node_key_git_plus_file_url_stays_git():
    # git+file:// is an explicit local git repo, not a plain directory.
    _key, kind, locator = _node_key("X", None, "git+file:///srv/foo.git")
    assert kind == "git"
    assert locator == ("file:///srv/foo.git", None)


@pytest.mark.parametrize("name", ["http://[::1", "CustomName=http://[::1"])
def test_node_key_malformed_url_in_name_raises(name: str) -> None:
    # A name that was clearly meant to be a URL but does not parse must fail
    # fast instead of degrading to a confusing registry lookup error.
    with pytest.raises(RuntimeError, match="Invalid PIO library URL"):
        _node_key(name, None, None)


def test_node_key_name_with_equals_but_no_url_is_registry():
    key, kind, locator = _node_key("FOO=BAR", "1.0", None)
    assert (key, kind, locator) == ("FOO=BAR", "registry", (None, "FOO=BAR"))


def test_node_key_version_url_still_ignored_when_name_plain():
    # A version that is a URL is handled by the dependency walk, not here;
    # a plain name must stay a registry spec regardless of version shape.
    key, kind, _locator = _node_key("bar", "https://github.com/foo/bar", None)
    assert (key, kind) == ("bar", "registry")


def test_node_key_registry_owner_name():
    key, kind, locator = _node_key("foo/bar", "^1.0.0", None)
    assert (key, kind, locator) == ("foo/bar", "registry", ("foo", "bar"))


def test_node_key_registry_bare_name():
    key, kind, locator = _node_key("bar", "1.0", None)
    assert (key, kind, locator) == ("bar", "registry", (None, "bar"))


def test_normalize_dependencies_none():
    assert normalize_dependencies(None) == []


def test_normalize_dependencies_list_form():
    deps = [{"name": "foo", "version": "1.0"}]
    assert normalize_dependencies(deps) == [{"name": "foo", "version": "1.0"}]


def test_normalize_dependencies_dict_form():
    out = normalize_dependencies({"nanopb/Nanopb": "^0.4.91", "BareName": "1.2.3"})
    assert {"name": "Nanopb", "owner": "nanopb", "version": "^0.4.91"} in out
    assert {"name": "BareName", "owner": None, "version": "1.2.3"} in out


def test_normalize_dependencies_dict_form_nested_spec():
    out = normalize_dependencies(
        {"nanopb/Nanopb": {"version": "^0.4.91", "platforms": "espidf"}}
    )
    assert out == [
        {
            "name": "Nanopb",
            "owner": "nanopb",
            "version": "^0.4.91",
            "platforms": "espidf",
        }
    ]


def _patch_registry(monkeypatch, versions):
    """Patch the registry client to serve a canned version list (no network).

    Only ``fetch_registry_package`` is faked; the real
    ``get_compatible_registry_versions`` / ``pick_best_registry_version`` run on
    the canned data so the intersection logic is exercised for real.
    """
    registry = esphome.platformio.library._make_registry_client()
    monkeypatch.setattr(
        registry,
        "fetch_registry_package",
        lambda spec: {
            "owner": {"username": spec.owner or "owner"},
            "name": spec.name,
            "versions": [
                {"name": v, "files": [{"download_url": f"http://x/{v}.tar.gz"}]}
                for v in versions
            ],
        },
    )
    monkeypatch.setattr(
        esphome.platformio.library, "_make_registry_client", lambda: registry
    )


def test_resolve_registry_version_intersects_constraints(monkeypatch):
    _patch_registry(monkeypatch, ["1.10018.1", "1.10021.0", "1.10021.1"])
    owner, name, version, url, _size = _resolve_registry_version(
        "esphome", "libsodium", {"==1.10021.0", "^1.10018.1"}
    )
    assert (owner, name, version) == ("esphome", "libsodium", "1.10021.0")
    assert url == "http://x/1.10021.0.tar.gz"


def test_resolve_registry_version_picks_highest_satisfying(monkeypatch):
    _patch_registry(monkeypatch, ["1.0.0", "1.5.0", "2.0.0"])
    _owner, _name, version, _url, _size = _resolve_registry_version(
        "o", "p", {"^1.0.0"}
    )
    assert version == "1.5.0"


def test_resolve_registry_version_conflict_raises(monkeypatch):
    _patch_registry(monkeypatch, ["1.0.0", "2.0.0"])
    with pytest.raises(RuntimeError, match="satisfies all requirements"):
        _resolve_registry_version("o", "p", {"==1.0.0", "==2.0.0"})


def test_generate_idf_components_dedupes_shared_dependency(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
) -> None:
    # A and B both depend on shared C under different version specs. The batch
    # must resolve C once with BOTH requirements collected, wire a single C
    # instance into both, and regenerate (overwrite) each library's build files.
    manifests = {
        "esphome/A": {
            "name": "A",
            "dependencies": [
                {"owner": "esphome", "name": "C", "version": "==1.10021.0"}
            ],
        },
        "esphome/B": {
            "name": "B",
            "dependencies": [
                {"owner": "esphome", "name": "C", "version": "^1.10018.1"}
            ],
        },
        "esphome/C": {"name": "C"},
    }

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "src" / "x.c").write_text("int x;")
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))
        (self.path / "CMakeLists.txt").write_text("# TRIPWIRE\n")

    monkeypatch.setattr(IDFComponent, "download", fake_download)

    captured: dict[str, set[str]] = {}
    resolve_calls: list[str] = []

    def fake_resolve(owner, pkgname, requirements):
        resolve_calls.append(pkgname)
        captured[f"{owner}/{pkgname}"] = set(requirements)
        version = "1.10021.0" if pkgname == "C" else "1.0.0"
        return owner, pkgname, version, f"http://x/{pkgname}.tar.gz", None

    monkeypatch.setattr(
        esphome.platformio.library, "_resolve_registry_version", fake_resolve
    )

    top = generate_idf_components(
        [Library("esphome/A", "1.0.0", None), Library("esphome/B", "1.0.0", None)]
    )

    # C resolved once (not once per consumer) with BOTH requirements gathered.
    assert captured["esphome/C"] == {"==1.10021.0", "^1.10018.1"}
    assert resolve_calls.count("C") == 1
    # Top-level components returned in request order.
    assert [c.name for c in top] == ["esphome/A", "esphome/B"]
    # A and B reference the SAME single C instance (deduped).
    a_dep = top[0].dependencies[0]
    b_dep = top[1].dependencies[0]
    assert a_dep.name == "esphome/C"
    assert a_dep is b_dep
    # The bundled CMakeLists was overwritten with generated content.
    generated = (a_dep.path / "CMakeLists.txt").read_text()
    assert "TRIPWIRE" not in generated
    assert "idf_component_register" in generated


def test_generate_idf_components_lib_ignore_filters_top_level_and_dependencies(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
) -> None:
    # lib_ignore must drop B at the top level and C when it is discovered as a
    # dependency of A during the graph walk -- neither may be resolved,
    # downloaded, or wired into a manifest. Matching is by lowercase short name.
    manifests = {
        "esphome/A": {
            "name": "A",
            "dependencies": [
                {"owner": "esphome", "name": "C", "version": "==1.10021.0"}
            ],
        },
        "esphome/B": {"name": "B"},
    }

    download_salts: list[str] = []

    def fake_download(self, force=False, salt="", namespace=""):
        download_salts.append(salt)
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "src" / "x.c").write_text("int x;")
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(IDFComponent, "download", fake_download)

    resolve_calls: list[str] = []

    def fake_resolve(owner, pkgname, requirements):
        resolve_calls.append(pkgname)
        return owner, pkgname, "1.0.0", f"http://x/{pkgname}.tar.gz", None

    monkeypatch.setattr(
        esphome.platformio.library, "_resolve_registry_version", fake_resolve
    )
    # lib_ignore is read from CORE.platformio_options (stored there by
    # _add_platformio_options); matched by lowercase short name.
    monkeypatch.setattr(CORE, "platformio_options", {"lib_ignore": ["B", "esphome/C"]})

    top = generate_idf_components(
        [Library("esphome/A", "1.0.0", None), Library("esphome/B", "1.0.0", None)]
    )

    assert [c.name for c in top] == ["esphome/A"]
    # Ignored libraries were never resolved (and therefore never downloaded).
    assert resolve_calls == ["A"]
    # The ignored dependency is not wired into A's manifest.
    assert top[0].dependencies == []
    # lib_ignore changes the generated wiring, so the cache path is salted to
    # keep this conversion separate from ones with a different lib_ignore.
    assert download_salts == [hashlib.sha256(b"b,c").hexdigest()[:8]]


def test_generate_idf_components_handles_dependency_cycle(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
) -> None:
    # A -> B -> A. Must terminate (not recurse forever) and wire the cycle with
    # a single instance per component.
    manifests = {
        "esphome/A": {
            "name": "A",
            "dependencies": [{"owner": "esphome", "name": "B", "version": "1.0.0"}],
        },
        "esphome/B": {
            "name": "B",
            "dependencies": [{"owner": "esphome", "name": "A", "version": "1.0.0"}],
        },
    }

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "src" / "x.c").write_text("int x;")
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(IDFComponent, "download", fake_download)
    monkeypatch.setattr(
        esphome.platformio.library,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner,
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
            None,
        ),
    )

    top = generate_idf_components([Library("esphome/A", "1.0.0", None)])

    assert [c.name for c in top] == ["esphome/A"]
    component_a = top[0]
    component_b = component_a.dependencies[0]
    assert component_b.name == "esphome/B"
    # The cycle is wired back to the same A instance, not a duplicate.
    assert component_b.dependencies[0] is component_a


def test_generate_idf_components_git_overrides_registry_warns(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    # A pulls shared as a registry pin; B pulls the same component from a git
    # source. The git source wins, but the dropped registry pin must be warned
    # about (not silently discarded).
    manifests = {
        "esphome/A": {
            "name": "A",
            "dependencies": [
                {"owner": "esphome", "name": "shared", "version": "==1.0.0"}
            ],
        },
        "esphome/B": {
            "name": "B",
            "dependencies": [
                {
                    "owner": "esphome",
                    "name": "shared",
                    "version": "https://github.com/esphome/shared.git#main",
                }
            ],
        },
        "esphome/shared": {"name": "shared"},
    }

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "src" / "x.c").write_text("int x;")
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(IDFComponent, "download", fake_download)
    monkeypatch.setattr(
        esphome.platformio.library,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner,
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
            None,
        ),
    )

    top = generate_idf_components(
        [Library("esphome/A", "1.0.0", None), Library("esphome/B", "1.0.0", None)]
    )

    # shared resolved from the git source (version "*"), not the registry pin.
    shared = top[0].dependencies[0]
    assert shared.name == "esphome/shared"
    assert isinstance(shared.source, GitSource)
    assert "using the git source" in caplog.text
    assert "==1.0.0" in caplog.text


def test_generate_idf_components_missing_manifest_raises(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
) -> None:
    # A library with neither library.json nor library.properties is invalid;
    # fail loudly rather than silently generating build files for it.
    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        # no library.json / library.properties written

    monkeypatch.setattr(IDFComponent, "download", fake_download)
    monkeypatch.setattr(
        esphome.platformio.library,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner,
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
            None,
        ),
    )

    with pytest.raises(RuntimeError, match="missing library.json"):
        generate_idf_components([Library("esphome/A", "1.0.0", None)])


def test_generate_idf_components_warns_on_noncanonical_duplicate(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    # A references "shared" (bare) and B references "owner/shared"; both resolve
    # to the same canonical name but as distinct graph nodes, so they aren't
    # deduplicated -- warn about it.
    manifests = {
        "esphome/A": {
            "name": "A",
            "dependencies": [{"name": "shared", "version": "1.0.0"}],
        },
        "esphome/B": {
            "name": "B",
            "dependencies": [{"owner": "owner", "name": "shared", "version": "1.0.0"}],
        },
        "owner/shared": {"name": "shared"},
    }

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "src" / "x.c").write_text("int x;")
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(IDFComponent, "download", fake_download)
    # Bare "shared" and "owner/shared" both resolve to canonical owner/shared.
    monkeypatch.setattr(
        esphome.platformio.library,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner or "owner",
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
            None,
        ),
    )

    generate_idf_components(
        [Library("esphome/A", "1.0.0", None), Library("esphome/B", "1.0.0", None)]
    )

    assert "referenced under multiple names" in caplog.text


def test_generate_idf_components_incompatible_top_level_raises(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
) -> None:
    # A top-level library that isn't ESP-IDF/esp32 compatible must fail fast,
    # not be silently dropped.
    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "library.json").write_text(
            json.dumps({"name": "A", "platforms": ["espressif8266"]})
        )

    monkeypatch.setattr(IDFComponent, "download", fake_download)
    monkeypatch.setattr(
        esphome.platformio.library,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner,
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
            None,
        ),
    )

    with pytest.raises(RuntimeError, match="not compatible with espidf"):
        generate_idf_components([Library("esphome/A", "1.0.0", None)])


def test_generate_idf_components_incompatible_dependency_skipped(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    esp32_idf_core: None,
) -> None:
    # An incompatible *transitive* dependency is skipped (not fatal): A is fine,
    # its esp8266-only dep B is dropped and not wired.
    manifests = {
        "esphome/A": {
            "name": "A",
            "dependencies": [{"owner": "esphome", "name": "B", "version": "1.0.0"}],
        },
        "esphome/B": {"name": "B", "platforms": ["espressif8266"]},
    }

    def fake_download(self, force=False, salt="", namespace=""):
        self.path = tmp_path / self.get_sanitized_name().replace("/", "__")
        (self.path / "src").mkdir(parents=True, exist_ok=True)
        (self.path / "library.json").write_text(json.dumps(manifests[self.name]))

    monkeypatch.setattr(IDFComponent, "download", fake_download)
    monkeypatch.setattr(
        esphome.platformio.library,
        "_resolve_registry_version",
        lambda owner, pkgname, requirements: (
            owner,
            pkgname,
            "1.0.0",
            f"http://x/{pkgname}.tar.gz",
            None,
        ),
    )

    top = generate_idf_components([Library("esphome/A", "1.0.0", None)])

    assert [c.name for c in top] == ["esphome/A"]
    # The incompatible dependency was dropped, not wired in.
    assert top[0].dependencies == []


def test_url_source_salt_changes_cache_path(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The salt is mixed into the URL hash so salted conversions get their own
    cache tree. Pre-created extraction markers keep this network-free."""
    monkeypatch.setattr(CORE, "config_path", tmp_path / "test.yaml")
    url = "http://example.com/lib.tar.gz"
    base = tmp_path / ".esphome" / "pio_components"
    expected = {}
    for salt in ("", "abcd1234"):
        digest = hashlib.sha256((url + salt).encode()).hexdigest()[:8]
        expected[salt] = base / digest / "lib"
        expected[salt].mkdir(parents=True)
        (expected[salt] / ".esphome_extracted").touch()

    source = URLSource(url)
    assert source.download("lib") == expected[""]
    assert source.download("lib", salt="abcd1234") == expected["abcd1234"]

    # A backend namespace adds a pio_components/<namespace>/ subdir.
    digest = hashlib.sha256(url.encode()).hexdigest()[:8]
    ns_expected = base / "idf" / digest / "lib"
    ns_expected.mkdir(parents=True)
    (ns_expected / ".esphome_extracted").touch()
    assert source.download("lib", namespace="idf") == ns_expected


def test_git_source_salt_scopes_domain(monkeypatch: pytest.MonkeyPatch) -> None:
    """The salt becomes a subdirectory of the git clone domain."""
    domains: list[str] = []

    def fake_clone_or_update(**kwargs):
        domains.append(kwargs["domain"])
        return Path("/cloned"), None

    monkeypatch.setattr(
        esphome.platformio.library.git, "clone_or_update", fake_clone_or_update
    )

    source = GitSource("https://github.com/esphome/noise-c.git", "v1.0")
    source.download("noise-c")
    source.download("noise-c", salt="abcd1234")
    source.download("noise-c", namespace="idf")
    source.download("noise-c", namespace="zephyr", salt="abcd1234")
    assert domains == [
        "pio_components",
        "pio_components/abcd1234",
        "pio_components/idf",
        "pio_components/zephyr/abcd1234",
    ]


def test_idf_component_download_passes_salt() -> None:
    """IDFComponent.download forwards the sanitized name and salt to the
    source and records the returned path."""
    source = MagicMock()
    source.download.return_value = Path("/converted/owner/name")

    c = IDFComponent("owner/name", "1.0", source=source)
    c.download(force=True, salt="abcd1234", namespace="idf")

    source.download.assert_called_once_with(
        "owner/name", force=True, salt="abcd1234", namespace="idf"
    )
    assert c.path == Path("/converted/owner/name")


def test_emit_idf_component_wires_esp32_target(tmp_path, monkeypatch):
    """Emitting a component resolves the esp32 variant into the shared
    extraScript helper."""

    monkeypatch.setattr(esp32_module, "get_esp32_variant", lambda: "ESP32")
    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=[env.get('BOARD_MCU')])\n")
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py"}}
    _emit_idf_component(c)
    assert c.data["build"]["flags"] == ["-lesp32"]


def test_build_flags_dangling_flag_does_not_cross_entries(
    tmp_path, caplog: pytest.LogCaptureFixture
) -> None:
    """Each entry is lexed independently, as ParseFlags does: a dangling -I ending one
    entry warns instead of absorbing the next entry's first token."""
    (tmp_path / "src").mkdir()
    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"flags": ["-Wall -I", "-DFOO=1"]}}
    content = generate_cmakelists_txt(c)
    assert "FOO=1" in content
    assert "-I-DFOO" not in content
    assert "Ignoring trailing '-I'" in caplog.text
