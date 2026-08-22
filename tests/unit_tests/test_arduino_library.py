"""Tests for esphome.arduino.library (Arduino-core library resolution)."""

from __future__ import annotations

from contextlib import contextmanager
import json
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.arduino import library as component
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_ESP8266
from esphome.core import CORE, EsphomeError, Library
from esphome.platformio.library import ConvertedLibrary, LibraryBackend


@pytest.fixture(autouse=True)
def _reset_libraries() -> None:
    # conftest's reset_core fixture clears platformio_libraries after each test
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP8266}


def _add_library(name: str, version: str | None, repository: str | None = None) -> None:
    CORE.add_library(Library(name=name, version=version, repository=repository))


def _make_framework(tmp_path: Path) -> Path:
    framework = tmp_path / "framework"
    lib = framework / "libraries" / "ESP8266WiFi" / "src"
    lib.mkdir(parents=True)
    (lib / "ESP8266WiFi.cpp").write_text("")
    (lib / "ESP8266WiFi.h").write_text("")
    (lib.parent / "library.properties").write_text("name=ESP8266WiFi\nversion=1.0\n")
    root_lib = framework / "libraries" / "Wire"
    root_lib.mkdir(parents=True)
    (root_lib / "Wire.cpp").write_text("")
    (root_lib / "examples").mkdir()
    (root_lib / "examples" / "scan.ino").write_text("")
    return framework


@contextmanager
def _emitting_converter(*converted):
    """Patch convert_libraries to emit the given components via the backend."""

    def fake_convert(libraries: list, backend: LibraryBackend) -> list:
        assert backend.platform == "espressif8266"
        assert backend.framework == "arduino"
        assert backend.cache_key == "arduino8266"
        for c in converted:
            backend.emit(c)
        return list(converted)

    with (
        patch.object(component, "convert_libraries", side_effect=fake_convert),
        patch.object(component, "apply_extra_script") as mock_extra,
    ):
        yield mock_extra


def _converted(name: str, source_dir: Path, data: dict) -> ConvertedLibrary:
    converted = ConvertedLibrary(name, "1.0.0", source=None)
    converted.path = source_dir
    converted.data = data
    return converted


def _resolve(framework: Path) -> list[component.ArduinoLibrary]:
    return component.resolve_libraries(
        framework,
        pio_platform="espressif8266",
        board_mcu="esp8266",
        cache_key="arduino8266",
    )


def _webserver(tmp_path: Path, data: dict) -> ConvertedLibrary:
    """Register ESPAsyncWebServer and return its converted stand-in."""
    _add_library("ESP32Async/ESPAsyncWebServer", "3.9.6")
    lib_dir = tmp_path / "converted" / "webserver"
    (lib_dir / "src").mkdir(parents=True)
    return _converted("esp32async__ESPAsyncWebServer", lib_dir, data)


def _local_lib(tmp_path: Path, dependencies: dict | list) -> None:
    """Register a local file:// library declaring the given dependencies."""
    local_lib = tmp_path / "locallib"
    (local_lib / "src").mkdir(parents=True)
    (local_lib / "src" / "local.cpp").write_text("")
    (local_lib / "library.json").write_text(
        json.dumps(
            {"name": "LocalLib", "version": "1.0.0", "dependencies": dependencies}
        )
    )
    # as_uri() forms a valid file:// URL on every platform (file:///C:/...
    # on Windows; a bare f-string would embed backslashes)
    _add_library(local_lib.as_uri(), None)


def _ws_tcp_pair(tmp_path: Path) -> tuple[ConvertedLibrary, ConvertedLibrary]:
    """Build ESPAsyncWebServer (depending on ESPAsyncTCP) plus resolved TCP."""
    ws_dir = tmp_path / "converted" / "webserver"
    (ws_dir / "src").mkdir(parents=True)
    tcp_dir = tmp_path / "converted" / "tcp"
    (tcp_dir / "src").mkdir(parents=True)
    ws = _converted(
        "esp32async__ESPAsyncWebServer",
        ws_dir,
        {"build": {}, "dependencies": [{"name": "ESPAsyncTCP"}]},
    )
    tcp = _converted("esp32async__ESPAsyncTCP", tcp_dir, {"build": {}})
    return ws, tcp


