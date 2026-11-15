"""Tests for attributing ESP8266 string literals in RAM to components."""

from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.__main__ import _secret_values
from esphome.analyze_memory import (
    MemorySection,
    StringLiteral,
    _parse_string_sections,
    _ram_string_size,
)
from esphome.analyze_memory.cli import MemoryAnalyzerCLI
from esphome.core import CORE

from . import make_analyzer

WEB_OBJ = "src/esphome/components/web_server/web_server.cpp.o"
LIGHT_OBJ = "src/esphome/components/light/light_json_schema.cpp.o"
COVER_JSON = "_ZN7esphome10web_server9WebServer11cover_json_Ev"
MATCH_URL = "_ZN7esphome10web_serverL9match_urlEPKcjbb"
DUMP_JSON = "_ZN7esphome5light15LightJSONSchema9dump_jsonEv"

SECTIONS = """\
File: {web}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 7] .rodata           PROGBITS        00000000 000040 000008 00   A  0   0  4
  [209] .rodata.str1.1    PROGBITS        00000000 000598 00000a 01 AMS  0   0  1
  [481] .rodata.{cover}.str1.1 PROGBITS        00000000 003910 00000c 01 AMS  0   0  1

File: {light}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 30] .rodata.{dump}.str1.1 PROGBITS        00000000 000100 000007 01 AMS  0   0  1
"""


def _hex_dump(section: str, data: bytes, address: int = 0) -> str:
    """Format bytes the way ``readelf -x`` does."""
    lines = [f"Hex dump of section '{section}':"]
    for start in range(0, len(data), 16):
        chunk = data[start : start + 16]
        words = " ".join(chunk[i : i + 4].hex() for i in range(0, len(chunk), 4))
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"  0x{address + start:08x} {words:<35} {text}")
    return "\n".join(lines) + "\n\n"


# "OPEN\0CLOSED\0" for cover_json_, "ON\0all\0none\0" shared by match_url
WEB_STRINGS = (
    f"""\
Relocation section '.rela.text.{MATCH_URL}' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000004  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0

"""
    + _hex_dump(".rodata.str1.1", b"ON\0all\0none\0")
    + _hex_dump(f".rodata.{COVER_JSON}.str1.1", b"OPEN\0CLOSED\0")
)

# The literal pool word of match_url holds the literal's offset, 3 ("all")
WEB_REFERRERS = _hex_dump(f".text.{MATCH_URL}", bytes(4) + (3).to_bytes(4, "little"))

LIGHT_STRINGS = _hex_dump(f".rodata.{DUMP_JSON}.str1.1", b"ON\0OFF\0")

# Linked .rodata: "OFF" is kept on its own and "ON" only as the tail of "MON"
ELF_RODATA = _hex_dump(".rodata", b"MON\0OFF\0CLOSED\0all\0", 0x3FFE86B0)


def _completed(stdout: str) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess([], 0, stdout=stdout)


def test_parse_string_sections() -> None:
    """Only string literal sections are listed, named after their function or not."""
    output = SECTIONS.format(
        web="web.o", light="light.o", cover=COVER_JSON, dump=DUMP_JSON
    )
    assert _parse_string_sections(output) == {
        "web.o": {"209": ".rodata.str1.1", "481": f".rodata.{COVER_JSON}.str1.1"},
        "light.o": {"30": f".rodata.{DUMP_JSON}.str1.1"},
    }


@pytest.mark.parametrize(
    ("text", "expected"),
    [(b"OFF", 4), (b"MON", 4), (b"ON", 0), (b"missing", 0), (b"all", 4)],
)
def test_ram_string_size(text: bytes, expected: int) -> None:
    """A literal stored only as the tail of a longer one, or dropped, takes no RAM."""
    rodata = b"MON\0OFF\0CLOSED\0all\0"
    assert _ram_string_size(text, rodata) == expected


@pytest.fixture
def obj_dir(tmp_path: Path) -> Path:
    for rel in (WEB_OBJ, LIGHT_OBJ, "src/main.cpp.o"):
        path = tmp_path / "obj" / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    return tmp_path / "obj"


def _analyze(obj_dir: Path, kept: set[str]) -> MemoryAnalyzerCLI:
    analyzer = make_analyzer(cls=MemoryAnalyzerCLI)
    analyzer.elf_path = obj_dir.parent / "firmware.elf"
    analyzer.rodata_in_ram = True
    analyzer.sections[".data"] = MemorySection(".data", total_size=1000)
    analyzer._elf_symbol_names = kept
    web = str(obj_dir / WEB_OBJ)
    light = str(obj_dir / LIGHT_OBJ)
    sections = SECTIONS.format(web=web, light=light, cover=COVER_JSON, dump=DUMP_JSON)

    def run_tool(cmd: list[str]) -> subprocess.CompletedProcess[str]:
        if "-SW" in cmd:
            return _completed(sections)
        if cmd[-1].endswith("firmware.elf"):
            return _completed(ELF_RODATA)
        if cmd[-1] == light:
            return _completed(LIGHT_STRINGS)
        if "-rW" not in cmd:
            assert cmd[1:-1] == ["-x", f".text.{MATCH_URL}"]
            return _completed(WEB_REFERRERS)
        if "7" in cmd:  # the unnamed constant scan of .rodata
            return _completed("")
        return _completed(WEB_STRINGS)

    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_object_rodata()
    return analyzer


