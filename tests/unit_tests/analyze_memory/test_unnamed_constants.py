"""Tests for ESP8266 .rodata accounting and unnamed constant attribution."""

from collections.abc import Callable
from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.analyze_memory import (
    _CODE_SECTION_PATTERN,
    MemoryAnalyzer,
    MemorySection,
    UnnamedConstant,
    _FunctionDefinitions,
    _parse_unnamed_rodata,
    _RodataSection,
)
from esphome.analyze_memory.cli import MemoryAnalyzerCLI

from . import completed, hex_dump, make_analyzer

LIGHT_OBJ = "src/esphome/components/light/light_state.cpp.o"
WEB_OBJ = "src/esphome/components/web_server/web_server.cpp.o"
SETUP = "_ZN7esphome5light10LightState5setupEv"
SAVE = "_ZN7esphome5light10LightState19save_remote_values_Ev"
MATCH_URL = "_ZN7esphome10web_serverL9match_urlEPKcjbb"

# readelf -SW -sW over all objects; the web section index and size vary per test
HEADERS_AND_SYMBOLS = """\
File: {light}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [124] .rodata._ZN7esphome5light10LightStateC2EPNS0_11LightOutputE.str1.1 PROGBITS        00000000 00067d 000001 01 AMS  0   0  1
  [174] .rodata           PROGBITS        00000000 00139c 00002c 00   A  0   0  4
  [221] .rodata.str1.1    PROGBITS        00000000 001f39 000005 01 AMS  0   0  1
  [225] .rodata.BUILTIN_LED PROGBITS        00000000 001f54 000004 00   A  0   0  4

Symbol table '.symtab' contains 348 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND
   300: 00000000    64 FUNC    {bind:<6} DEFAULT  12 {setup}
   301: 00000000    64 FUNC    {bind:<6} DEFAULT  13 {save}
   347: 00000000     4 OBJECT  WEAK   DEFAULT  225 BUILTIN_LED

File: {web}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [{web_index:>3}] .rodata           PROGBITS        00000000 000400 {web_size:06x} 00   A  0   0  4

Symbol table '.symtab' contains 1 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND
{web_symbols}"""

LIGHT_OBJECT_DUMP = f"""\
Relocation section '.rela.text.{SETUP}' at offset 0x7928 contains 2 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0
00000094  00003b14 R_XTENSA_SLOT0_OP      00000000   .text.{SETUP} + 82

Relocation section '.rela.text.{SAVE}' at offset 0x7d28 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Relocation section '.rela.xt.prop' at offset 0x8000 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
000007d4  00003a01 R_XTENSA_32            00000000   .rodata + 0

Hex dump of section '.rodata':
  0x00000000 0000803f 0000803f 0000803f 0000803f ...?...?...?...?
  0x00000010 0000803f 0000803f 0000803f 0000803f ...?...?...?...?
  0x00000020 0000803f 00000000 00000000          ...?........
"""

WEB_OBJECT_DUMP = f"""\
Relocation section '.rela.text.{MATCH_URL}' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Hex dump of section '.rodata':
  0x00000000 00000000 00000000 00000000 00000000 ................
"""


@pytest.fixture
def obj_dir(tmp_path: Path) -> Path:
    """Native ESP8266 build layout: objects under obj/, ELF next to it."""
    for rel in (LIGHT_OBJ, WEB_OBJ, "src/main.cpp.o"):
        path = tmp_path / "obj" / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    return tmp_path / "obj"


def _esp8266_analyzer(
    obj_dir: Path,
    kept: set[str],
    web_index: int = 9,
    web_size: int = 0x1C,
    web_dump: str = WEB_OBJECT_DUMP,
    bind: str = "GLOBAL",
    web_symbols: str = "",
) -> tuple[MemoryAnalyzer, Callable[..., subprocess.CompletedProcess[str]]]:
    analyzer = make_analyzer()
    analyzer.elf_path = obj_dir.parent / "firmware.elf"
    analyzer.rodata_in_ram = True
    analyzer.sections[".data"] = MemorySection(".data", total_size=1000)
    analyzer._elf_symbol_names = kept
    # String literals have their own tests
    analyzer._attribute_string_literals = lambda *_: None
    light = str(obj_dir / LIGHT_OBJ)
    web = str(obj_dir / WEB_OBJ)

    def run_tool(cmd: list[str]) -> subprocess.CompletedProcess[str]:
        if "-SW" in cmd:
            return completed(
                HEADERS_AND_SYMBOLS.format(
                    light=light,
                    web=web,
                    web_index=web_index,
                    web_size=web_size,
                    bind=bind,
                    setup=SETUP,
                    save=SAVE,
                    web_symbols=web_symbols,
                )
            )
        # Only sections with uncovered bytes are dumped
        if cmd[-1] == light:
            assert cmd[1:-1] == ["-rW", "-x", "174"]
            return completed(LIGHT_OBJECT_DUMP)
        return completed(web_dump)

    return analyzer, run_tool


