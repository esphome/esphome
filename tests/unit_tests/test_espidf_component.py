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


def test_extra_script_logs_warning(caplog, esp32_idf_core):
    extra_script = "myscript.sh"

    with caplog.at_level("WARNING"):
        _check_library_data({"build": {"extraScript": extra_script}})

    assert "not supported" in caplog.text
    assert "myscript.sh" in caplog.text


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
