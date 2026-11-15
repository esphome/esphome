"""Tests for attributing ESP8266 string literals in RAM to components."""

from pathlib import Path
import re
import subprocess
from unittest.mock import patch

import pytest

from esphome.analyze_memory import (
    MemorySection,
    StringLiteral,
    _parse_string_sections,
    _ram_string_size,
)
from esphome.analyze_memory.cli import MemoryAnalyzerCLI

from . import completed, hex_dump, make_analyzer

WEB_OBJ = "src/esphome/components/web_server/web_server.cpp.o"
LIGHT_OBJ = "src/esphome/components/light/light_json_schema.cpp.o"
COVER_JSON = "_ZN7esphome10web_server9WebServer11cover_json_Ev"
MATCH_URL = "_ZN7esphome10web_serverL9match_urlEPKcjbb"
DUMP_JSON = "_ZN7esphome5light15LightJSONSchema9dump_jsonEv"

_SECTIONS = """\
File: {web}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 7] .rodata           PROGBITS        00000000 000040 000008 00   A  0   0  4
  [209] .rodata.str1.1    PROGBITS        00000000 000598 00000a 01 AMS  0   0  1
  [481] .rodata.{cover_json}.str1.1 PROGBITS        00000000 003910 00000c 01 AMS  0   0  1

File: {light}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 30] .rodata.{dump_json}.str1.1 PROGBITS        00000000 000100 000007 01 AMS  0   0  1
"""


def _sections(web: str, light: str) -> str:
    """``readelf -SW`` output for the two objects."""
    return _SECTIONS.format(
        web=web, light=light, cover_json=COVER_JSON, dump_json=DUMP_JSON
    )


# "OPEN\0CLOSED\0" for cover_json_, "ON\0all\0none\0" shared by match_url
WEB_STRINGS = (
    f"""\
Relocation section '.rela.text.{MATCH_URL}' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000004  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0

"""
    + hex_dump(".rodata.str1.1", b"ON\0all\0none\0")
    + hex_dump(f".rodata.{COVER_JSON}.str1.1", b"OPEN\0CLOSED\0")
)

# The literal pool word of match_url holds the literal's offset, 3 ("all")
WEB_REFERRERS = hex_dump(f".text.{MATCH_URL}", bytes(4) + (3).to_bytes(4, "little"))

LIGHT_STRINGS = hex_dump(f".rodata.{DUMP_JSON}.str1.1", b"ON\0OFF\0")

# Linked .rodata: "OPEN" and "ON" were dropped
ELF_RODATA = hex_dump(".rodata", b"OFF\0CLOSED\0all\0", 0x3FFE86B0)


def test_parse_string_sections() -> None:
    """Only string literal sections are listed, named after their function or not."""
    assert _parse_string_sections(_sections("web.o", "light.o")) == {
        "web.o": {
            "209": (".rodata.str1.1", None),
            "481": (f".rodata.{COVER_JSON}.str1.1", COVER_JSON),
        },
        "light.o": {"30": (f".rodata.{DUMP_JSON}.str1.1", DUMP_JSON)},
    }


@pytest.mark.parametrize(
    ("text", "expected"),
    [
        (b"OFF", 4),
        (b"MON", 4),
        # Stored only as the tail of "MON"
        (b"ON", 0),
        (b"missing", 0),
        # After a float constant ending in 0x3f, not inside another literal
        (b"all", 4),
    ],
)
def test_ram_string_size(text: bytes, expected: int) -> None:
    """A literal stored only as the tail of a longer one, or dropped, takes no RAM."""
    rodata = b"MON\0OFF\0CLOSED\0\x00\x00\x80\x3fall\0"
    literals = {b"MON", b"OFF", b"ON", b"CLOSED", b"all", b"missing"}
    assert _ram_string_size(text, rodata, literals) == expected


def test_ram_string_size_tail_after_other_data() -> None:
    """A tail of a literal that follows binary data is still only a tail."""
    rodata = b"\x00\x00\x80\x3fMON\0"
    assert _ram_string_size(b"MON", rodata, {b"MON", b"ON"}) == 4
    assert _ram_string_size(b"ON", rodata, {b"MON", b"ON"}) == 0