def test_find_object_files_dir_native_layout(obj_dir: Path) -> None:
    """The native ESP8266 build keeps objects under obj/."""
    analyzer = make_analyzer()
    analyzer.elf_path = obj_dir.parent / "firmware.elf"
    assert analyzer._find_object_files_dir() == obj_dir


def test_find_object_files_dir_platformio_layout(tmp_path: Path) -> None:
    """PlatformIO keeps objects next to the ELF."""
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "main.cpp.o").touch()
    analyzer = make_analyzer()
    analyzer.elf_path = tmp_path / "firmware.elf"
    assert analyzer._find_object_files_dir() == tmp_path


def _parse_sections(code_section: str) -> MemoryAnalyzer:
    readelf = f"""\
  [ 1] .data             PROGBITS        3ffe8000 001000 000668 00 WAX  0   0 16
  [ 4] {code_section}       PROGBITS        40201010 00b010 06612c 00 WAX  0   0 16
  [ 6] .rodata           PROGBITS        3ffe86b0 0016b0 000b90 00   A  0   0 16
"""
    analyzer = make_analyzer()
    with patch(
        "esphome.analyze_memory.subprocess.run", return_value=completed(readelf)
    ):
        analyzer._parse_sections()
    return analyzer


def test_parse_sections_esp8266_counts_rodata_as_data() -> None:
    """ESP8266 (.irom0.text) copies .rodata to RAM, so it is counted as .data."""
    analyzer = _parse_sections(".irom0.text")
    assert analyzer.rodata_in_ram
    assert ".rodata" not in analyzer.sections
    assert analyzer.sections[".data"].total_size == 0x668 + 0xB90


def test_parse_sections_other_platforms_keep_rodata() -> None:
    """Other platforms keep .rodata in flash."""
    analyzer = _parse_sections(".flash.text")
    assert not analyzer.rodata_in_ram
    assert analyzer.sections[".rodata"].total_size == 0xB90
    assert analyzer.sections[".data"].total_size == 0x668


def test_unnamed_constants_attributed_to_component(obj_dir: Path) -> None:
    """Unnamed .rodata in a kept function is listed and counted as the component's RAM."""
    analyzer, run_tool = _esp8266_analyzer(obj_dir, {SETUP, MATCH_URL})
    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_object_rodata()

    light, web = analyzer._unnamed_constants
    assert light == UnnamedConstant(
        size=44,
        source_file=LIGHT_OBJ,
        component="[esphome]light",
        functions=[SAVE, SETUP],
        jump_table=False,
        data=bytes.fromhex("0000803f") * 4,
    )
    assert (web.size, web.component, web.functions) == (
        28,
        "[esphome]web_server",
        [MATCH_URL],
    )
    assert analyzer.components["[esphome]light"].data_size == 44
    assert analyzer.components["[esphome]web_server"].ram_total == 28
    # Attributed bytes no longer count as unattributed RAM
    assert analyzer.sections[".data"].symbol_size == 72


def test_unnamed_constants_skip_garbage_collected(obj_dir: Path) -> None:
    """A constant whose functions the linker dropped is not in the firmware."""
    analyzer, run_tool = _esp8266_analyzer(obj_dir, {MATCH_URL})
    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_object_rodata()
    assert [c.component for c in analyzer._unnamed_constants] == ["[esphome]web_server"]


def test_unnamed_constants_skipped_when_rodata_in_flash(obj_dir: Path) -> None:
    """Other platforms keep .rodata in flash, so there is nothing to scan."""
    analyzer, run_tool = _esp8266_analyzer(obj_dir, {SETUP, MATCH_URL})
    analyzer.rodata_in_ram = False
    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool) as mock:
        analyzer._analyze_object_rodata()
    mock.assert_not_called()
    assert analyzer._unnamed_constants == []


@pytest.mark.parametrize(("bind", "expected"), [("WEAK", 1), ("GLOBAL", 2)])
def test_unnamed_constants_inline_copies_counted_once(
    obj_dir: Path, bind: str, expected: int
) -> None:
    """Only weak (inline) functions bring the same constant from several objects."""
    # The second object emits the same functions, with the same binding
    analyzer, run_tool = _esp8266_analyzer(
        obj_dir,
        {SETUP},
        web_index=174,
        web_size=0x2C,
        web_dump=LIGHT_OBJECT_DUMP,
        bind=bind,
        web_symbols=(
            f"   300: 00000000    64 FUNC    {bind:<6} DEFAULT  12 {SETUP}\n"
            f"   301: 00000000    64 FUNC    {bind:<6} DEFAULT  13 {SAVE}\n"
        ),
    )
    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_object_rodata()
    assert len(analyzer._unnamed_constants) == expected
    assert analyzer.sections[".data"].symbol_size == 44 * expected


