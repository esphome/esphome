import glob
import hashlib
import json
import os
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    Framework,
    Platform,
)
from esphome.core import CORE, Library
from esphome.espidf.component import (
    generate_cmakelists_txt,
    generate_idf_component_yml,
    generate_idf_components,
)
import esphome.platformio.library
from esphome.platformio.library import (
    ConvertedLibrary as IDFComponent,
    GitSource,
    URLSource,
    _node_key,
    _normalize_dependencies,
    _parse_library_json,
    _parse_library_properties,
    _resolve_registry_version,
    collect_filtered_files,
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
    sep = "\\\\" if os.name == "nt" else "/"
    assert (
        content
        == f"""idf_component_register(
  SRCS "src{sep}main.c"
  INCLUDE_DIRS "src"
  REQUIRES dep ${{ESPHOME_PROJECT_MANAGED_COMPONENTS}} ${{ESPHOME_PROJECT_BUILTIN_COMPONENTS}}
)
target_compile_options(${{COMPONENT_LIB}} PUBLIC
  "-DTEST"
)
target_compile_options(${{COMPONENT_LIB}} PRIVATE
  "-Wall"
)
target_link_directories(${{COMPONENT_LIB}} INTERFACE
  "lib"
)
target_link_libraries(${{COMPONENT_LIB}} INTERFACE
  "mylib"
)
"""
    )


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


def test_extra_script_captures_libpath_libs_and_defines(tmp_path):
    from esphome.espidf.extra_script import captured_as_build_flags, run_extra_script

    (tmp_path / "src" / "esp32").mkdir(parents=True)
    script = tmp_path / "extra_script.py"
    script.write_text(
        "Import('env')\n"
        "mcu = env.get('BOARD_MCU')\n"
        "env.Append(\n"
        "    LIBPATH=[join('src', mcu)],\n"
        "    LIBS=['algobsec'],\n"
        "    CPPDEFINES=['FOO', ('BAR', '1')],\n"
        "    LINKFLAGS=['-Wl,--gc-sections'],\n"
        ")\n"
    )
    # The script uses bare ``join`` (PIO's extra-scripts run inside SCons
    # where this is in scope). Inject it via the script header so the
    # shim's exec namespace can resolve it.
    script.write_text("from os.path import join\n" + script.read_text())

    result = run_extra_script(script, library_dir=tmp_path, idf_target="esp32")

    assert result.libpath == [str(Path("src") / "esp32")]
    assert result.libs == ["algobsec"]
    assert ("BAR", "1") in result.cppdefines
    assert "FOO" in result.cppdefines
    assert result.linkflags == ["-Wl,--gc-sections"]

    flags = captured_as_build_flags(result, library_dir=tmp_path)
    sep = os.sep
    assert f"-Lsrc{sep}esp32" in flags
    assert "-lalgobsec" in flags
    assert "-DFOO" in flags
    assert "-DBAR=1" in flags
    assert "-Wl,--gc-sections" in flags


def test_extra_script_libpath_relative_resolves_against_library_dir(
    tmp_path, monkeypatch
):
    """Relative LIBPATH entries must resolve against ``library_dir``, not the
    caller's CWD (the shim restores CWD before ``captured_as_build_flags``
    runs)."""
    from esphome.espidf.extra_script import ExtraScriptResult, captured_as_build_flags

    (tmp_path / "lib" / "esp32").mkdir(parents=True)
    elsewhere = tmp_path.parent / "not_the_library_dir"
    elsewhere.mkdir(exist_ok=True)
    monkeypatch.chdir(elsewhere)

    result = ExtraScriptResult(libpath=["lib/esp32"])
    flags = captured_as_build_flags(result, library_dir=tmp_path)

    sep = os.sep
    assert flags == [f"-Llib{sep}esp32"]


def test_extra_script_libpath_absolute_outside_library_dir(tmp_path):
    from esphome.espidf.extra_script import ExtraScriptResult, captured_as_build_flags

    outside = tmp_path.parent / "system_lib"
    outside.mkdir(exist_ok=True)
    result = ExtraScriptResult(libpath=[str(outside)])

    flags = captured_as_build_flags(result, library_dir=tmp_path)
    assert flags == [f"-L{outside.resolve()}"]


def test_extra_script_failure_returns_empty_result(tmp_path, caplog):
    from esphome.espidf.extra_script import run_extra_script

    script = tmp_path / "broken.py"
    script.write_text("raise RuntimeError('boom')\n")

    with caplog.at_level("WARNING"):
        result = run_extra_script(script, library_dir=tmp_path, idf_target="esp32")

    assert result.libpath == []
    assert result.libs == []
    assert "broken.py" in caplog.text


def test_apply_extra_script_path_traversal_is_rejected(tmp_path):
    from esphome.espidf.component import _apply_extra_script

    library_dir = tmp_path / "lib"
    library_dir.mkdir()
    outside = tmp_path / "evil.py"
    outside.write_text("env.Append(LIBS=['pwned'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = library_dir
    c.data = {"build": {"extraScript": "../evil.py"}}

    _apply_extra_script(c)

    # Nothing was folded into flags: the traversal was rejected before
    # the script could run.
    assert "flags" not in c.data["build"]


def test_apply_extra_script_merges_into_existing_flags(tmp_path, monkeypatch):
    from esphome.components import esp32 as esp32_module

    monkeypatch.setattr(esp32_module, "get_esp32_variant", lambda: "ESP32")

    from esphome.espidf.component import _apply_extra_script

    (tmp_path / "src").mkdir()
    script = tmp_path / "extra.py"
    script.write_text("env.Append(LIBS=['algobsec'])\n")

    c = IDFComponent("owner/name", "1.0", source=URLSource("http://dummy"))
    c.path = tmp_path
    c.data = {"build": {"extraScript": "extra.py", "flags": ["-DEXISTING"]}}

    _apply_extra_script(c)

    assert "-DEXISTING" in c.data["build"]["flags"]
    assert "-lalgobsec" in c.data["build"]["flags"]


def test_parse_library_json(tmp_path):
    f = tmp_path / "library.json"
    f.write_text(json.dumps({"name": "test"}))

    result = _parse_library_json(f)
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

    result = _parse_library_properties(f)

    assert result["name"] == "Test"
    assert result["version"] == "1.0"
    assert "empty" not in result


def test_node_key_git_with_ref():
    key, is_git, locator = _node_key(
        "name", None, "https://github.com/foo/bar.git#v1.2.3"
    )
    assert key == "foo/bar"
    assert is_git is True
    assert locator == ("https://github.com/foo/bar.git", "v1.2.3")


def test_node_key_git_branch_ref():
    key, is_git, locator = _node_key(
        "name", None, "https://github.com/foo/bar.git#some-branch"
    )
    assert (key, is_git, locator[1]) == ("foo/bar", True, "some-branch")


def test_node_key_git_no_ref():
    _key, is_git, locator = _node_key("name", None, "https://github.com/foo/bar.git")
    assert is_git is True
    assert locator == ("https://github.com/foo/bar.git", None)


def test_node_key_registry_owner_name():
    key, is_git, locator = _node_key("foo/bar", "^1.0.0", None)
    assert (key, is_git, locator) == ("foo/bar", False, ("foo", "bar"))


def test_node_key_registry_bare_name():
    key, is_git, locator = _node_key("bar", "1.0", None)
    assert (key, is_git, locator) == ("bar", False, (None, "bar"))


def test_normalize_dependencies_none():
    assert _normalize_dependencies(None) == []


def test_normalize_dependencies_list_form():
    deps = [{"name": "foo", "version": "1.0"}]
    assert _normalize_dependencies(deps) == [{"name": "foo", "version": "1.0"}]


def test_normalize_dependencies_dict_form():
    out = _normalize_dependencies({"nanopb/Nanopb": "^0.4.91", "BareName": "1.2.3"})
    assert {"name": "Nanopb", "owner": "nanopb", "version": "^0.4.91"} in out
    assert {"name": "BareName", "owner": None, "version": "1.2.3"} in out


def test_normalize_dependencies_dict_form_nested_spec():
    out = _normalize_dependencies(
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
    owner, name, version, url = _resolve_registry_version(
        "esphome", "libsodium", {"==1.10021.0", "^1.10018.1"}
    )
    assert (owner, name, version) == ("esphome", "libsodium", "1.10021.0")
    assert url == "http://x/1.10021.0.tar.gz"


def test_resolve_registry_version_picks_highest_satisfying(monkeypatch):
    _patch_registry(monkeypatch, ["1.0.0", "1.5.0", "2.0.0"])
    _owner, _name, version, _url = _resolve_registry_version("o", "p", {"^1.0.0"})
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
        return owner, pkgname, version, f"http://x/{pkgname}.tar.gz"

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
        return owner, pkgname, "1.0.0", f"http://x/{pkgname}.tar.gz"

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