def test_library_info_src_layout(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    lib = component._bundled_library(framework, "ESP8266WiFi")
    assert lib.name == "ESP8266WiFi"
    assert [p.name for p in lib.sources] == ["ESP8266WiFi.cpp"]
    assert lib.include_dirs == [(framework / "libraries/ESP8266WiFi/src").resolve()]


def test_library_info_root_layout_excludes_examples(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    lib = component._bundled_library(framework, "Wire")
    assert [p.name for p in lib.sources] == ["Wire.cpp"]
    assert lib.include_dirs == [(framework / "libraries/Wire").resolve()]


def test_library_info_flags_parsing(tmp_path: Path) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    (read_path / "src" / "a.cpp").write_text("")
    (read_path / "inc").mkdir()
    (read_path / "blobs").mkdir()
    data = {
        "build": {
            "flags": [
                "-DFOO=1 -I inc",
                "-lalgobsec",
                "-fno-lto",
                "-Wl,--wrap=malloc",
                # Bare flags join their argument within one entry only, as
                # ParseFlags lexes each entry independently
                "-l m",
                "-L blobs",
            ],
        }
    }
    lib = component._library_info("x", read_path, data)
    assert lib.flags == ["-DFOO=1", "-fno-lto"]
    assert lib.include_dirs == [
        (read_path / "src").resolve(),
        (read_path / "inc").resolve(),
    ]
    assert lib.link_dirs == [(read_path / "blobs").resolve()]
    assert lib.link_libs == ["algobsec", "m"]
    assert lib.link_flags == ["-Wl,--wrap=malloc"]


def test_library_info_missing_link_dir_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    read_path.mkdir()
    data = {"build": {"flags": ["-Lmissing_blobs"]}}
    lib = component._library_info("x", read_path, data)
    assert "declares library dir missing_blobs which does not exist" in caplog.text
    # Kept anyway: the linker ignores missing -L dirs
    assert lib.link_dirs == [(read_path / "missing_blobs").resolve()]


def test_library_info_declared_filter_matches_nothing_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    data = {"build": {"srcFilter": ["+<nothing/*>"]}}
    lib = component._library_info("x", read_path, data)
    assert not lib.sources
    assert "declares srcFilter/srcDir but no source files matched" in caplog.text


def test_library_info_header_only_does_not_warn(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {})
    assert not lib.sources
    assert "no source files matched" not in caplog.text


def test_library_info_no_src_dir(tmp_path: Path) -> None:
    read_path = tmp_path / "empty"
    read_path.mkdir()
    lib = component._library_info("x", read_path, {})
    # With no manifest hints the source dir falls back to the library root
    assert lib.sources == []
    assert lib.include_dirs == [read_path.resolve()]


def test_resolve_libraries_bundled(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    _add_library("ESP8266WiFi", None)
    libs = _resolve(framework)
    assert [lib.name for lib in libs] == ["ESP8266WiFi"]


@pytest.mark.parametrize("version", [None, "1.1.0"])
def test_resolve_libraries_registry_name_is_external(
    tmp_path: Path, version: str | None
) -> None:
    """A name that is not bundled reaches the converter: bare resolves from
    the registry at the latest version (matching PlatformIO and the
    documented libraries: key) and a version pin is a registry package."""
    framework = _make_framework(tmp_path)
    _add_library("pngle", version)
    with patch.object(component, "convert_libraries", return_value=[]) as mock_convert:
        _resolve(framework)
    (libraries, _backend), _ = mock_convert.call_args
    assert [lib.name for lib in libraries] == ["pngle"]


def test_resolve_libraries_external_and_bundled_deps(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    _add_library("ESP32Async/ESPAsyncWebServer", "3.9.6")

    lib_dir = tmp_path / "converted" / "webserver"
    (lib_dir / "src").mkdir(parents=True)
    (lib_dir / "src" / "server.cpp").write_text("")
    converted = _converted(
        "esp32async__ESPAsyncWebServer",
        lib_dir,
        {
            "build": {},
            "dependencies": [
                # Version-less bundled dependency: resolved from the framework
                {"name": "Wire", "platforms": "espressif8266"},
                # Wrong platform: skipped
                {"name": "ESP8266WiFi", "platforms": "espressif32"},
                # Registry dependency with a version: handled by the converter
                {"name": "ESPAsyncTCP", "owner": "ESP32Async", "version": "^2.0.0"},
                # Not bundled: skipped
                {"name": "NotBundled"},
            ],
        },
    )

    with _emitting_converter(converted) as mock_extra:
        libs = _resolve(framework)

    mock_extra.assert_called_once()
    assert mock_extra.call_args.args == (converted,)
    assert mock_extra.call_args.kwargs["pio_platform"] == "espressif8266"
    # board_mcu is passed lazily, as the shared helper requires
    assert mock_extra.call_args.kwargs["board_mcu"]() == "esp8266"
    assert [lib.name for lib in libs] == [
        "Wire",
        "esp32async__ESPAsyncWebServer",
    ]


def test_resolve_libraries_bundled_dep_already_present(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    _add_library("Wire", None)
    _add_library("Some/External", "1.0.0")

    lib_dir = tmp_path / "converted" / "external"
    lib_dir.mkdir(parents=True)
    converted = _converted(
        "some__External", lib_dir, {"dependencies": [{"name": "Wire"}]}
    )

    with _emitting_converter(converted):
        libs = _resolve(framework)

    # Wire appears once (from the explicit registration), not twice
    assert [lib.name for lib in libs] == ["Wire", "some__External"]


def test_library_info_trailing_bare_flag_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {"build": {"flags": ["-DA=1 -l"]}})
    assert lib.flags == ["-DA=1"]
    assert lib.link_libs == []
    assert "Ignoring trailing '-l'" in caplog.text


def test_library_info_missing_explicit_include_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {"build": {"flags": ["-Inope"]}})
    assert lib.include_dirs == [(read_path / "src").resolve()]
    assert "include dir nope which does not exist" in caplog.text


def test_library_info_missing_declared_src_dir_raises(tmp_path: Path) -> None:
    """An explicitly declared srcDir that does not exist is a manifest error."""
    read_path = tmp_path / "lib"
    read_path.mkdir()
    with pytest.raises(EsphomeError, match="srcDir 'nosrc' which does not exist"):
        component._library_info("x", read_path, {"build": {"srcDir": "nosrc"}})


def test_library_info_missing_declared_include_dir_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    read_path.mkdir()
    component._library_info("x", read_path, {"build": {"includeDir": "noinc"}})
    assert "include dir noinc which does not exist" in caplog.text


def test_resolve_libraries_lib_ignore_covers_bundled(tmp_path: Path) -> None:
    """lib_ignore applies to framework-bundled libraries, as under PlatformIO."""
    framework = _make_framework(tmp_path)
    _add_library("ESP8266WiFi", None)
    _add_library("Wire", None)
    CORE.platformio_options = {"lib_ignore": ["Wire"]}
    libs = _resolve(framework)
    assert [lib.name for lib in libs] == ["ESP8266WiFi"]


def test_resolve_libraries_lib_ignore_covers_bundled_dependencies(
    tmp_path: Path,
) -> None:
    framework = _make_framework(tmp_path)
    _add_library("Some/External", "1.0.0")
    CORE.platformio_options = {"lib_ignore": ["Wire"]}

    lib_dir = tmp_path / "converted" / "external"
    lib_dir.mkdir(parents=True)
    converted = _converted(
        "some__External", lib_dir, {"dependencies": [{"name": "Wire"}]}
    )

    with _emitting_converter(converted):
        libs = _resolve(framework)

    assert [lib.name for lib in libs] == ["some__External"]


def test_bundled_library_prefers_library_json(tmp_path: Path) -> None:
    """A bundled library.json wins over library.properties (PIO semantics);
    its build section is honored."""
    framework = _make_framework(tmp_path)
    lib_dir = framework / "libraries" / "GDBStub"
    (lib_dir / "custom").mkdir(parents=True)
    (lib_dir / "custom" / "gdb.cpp").write_text("")
    (lib_dir / "library.properties").write_text("name=GDBStub\n")
    (lib_dir / "library.json").write_text(
        '{"name": "GDBStub", "build": {"srcDir": "custom"}}'
    )
    lib = component._bundled_library(framework, "GDBStub")
    assert [s.name for s in lib.sources] == ["gdb.cpp"]


def test_library_info_lib_archive_flag(tmp_path: Path) -> None:
    """Both libArchive (library.json) and dot_a_linkage (properties) reach
    the generator's contract; default is archive."""
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    assert component._library_info("x", read_path, {}).lib_archive is True
    assert (
        component._library_info(
            "x", read_path, {"build": {"libArchive": False}}
        ).lib_archive
        is False
    )
    assert (
        component._library_info("x", read_path, {"dot_a_linkage": "false"}).lib_archive
        is False
    )
    assert (
        component._library_info("x", read_path, {"dot_a_linkage": "true"}).lib_archive
        is True
    )


def test_resolve_libraries_dep_warnings(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A nameless dependency entry warns; an owner-without-version entry is
    left to the shared walk's reconciliation (no local warning)."""
    framework = _make_framework(tmp_path)
    converted = _webserver(
        tmp_path,
        {
            "build": {},
            "dependencies": [
                {"owner": "someone"},
                {"name": "Orphan", "owner": "someone"},
            ],
        },
    )
    with _emitting_converter(converted):
        _resolve(framework)
    assert "malformed dependency entry" in caplog.text
    assert "Orphan" not in caplog.text


def test_bundled_dependency_nonplatform_rejection_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An InvalidLibrary whose cause is not the platform filter is visible."""
    from esphome.platformio.library import InvalidLibrary

    framework = _make_framework(tmp_path)
    converted = _webserver(tmp_path, {"build": {}, "dependencies": [{"name": "Wire"}]})
    import esphome.platformio.library as pio_library

    with (
        _emitting_converter(converted),
        patch.object(
            pio_library,
            "check_library_data",
            side_effect=InvalidLibrary("manifest is corrupt"),
        ),
    ):
        _resolve(framework)
    assert "Skipping dependency Wire" in caplog.text
    assert "manifest is corrupt" in caplog.text


@pytest.mark.parametrize("declared", ["", None])
def test_library_info_falsy_declared_src_dir_raises(
    tmp_path: Path, declared: str | None
) -> None:
    """A declared-but-falsy srcDir must not silently fall back to the probe."""
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    with pytest.raises(EsphomeError, match="does not exist"):
        component._library_info("x", read_path, {"build": {"srcDir": declared}})


@pytest.mark.parametrize(
    ("value", "expected", "warns"),
    [
        (False, False, False),
        ("false", False, False),
        ("False", False, False),
        ("true", True, False),
        ("archive-me", True, True),
    ],
)
def test_library_info_lib_archive_parse(
    tmp_path: Path,
    value: object,
    expected: bool,
    warns: bool,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """bool("false") is True; the string forms must parse, not coerce."""
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {"build": {"libArchive": value}})
    assert lib.lib_archive is expected
    assert ("unrecognized libArchive" in caplog.text) is warns


def test_bundled_dependency_dict_shorthand_prefers_bundled(tmp_path: Path) -> None:
    """The {"Wire": "*"} dict shorthand (version="*", no owner) must resolve
    to the bundled library, matching PIO's process_dependencies, instead of
    being routed to the registry."""
    framework = _make_framework(tmp_path)
    converted = _webserver(tmp_path, {"build": {}, "dependencies": {"Wire": "*"}})
    with _emitting_converter(converted):
        libs = _resolve(framework)
    assert "Wire" in [lib.name for lib in libs]


def test_bundled_dependency_platform_rejection_is_debug(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """The typed IncompatiblePlatform (the routine cross-platform skip)
    stays at debug regardless of message wording."""
    from esphome.platformio.library import IncompatiblePlatform

    framework = _make_framework(tmp_path)
    converted = _webserver(tmp_path, {"build": {}, "dependencies": [{"name": "Wire"}]})
    import esphome.platformio.library as pio_library

    with (
        _emitting_converter(converted),
        patch.object(
            pio_library,
            "check_library_data",
            side_effect=IncompatiblePlatform("nothing about the p-word here"),
        ),
    ):
        _resolve(framework)
    assert "Skipping dependency Wire" not in caplog.text


@pytest.mark.parametrize("data", [{"build": "src"}, [], "nope"])
def test_library_info_malformed_manifest_is_named(tmp_path: Path, data: object) -> None:
    """A malformed manifest names the library, never an AttributeError."""
    read_path = tmp_path / "lib"
    read_path.mkdir()
    with pytest.raises(EsphomeError, match="Library x has a malformed manifest"):
        component._library_info("x", read_path, data)


def test_bundled_library_with_declared_dependencies_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A bundled manifest that declares dependencies is visible, not
    silently skipped (a no-op for the ESP8266 core, not for every core)."""
    framework = _make_framework(tmp_path)
    wire = framework / "libraries" / "Wire"
    (wire / "library.json").write_text(
        '{"name": "Wire", "dependencies": [{"name": "SPI"}]}'
    )
    _add_library("Wire", None)
    _resolve(framework)
    assert "Bundled library Wire declares dependencies" in caplog.text


@pytest.mark.parametrize(
    ("build", "match"),
    [
        ({"includeDir": ["a", "b"]}, "malformed includeDir"),
        ({"srcFilter": [123]}, "malformed srcFilter"),
    ],
)
def test_library_info_malformed_build_fields_are_named(
    tmp_path: Path, build: dict, match: str
) -> None:
    """Malformed includeDir/srcFilter fail naming the library like srcDir."""
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    with pytest.raises(EsphomeError, match=match):
        component._library_info("x", read_path, {"build": build})


@pytest.mark.parametrize(
    ("value", "expected", "warns"),
    [
        ("true", True, False),
        ("False", False, False),
        ("yes", True, True),
    ],
)
def test_library_info_dot_a_linkage_parses_strictly(
    tmp_path: Path,
    value: str,
    expected: bool,
    warns: bool,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The dot_a_linkage property uses the same strict table as libArchive; a typo warns
    and keeps the archive default instead of silently flipping linkage."""
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {"dot_a_linkage": value, "build": {}})
    assert lib.lib_archive is expected
    assert ("unrecognized dot_a_linkage" in caplog.text) is warns


def test_bundled_library_properties_depends_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """The library.properties depends= spelling reaches the visibility
    warning too; the shared parser returns it raw."""
    framework = _make_framework(tmp_path)
    wire = framework / "libraries" / "Wire"
    (wire / "library.properties").write_text("name=Wire\nversion=1.0\ndepends=SPI\n")
    _add_library("Wire", None)
    _resolve(framework)
    assert "Library Wire declares dependencies via library.properties" in caplog.text


def test_bundled_library_extra_script_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A bundled manifest relying on an extraScript is a named deviation,
    not a silently miscompiled library."""
    framework = _make_framework(tmp_path)
    wire = framework / "libraries" / "Wire"
    (wire / "library.json").write_text(
        '{"name": "Wire", "build": {"extraScript": "extra.py"}}'
    )
    _add_library("Wire", None)
    _resolve(framework)
    assert "declares an extraScript" in caplog.text


def test_dependency_requested_top_level_is_not_a_drop(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A version-less manifest dependency the config separately requests is
    already in the build; it is not probed as a bundled library."""
    framework = _make_framework(tmp_path)
    _add_library("ESP32Async/ESPAsyncWebServer", "3.9.6")
    _add_library("ESP32Async/ESPAsyncTCP", "2.0.0")
    ws, tcp = _ws_tcp_pair(tmp_path)
    with _emitting_converter(ws, tcp):
        libs = _resolve(framework)
    # Exactly the two converted libraries; no bundled stand-in was added
    assert [lib.name for lib in libs] == [
        "esp32async__ESPAsyncWebServer",
        "esp32async__ESPAsyncTCP",
    ]
    assert "Skipping" not in caplog.text


def test_bundled_library_non_dict_manifest_skips_probes_and_raises(
    tmp_path: Path,
) -> None:
    """A bundled library.json that is a JSON array skips the dependency and
    extraScript probes and fails in _library_info naming the library."""
    framework = _make_framework(tmp_path)
    wire = framework / "libraries" / "Wire"
    (wire / "library.json").write_text('["not", "a", "manifest"]')
    with pytest.raises(EsphomeError, match="Library Wire has a malformed manifest"):
        component._bundled_library(framework, "Wire")


def test_dict_shorthand_dependency_skips_registry_through_real_converter(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """{"Wire": "*"} in a real manifest must never reach the registry: the
    graph walk skips backend-provided names and the bundled copy is added
    after emit (no converter mock; a registry touch fails the test)."""
    import esphome.platformio.library as pio_library

    framework = _make_framework(tmp_path)
    _local_lib(tmp_path, {"Wire": "*"})
    # Pin the component cache to tmp_path (data_dir honors an ambient
    # ESPHOME_DATA_DIR otherwise)
    monkeypatch.setenv("ESPHOME_DATA_DIR", str(tmp_path / ".esphome"))
    with patch.object(
        pio_library,
        "_resolve_registry_version",
        side_effect=AssertionError("registry touched"),
    ):
        libs = _resolve(framework)
    names = [lib.name for lib in libs]
    assert "Wire" in names
    assert any("locallib" in n.lower() for n in names)


def test_converted_properties_depends_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A converted library shipping only the properties depends= spelling
    is visible, not a silent bundled-dependency drop."""
    framework = _make_framework(tmp_path)
    converted = _webserver(tmp_path, {"build": {}, "depends": "Wire,SPI"})
    with _emitting_converter(converted):
        _resolve(framework)
    assert "declares dependencies via library.properties" in caplog.text


@pytest.mark.parametrize(
    ("bad_name", "message"),
    [
        # A non-string name never leaves the shared normalizer
        (1, "Ignoring unrecognized dependency entry"),
        ("../escape", "Ignoring malformed dependency entry"),
        ("a/b", "Ignoring malformed dependency entry"),
        ("..", "Ignoring malformed dependency entry"),
    ],
)
def test_bundled_dependency_bad_name_is_malformed(
    tmp_path: Path, bad_name: object, message: str, caplog: pytest.LogCaptureFixture
) -> None:
    """A dependency name becomes a path component; a traversal or a
    non-string is a malformed entry, never joined."""
    framework = _make_framework(tmp_path)
    converted = _webserver(
        tmp_path, {"build": {}, "dependencies": [{"name": bad_name}]}
    )
    with _emitting_converter(converted):
        _resolve(framework)
    assert message in caplog.text


def test_bundled_dependency_string_list_form(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """The bare string-list dependency form (PIO-legal) resolves to the
    bundled library instead of vanishing in normalization."""
    framework = _make_framework(tmp_path)
    converted = _webserver(tmp_path, {"build": {}, "dependencies": ["Wire"]})
    with _emitting_converter(converted):
        libs = _resolve(framework)
    assert "Wire" in [lib.name for lib in libs]


def test_pinned_bundled_dependency_substitution_warns(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A non-* version pin on a backend-provided dependency is discarded
    for the bundled copy; the substitution must be visible."""
    import esphome.platformio.library as pio_library

    framework = _make_framework(tmp_path)
    _local_lib(tmp_path, {"Wire": "^2.0.0"})
    monkeypatch.setenv("ESPHOME_DATA_DIR", str(tmp_path / ".esphome"))
    with patch.object(
        pio_library,
        "_resolve_registry_version",
        side_effect=AssertionError("registry touched"),
    ):
        libs = _resolve(framework)
    assert "Wire" in [lib.name for lib in libs]
    assert "pins version ^2.0.0; using the library bundled" in caplog.text


def test_transitively_resolved_dependency_does_not_warn(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A version-less dependency the walk resolved as another library's
    registry dependency is present in the build; the skipping warning must
    stay quiet for it."""
    framework = _make_framework(tmp_path)
    _add_library("ESP32Async/ESPAsyncWebServer", "3.9.6")
    ws, tcp = _ws_tcp_pair(tmp_path)
    with _emitting_converter(ws, tcp):
        libs = _resolve(framework)
    assert [lib.name for lib in libs] == [
        "esp32async__ESPAsyncWebServer",
        "esp32async__ESPAsyncTCP",
    ]
    assert "Skipping" not in caplog.text


def test_empty_bundled_library_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A bundled directory with no sources or headers is a broken install
    that can never link; fail by name instead of warning into it."""
    framework = _make_framework(tmp_path)
    (framework / "libraries" / "Empty").mkdir()
    _add_library("Empty", None)
    with pytest.raises(
        EsphomeError, match="Bundled library Empty has no sources or headers"
    ):
        _resolve(framework)


def test_versionless_dependency_with_provider_stays_quiet(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """With a provides backend the version-less skip is routine (debug) and
    the bundled copy is picked up after emit."""
    import esphome.platformio.library as pio_library

    framework = _make_framework(tmp_path)
    _local_lib(tmp_path, [{"name": "Wire"}])
    monkeypatch.setenv("ESPHOME_DATA_DIR", str(tmp_path / ".esphome"))
    with patch.object(
        pio_library,
        "_resolve_registry_version",
        side_effect=AssertionError("registry touched"),
    ):
        libs = _resolve(framework)
    assert "Wire" in [lib.name for lib in libs]
    assert "has no version to resolve" not in caplog.text