def test_parse_symbols_keeps_alias_names() -> None:
    """C1/C2 constructor aliases share an address; both names are in the ELF."""
    objdump = """4020b558 g     F .irom0.text\t0000007e _ZN7esphome3FooC1Ev
4020b558 g     F .irom0.text\t0000007e _ZN7esphome3FooC2Ev
"""
    analyzer = make_analyzer()
    analyzer.sections[".text"] = MemorySection(".text")
    with patch(
        "esphome.analyze_memory.subprocess.run", return_value=completed(objdump)
    ):
        analyzer._parse_symbols()
    assert {"_ZN7esphome3FooC1Ev", "_ZN7esphome3FooC2Ev"} <= analyzer._elf_symbol_names
    assert analyzer.sections[".text"].symbol_size == 0x7E


def test_unnamed_constants_skip_named_bytes() -> None:
    """In a section shared with a named object, only the unnamed bytes are shown."""
    analyzer = make_analyzer()
    analyzer._elf_symbol_names = {"_Z3fooi"}
    dump = """Relocation section '.rela.text._Z3fooi' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 4

Hex dump of section '.rodata':
  0x00000000 aaaaaaaa 01020304                   ........
"""
    section = _RodataSection(".rodata", 8, covered=[(0, 4)])
    with patch("esphome.analyze_memory.run_tool", return_value=completed(dump)):
        (constant,) = analyzer._scan_object_constants(
            "foo.o", {"5": section}, "src/foo.o", "[esphome]foo"
        )
    assert constant.size == 4
    assert constant.data == bytes([1, 2, 3, 4])


def test_unnamed_constants_jump_table() -> None:
    """A .rodata section that points into code is a switch jump table."""
    analyzer = make_analyzer()
    analyzer._elf_symbol_names = {"_Z3fooi"}
    dump = """\
Relocation section '.rela.text._Z3fooi' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Relocation section '.rela.rodata' at offset 0x200 contains 2 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003b01 R_XTENSA_32            00000000   .text._Z3fooi + 12
00000004  00003b01 R_XTENSA_32            00000000   .text._Z3fooi + 20
"""
    with patch("esphome.analyze_memory.run_tool", return_value=completed(dump)):
        (constant,) = analyzer._scan_object_constants(
            "foo.o", {"5": _RodataSection(".rodata", 8)}, "src/foo.o", "[esphome]foo"
        )
    assert constant.jump_table
    assert constant.functions == ["_Z3fooi"]


def test_report_lists_unnamed_constants() -> None:
    """The report names the component, the object, the functions and the bytes."""
    analyzer = make_analyzer(cls=MemoryAnalyzerCLI)
    analyzer.rodata_in_ram = True
    analyzer._demangle_cache = {SETUP: "esphome::light::LightState::setup()"}
    analyzer._unnamed_constants = [
        UnnamedConstant(
            size=44,
            source_file=LIGHT_OBJ,
            component="[esphome]light",
            functions=[SETUP],
            jump_table=False,
            data=bytes.fromhex("0000803f") * 4,
        )
    ]
    report = analyzer.generate_report()
    assert "ESP8266 keeps .rodata in RAM" in report
    assert "Unnamed Constants in RAM (.rodata)" in report
    assert f"44 B constant     [esphome]light ({LIGHT_OBJ})" in report
    assert "used by esphome::light::LightState::setup()" in report
    assert "bytes: 00 00 80 3f 00 00 80 3f 00 00 80 3f 00 00 80 3f ..." in report


def test_unnamed_constants_resolve_string_pointers() -> None:
    """A constant of pointers to literals lists the strings, so it can be recognized."""
    analyzer = make_analyzer()
    analyzer._elf_symbol_names = {"setup"}
    relocations = """\
Relocation section '.rela.text.setup' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Relocation section '.rela.rodata' at offset 0xe6fc contains 3 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0
00000004  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0
00000008  00000501 R_XTENSA_32            00000000   .rodata.str1.1 + 0

Hex dump of section '.rodata':
  0x00000000 00000000 0f000000 1e000000          ............
"""
    literals = hex_dump(
        ".rodata.str1.1", b"0.pool.ntp.org\0001.pool.ntp.org\0002.pool.ntp.org\0"
    )

    def run_tool(cmd: list[str]) -> subprocess.CompletedProcess[str]:
        if "-rW" not in cmd:
            assert cmd[1:-1] == ["-x", ".rodata.str1.1"]
            return completed(literals)
        return completed(relocations)

    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        (constant,) = analyzer._scan_object_constants(
            "main.cpp.o",
            {"5": _RodataSection(".rodata", 12)},
            "src/main.cpp.o",
            "[esphome]core",
        )
    assert constant.strings == ["0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"]

    cli = make_analyzer(cls=MemoryAnalyzerCLI)
    cli._unnamed_constants = [constant]
    assert (
        'points to "0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"'
        in cli.generate_report()
    )