def test_string_literals_attributed(obj_dir: Path) -> None:
    """Each literal is counted once, with the bytes it takes in the linked .rodata."""
    analyzer = _analyze(obj_dir, {COVER_JSON, MATCH_URL, DUMP_JSON})
    assert analyzer._string_literals == [
        StringLiteral("CLOSED", 7, {"[esphome]web_server"}, {COVER_JSON}),
        StringLiteral("OFF", 4, {"[esphome]light"}, {DUMP_JSON}),
        StringLiteral("all", 4, {"[esphome]web_server"}, {MATCH_URL}),
    ]
    # "OPEN" is not in the ELF and "ON" is only the tail of "MON"
    assert analyzer.components["[esphome]web_server"].data_size == 11
    assert analyzer.components["[esphome]light"].data_size == 4
    assert analyzer.sections[".data"].symbol_size == 15


def test_string_literals_skip_dropped_functions(obj_dir: Path) -> None:
    """Literals of functions the linker dropped are not charged."""
    analyzer = _analyze(obj_dir, {DUMP_JSON})
    assert [s.text for s in analyzer._string_literals] == ["OFF"]


def test_string_literals_shared_between_components(obj_dir: Path) -> None:
    """A literal two components use is listed under both and charged to neither."""
    analyzer = _analyze(obj_dir, {DUMP_JSON})
    analyzer._string_literals = []
    with patch.object(
        analyzer,
        "_scan_object_strings",
        side_effect=[{b"OFF": {COVER_JSON}}, {b"OFF": {DUMP_JSON}}],
    ):
        analyzer._elf_symbol_names = {COVER_JSON, DUMP_JSON}
        analyzer.components.clear()
        analyzer.sections[".data"].symbol_size = 0
        with patch(
            "esphome.analyze_memory.run_tool", return_value=_completed(ELF_RODATA)
        ):
            sections = SECTIONS.format(
                web=str(obj_dir / WEB_OBJ),
                light=str(obj_dir / LIGHT_OBJ),
                cover=COVER_JSON,
                dump=DUMP_JSON,
            )
            analyzer._attribute_string_literals(sections, obj_dir)
    (literal,) = analyzer._string_literals
    assert literal.components == {"[esphome]web_server", "[esphome]light"}
    assert analyzer.sections[".data"].symbol_size == 0
    report = analyzer.generate_report()
    assert report.count('"OFF"') == 2


def test_report_groups_by_component_and_masks_secrets() -> None:
    """Strings are listed by component; !secret values are never printed."""
    analyzer = make_analyzer(cls=MemoryAnalyzerCLI)
    analyzer.secret_values = {"hunter2": "wifi_password"}
    analyzer._demangle_cache = {
        "setup": "setup",
        COVER_JSON: "WebServer::cover_json_()",
    }
    analyzer._string_literals = [
        StringLiteral("hunter2", 8, {"[esphome]core"}, {"setup"}),
        StringLiteral("CLOSED", 7, {"[esphome]web_server"}, {COVER_JSON}),
    ]
    report = analyzer.generate_report()
    assert "String Literals in RAM (.rodata)" in report
    assert "[esphome]core (8 B, 1 strings):" in report
    assert "!secret wifi_password" in report
    assert "hunter2" not in report
    assert '"CLOSED"' in report
    assert "WebServer::cover_json_()" in report


@pytest.mark.parametrize(
    ("rel_path", "component"),
    [
        (
            "lib/esp32async__ESPAsyncWebServer/WebHandlers.cpp.o",
            "[lib]espasyncwebserver",
        ),
        ("lib/ESP8266WiFi/ESP8266WiFi.cpp.o", "[lib]esp8266wifi"),
        ("core/core_esp8266_main.cpp.o", "arduino_core"),
    ],
)
def test_native_library_objects(rel_path: str, component: str) -> None:
    """Objects of the native ESP8266 build's libraries and core get a component."""
    assert make_analyzer()._source_file_to_component(rel_path) == component


def test_secret_values(tmp_path: Path) -> None:
    """The configuration's !secret values are collected to mask them."""
    (tmp_path / "secrets.yaml").write_text("wifi_password: hunter2\n")
    config = tmp_path / "device.yaml"
    config.write_text("wifi:\n  password: !secret wifi_password\n")
    with patch.object(CORE, "config_path", config):
        assert _secret_values() == {"hunter2": "wifi_password"}


def test_secret_values_unreadable(tmp_path: Path) -> None:
    """A configuration that cannot be read leaves nothing to mask."""
    with patch.object(CORE, "config_path", tmp_path / "missing.yaml"):
        assert _secret_values() == {}
