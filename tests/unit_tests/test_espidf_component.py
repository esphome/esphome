import json
import os
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
import esphome.espidf.component
from esphome.espidf.component import (
    GitSource,
    IDFComponent,
    InvalidIDFComponent,
    URLSource,
    _check_library_data,
    _collect_filtered_files,
    _convert_library_to_component,
    _detect_requires,
    _parse_library_json,
    _parse_library_properties,
    _process_dependencies,
    _split_list_by_condition,
    generate_cmakelists_txt,
    generate_idf_component_yml,
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

    result = _collect_filtered_files(tmp_path, ["+<*>"])
    assert str(f1) in result
    assert str(f2) in result


def test_collect_filtered_files_exclude(tmp_path):
    f1 = tmp_path / "a.c"
    f2 = tmp_path / "b.cpp"
    f1.write_text("int a;")
    f2.write_text("int b;")

    result = _collect_filtered_files(tmp_path, ["+<*> -<*.cpp>"])
    assert str(f1) in result
    assert str(f2) not in result


def test_detect_requires(tmp_path):
    f = tmp_path / "main.c"
    f.write_text('#include "mbedtls/foo.h"')

    result = _detect_requires([str(f)])
    assert "mbedtls" in result


def test_detect_requires_ignores_invalid_file(tmp_path):
    result = _detect_requires([str(tmp_path / "missing.c")])
    assert result == set()


def test_split_list_by_condition():
    items = ["-Iinclude", "-Llib", "-Wall"]

    matched, rest = _split_list_by_condition(
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
  REQUIRES dep
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


def test_generate_idf_component_yml_basic(tmp_component):
    tmp_component.data = {"description": "test", "repository": {"url": "http://aaa"}}
    result = generate_idf_component_yml(tmp_component)

    assert result == "description: test\nversion: 1.0.0\nrepository: http://aaa\n"


def test_generate_idf_component_yml_with_dependencies(tmp_component, tmp_path):
    dep = IDFComponent("dep", "1.0", source=URLSource("http://dummy.com"))
    dep.path = tmp_path / "dep"

    tmp_component.dependencies = [dep]
    tmp_component.data = {}

    result = generate_idf_component_yml(tmp_component)

    assert (
        result
        == f"""version: 1.0.0
dependencies:
  dep:
    version: '1.0'
    override_path: {dep.path}
"""
    )


def test_generate_idf_component_yml_arduino_registry_dep(tmp_component):
    # Synthetic arduino-esp32 dep with no source / no path: should emit a
    # version-only entry so the IDF component manager resolves it from the
    # registry instead of via git.
    dep = IDFComponent("espressif/arduino-esp32", "3.3.8", source=None)

    tmp_component.dependencies = [dep]
    tmp_component.data = {}

    result = generate_idf_component_yml(tmp_component)

    assert (
        result
        == """version: 1.0.0
dependencies:
  espressif/arduino-esp32:
    version: 3.3.8
"""
    )


def test_generate_idf_component_yml_missing_path_reraises(tmp_component):
    # A dep without a path and without a recognised source should re-raise
    # the underlying RuntimeError instead of silently producing a bad manifest.
    dep = IDFComponent("foo/bar", "1.0", source=None)

    tmp_component.dependencies = [dep]
    tmp_component.data = {}

    with pytest.raises(RuntimeError):
        generate_idf_component_yml(tmp_component)


def test_check_library_data_valid(esp32_idf_core):
    _check_library_data({"platforms": "*", "frameworks": "*"})


def test_check_library_data_valid2(esp32_idf_core):
    _check_library_data({"platforms": "*"})


def test_check_library_data_valid3(esp32_idf_core):
    _check_library_data({})


def test_check_library_data_valid4(esp32_idf_core):
    _check_library_data({"platforms": "espressif32", "frameworks": "*"})


def test_check_library_data_valid5(esp32_idf_core):
    _check_library_data({"platforms": "*", "frameworks": "espidf"})


def test_check_library_data_invalid_platform(esp32_idf_core):
    with pytest.raises(InvalidIDFComponent):
        _check_library_data({"platforms": ["other"], "frameworks": "*"})


def test_check_library_data_invalid_framework(esp32_idf_core):
    with pytest.raises(InvalidIDFComponent):
        _check_library_data({"platforms": "*", "frameworks": ["other"]})


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

    assert result.libpath == [os.path.join("src", "esp32")]
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


def test_convert_library_with_repository():
    lib = Library("name", None, "https://github.com/foo/bar.git#v1.2.3")

    result = _convert_library_to_component(lib)

    assert result.name == "foo/bar"
    assert result.version == "1.2.3"
    assert isinstance(result.source, GitSource)


def test_convert_library_missing_ref():
    lib = Library("name", None, "https://github.com/foo/bar.git")

    with pytest.raises(ValueError):
        _convert_library_to_component(lib)


def test_convert_library_registry(monkeypatch):
    lib = Library("foo/bar", "^1.0.0", None)

    monkeypatch.setattr(
        esphome.espidf.component,
        "_get_package_from_pio_registry",
        lambda o, n, r: ("foo", "bar", "1.2.3", "http://example.com/pkg.zip"),
    )

    result = _convert_library_to_component(lib)

    assert result.name == "foo/bar"
    assert result.version == "1.2.3"
    assert isinstance(result.source, URLSource)


def test_process_dependencies_adds_valid_dependency(tmp_component, monkeypatch):
    tmp_component.data = {
        "dependencies": [
            {
                "name": "foo",
                "version": "1.0",
            }
        ]
    }

    monkeypatch.setattr(
        esphome.espidf.component,
        "_generate_idf_component",
        lambda lib: esphome.espidf.component.IDFComponent(
            lib.name, lib.version, source=URLSource("http://dummy.com")
        ),
    )

    monkeypatch.setattr(esphome.espidf.component, "_check_library_data", lambda x: None)

    _process_dependencies(tmp_component)

    assert len(tmp_component.dependencies) == 1


def test_process_dependencies_skips_invalid(tmp_component):
    tmp_component.data = {
        "dependencies": [
            {"name": "foo", "version": "1.0", "platforms": ["arduino"]},
            {"invalid": "entry"},
        ]
    }

    _process_dependencies(tmp_component)

    assert tmp_component.dependencies == []