@pytest.mark.parametrize(
    ("section", "function"),
    [
        (".text._Z3foov", "_Z3foov"),
        (".literal._Z3foov", "_Z3foov"),
        (".irom0.text._Z3foov", "_Z3foov"),
        (".text.startup._GLOBAL__sub_I_setup", "_GLOBAL__sub_I_setup"),
        (".text.unlikely._Z3foov", "_Z3foov"),
        (".iram.text._Z3foov", "_Z3foov"),
        (".text.hot_path", "hot_path"),
        (".text", None),
    ],
)
def test_code_section_function(section: str, function: str | None) -> None:
    """Placement prefixes and IRAM code still resolve to the function."""
    match = _CODE_SECTION_PATTERN.match(section)
    assert match is not None
    assert match.group(1) == function


def test_unnamed_constants_same_object_not_merged(obj_dir: Path) -> None:
    """Two equal constants of one inline function in one object are both counted."""
    analyzer, run_tool = _esp8266_analyzer(obj_dir, {SETUP}, bind="WEAK")
    constant = UnnamedConstant(
        size=8,
        source_file=LIGHT_OBJ,
        component="[esphome]light",
        functions=[SAVE, SETUP],
        jump_table=False,
        data=b"",
    )
    light = str(obj_dir / LIGHT_OBJ)
    with (
        patch("esphome.analyze_memory.run_tool", side_effect=run_tool),
        patch.object(
            analyzer,
            "_scan_object_constants",
            side_effect=lambda path, *_: [constant, constant] if path == light else [],
        ),
    ):
        analyzer._analyze_object_rodata()
    assert len(analyzer._unnamed_constants) == 2


def test_unnamed_constants_jump_table_only_from_unnamed_bytes() -> None:
    """Code pointers in a named table at the start do not make the rest a jump table."""
    analyzer = make_analyzer()
    analyzer._elf_symbol_names = {"_Z3fooi"}
    dump = """\
Relocation section '.rela.text._Z3fooi' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 4

Relocation section '.rela.rodata' at offset 0x200 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003b01 R_XTENSA_32            00000000   .text._Z3fooi + 12

"""
    section = _RodataSection(".rodata", 8, covered=[(0, 4)])
    with patch("esphome.analyze_memory.run_tool", return_value=completed(dump)):
        (constant,) = analyzer._scan_object_constants(
            "foo.o", {"5": section}, "src/foo.o", "[esphome]foo"
        )
    assert not constant.jump_table


def _definitions(rows: dict[str, list[tuple[str, str]]]) -> _FunctionDefinitions:
    """Parse symbol rows (binding, function) per object, as readelf -sW prints them."""
    output = "".join(
        f"File: {path}\n"
        + "".join(
            f"   {i}: 00000000    64 FUNC    {bind:<6} DEFAULT   12 {name}\n"
            for i, (bind, name) in enumerate(symbols)
        )
        for path, symbols in rows.items()
    )
    return _parse_unnamed_rodata(output)[1]


def test_linked_copy_unknown_weak_overridden_by_strong() -> None:
    """A strong definition replaces every weak copy, so their constants are not linked."""
    definitions = _definitions(
        {"a.o": [("WEAK", "_Z3foov")], "b.o": [("GLOBAL", "_Z3foov")]}
    )
    assert definitions.linked_copy_unknown("a.o", ["_Z3foov"])
    assert not definitions.linked_copy_unknown("b.o", ["_Z3foov"])


def test_linked_copy_unknown_same_named_locals() -> None:
    """Local functions with one name in two objects can't be told apart in the ELF."""
    definitions = _definitions(
        {
            "a.o": [("LOCAL", "_ZL3barv"), ("LOCAL", "_ZL3bazv")],
            "b.o": [("LOCAL", "_ZL3barv")],
        }
    )
    assert definitions.linked_copy_unknown("a.o", ["_ZL3barv"])
    # Another, unique user decides it
    assert not definitions.linked_copy_unknown("a.o", ["_ZL3barv", "_ZL3bazv"])