def test_string_literals_missing_rodata_warns(
    obj_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Without the ELF's .rodata dump the report says why strings are missing."""
    analyzer = _new_analyzer(obj_dir, {COVER_JSON})
    with patch("esphome.analyze_memory.run_tool", return_value=completed("")):
        analyzer._attribute_string_literals(
            _sections(str(obj_dir / WEB_OBJ), str(obj_dir / LIGHT_OBJ)), obj_dir
        )
    assert analyzer._string_literals == []
    assert "No .rodata dump" in caplog.text


@pytest.fixture
def obj_dir(tmp_path: Path) -> Path:
    for rel in (WEB_OBJ, LIGHT_OBJ, "src/main.cpp.o"):
        path = tmp_path / "obj" / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    return tmp_path / "obj"


def _new_analyzer(obj_dir: Path, kept: set[str]) -> MemoryAnalyzerCLI:
    analyzer = make_analyzer(cls=MemoryAnalyzerCLI)
    analyzer.elf_path = obj_dir.parent / "firmware.elf"
    analyzer.rodata_in_ram = True
    analyzer.sections[".data"] = MemorySection(".data", total_size=1000)
    analyzer._elf_symbol_names = kept
    # Unnamed constants have their own tests
    analyzer._attribute_unnamed_constants = lambda *_: None
    return analyzer


def _analyze(obj_dir: Path, kept: set[str]) -> MemoryAnalyzerCLI:
    analyzer = _new_analyzer(obj_dir, kept)
    light = str(obj_dir / LIGHT_OBJ)
    sections = _sections(str(obj_dir / WEB_OBJ), light)

    def run_tool(cmd: list[str]) -> subprocess.CompletedProcess[str]:
        if "-SW" in cmd:
            return completed(sections)
        if cmd[-1].endswith("firmware.elf"):
            return completed(ELF_RODATA)
        if cmd[-1] == light:
            return completed(LIGHT_STRINGS)
        if "-rW" not in cmd:
            assert cmd[1:-1] == ["-x", f".text.{MATCH_URL}"]
            return completed(WEB_REFERRERS)
        return completed(WEB_STRINGS)

    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_object_rodata()
    return analyzer


def test_string_literals_attributed(obj_dir: Path) -> None:
    """Each literal is counted once, with the bytes it takes in the linked .rodata."""
    analyzer = _analyze(obj_dir, {COVER_JSON, MATCH_URL, DUMP_JSON})
    assert analyzer._string_literals == [
        StringLiteral("CLOSED", 7, {"[esphome]web_server": {COVER_JSON}}),
        StringLiteral("OFF", 4, {"[esphome]light": {DUMP_JSON}}),
        StringLiteral("all", 4, {"[esphome]web_server": {MATCH_URL}}),
    ]
    # "OPEN" and "ON" are not in the ELF
    assert analyzer.components["[esphome]web_server"].data_size == 11
    assert analyzer.components["[esphome]light"].data_size == 4
    assert analyzer.sections[".data"].symbol_size == 15


def test_string_literals_skip_dropped_functions(obj_dir: Path) -> None:
    """Literals of functions the linker dropped are not charged."""
    analyzer = _analyze(obj_dir, {DUMP_JSON})
    assert [s.text for s in analyzer._string_literals] == ["OFF"]


def test_string_literals_shared_between_components(obj_dir: Path) -> None:
    """A literal two components use is listed under both and charged to neither."""
    analyzer = _new_analyzer(obj_dir, {COVER_JSON, DUMP_JSON})
    with (
        patch.object(
            analyzer,
            "_scan_object_strings",
            side_effect=[{b"OFF": {COVER_JSON}}, {b"OFF": {DUMP_JSON}}],
        ),
        patch("esphome.analyze_memory.run_tool", return_value=completed(ELF_RODATA)),
    ):
        analyzer._attribute_string_literals(
            _sections(str(obj_dir / WEB_OBJ), str(obj_dir / LIGHT_OBJ)), obj_dir
        )
    (literal,) = analyzer._string_literals
    assert literal.users == {
        "[esphome]web_server": {COVER_JSON},
        "[esphome]light": {DUMP_JSON},
    }
    assert analyzer.sections[".data"].symbol_size == 0
    analyzer._demangle_cache = {COVER_JSON: "cover_json_()", DUMP_JSON: "dump_json()"}
    report = analyzer.generate_report()
    # Each component's row names its own user
    assert re.search(r'"OFF" +cover_json_\(\)', report)
    assert re.search(r'"OFF" +dump_json\(\)', report)


def test_report_groups_by_component() -> None:
    """Strings are listed by component with the function that uses them."""
    analyzer = make_analyzer(cls=MemoryAnalyzerCLI)
    analyzer._demangle_cache = {
        "setup": "setup",
        COVER_JSON: "WebServer::cover_json_()",
    }
    analyzer._string_literals = [
        StringLiteral("hunter2", 8, {"[esphome]core": {"setup"}}),
        StringLiteral("CLOSED", 7, {"[esphome]web_server": {COVER_JSON}}),
    ]
    report = analyzer.generate_report()
    assert "String Literals in RAM (.rodata)" in report
    assert "[esphome]core (8 B, 1 strings):" in report
    assert '"hunter2"' in report
    assert '"CLOSED"' in report
    assert "WebServer::cover_json_()" in report


def test_string_literals_through_unnamed_table() -> None:
    """Literals reached through an unnamed pointer table go to the table's users."""
    analyzer = make_analyzer()
    relocations = """\
Relocation section '.rela.text.setup' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Relocation section '.rela.rodata' at offset 0xe6fc contains 2 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0
00000004  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0

""" + hex_dump(".rodata.str1.1", b"0.pool.ntp.org\0001.pool.ntp.org\0")
    table = hex_dump(".rodata", (0).to_bytes(4, "little") + (15).to_bytes(4, "little"))

    def run_tool(cmd: list[str]) -> subprocess.CompletedProcess[str]:
        if "-rW" in cmd:
            return completed(relocations)
        assert cmd[1:-1] == ["-x", ".rodata"]
        return completed(table)

    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        users = analyzer._scan_object_strings(
            "main.cpp.o", {"5": (".rodata.str1.1", None)}
        )
    assert users == {b"0.pool.ntp.org": {"setup"}, b"1.pool.ntp.org": {"setup"}}


@pytest.mark.parametrize(
    ("symbols", "expected"),
    [
        # The light object's weak copy is replaced by the web object's strong one
        ([("light", "WEAK"), ("web", "GLOBAL")], {"[esphome]web_server"}),
        # Same-named local functions: neither object can be told to be the linked one
        ([("light", "LOCAL"), ("web", "LOCAL")], set()),
    ],
)
def test_string_literals_skip_unlinked_copies(
    obj_dir: Path, symbols: list[tuple[str, str]], expected: set[str]
) -> None:
    """A user whose linked copy may be another object's does not make a literal shared."""
    analyzer = _new_analyzer(obj_dir, {"_Z3foov"})
    paths = {"web": str(obj_dir / WEB_OBJ), "light": str(obj_dir / LIGHT_OBJ)}
    output = _sections(paths["web"], paths["light"]) + "".join(
        f"File: {paths[obj]}\n   1: 00000000    64 FUNC    {bind:<6} DEFAULT   12 _Z3foov\n"
        for obj, bind in symbols
    )
    with (
        patch.object(
            analyzer,
            "_scan_object_strings",
            side_effect=lambda *_: {b"OFF": {"_Z3foov"}},
        ),
        patch("esphome.analyze_memory.run_tool", return_value=completed(ELF_RODATA)),
    ):
        analyzer._attribute_string_literals(output, obj_dir)
    assert {c for s in analyzer._string_literals for c in s.users} == expected


def test_unnamed_table_and_its_strings_charged_once(tmp_path: Path) -> None:
    """Both passes together: the pointer table and each literal are charged once."""
    obj = tmp_path / "obj" / "src" / "main.cpp.o"
    obj.parent.mkdir(parents=True)
    obj.touch()
    analyzer = make_analyzer(cls=MemoryAnalyzerCLI)
    analyzer.elf_path = tmp_path / "firmware.elf"
    analyzer.rodata_in_ram = True
    analyzer.sections[".data"] = MemorySection(".data", total_size=1000)
    analyzer._elf_symbol_names = {"setup"}
    names = b"0.pool.ntp.org\0001.pool.ntp.org\0002.pool.ntp.org\0"
    table = b"".join(offset.to_bytes(4, "little") for offset in (0, 15, 30))
    headers = f"""\
File: {obj}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [  5] .rodata           PROGBITS        00000000 000040 00000c 00   A  0   0  4
  [  6] .rodata.str1.1    PROGBITS        00000000 000050 00002d 01 AMS  0   0  1

Symbol table '.symtab' contains 2 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     1: 00000000    64 FUNC    GLOBAL DEFAULT    2 setup
"""
    relocations = """\
Relocation section '.rela.text.setup' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Relocation section '.rela.rodata' at offset 0x200 contains 3 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0
00000004  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0
00000008  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0

"""

    def run_tool(cmd: list[str]) -> subprocess.CompletedProcess[str]:
        args = cmd[1:-1]
        if "-SW" in args:
            return completed(headers)
        if cmd[-1].endswith("firmware.elf"):
            return completed(hex_dump(".rodata", table + names, 0x3FFE86B0))
        if args == ["-rW", "-x", "5"]:
            return completed(relocations + hex_dump(".rodata", table))
        if args == ["-rW", "-x", "6"]:
            return completed(relocations + hex_dump(".rodata.str1.1", names))
        if args == ["-x", ".rodata.str1.1"]:
            return completed(hex_dump(".rodata.str1.1", names))
        assert args == ["-x", ".rodata"], args
        return completed(hex_dump(".rodata", table))

    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_object_rodata()

    (constant,) = analyzer._unnamed_constants
    assert (constant.size, constant.functions) == (12, ["setup"])
    assert constant.strings == ["0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"]
    assert [(s.text, s.size, dict(s.users)) for s in analyzer._string_literals] == [
        (f"{i}.pool.ntp.org", 15, {"[esphome]core": {"setup"}}) for i in range(3)
    ]
    assert analyzer.components["[esphome]core"].data_size == 12 + 3 * 15
    assert analyzer.sections[".data"].symbol_size == 12 + 3 * 15
