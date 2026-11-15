"""Tests for ESP8266 .rodata accounting and unnamed constant attribution."""

from pathlib import Path
import subprocess
from unittest.mock import MagicMock, patch

import pytest

from esphome.analyze_memory import MemorySection, UnnamedConstant
from esphome.analyze_memory.cli import MemoryAnalyzerCLI

from . import make_analyzer

LIGHT_OBJ = "src/esphome/components/light/light_state.cpp.o"
WEB_OBJ = "src/esphome/components/web_server/web_server.cpp.o"
SETUP = "_ZN7esphome5light10LightState5setupEv"
SAVE = "_ZN7esphome5light10LightState19save_remote_values_Ev"
MATCH_URL = "_ZN7esphome10web_serverL9match_urlEPKcjbb"

SECTION_HEADERS = """\
File: {light}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [124] .rodata._ZN7esphome5light10LightStateC2EPNS0_11LightOutputE.str1.1 PROGBITS        00000000 00067d 000001 01 AMS  0   0  1
  [174] .rodata           PROGBITS        00000000 00139c 00002c 00   A  0   0  4
  [221] .rodata.str1.1    PROGBITS        00000000 001f39 000005 01 AMS  0   0  1
  [225] .rodata.BUILTIN_LED PROGBITS        00000000 001f54 000004 00   A  0   0  4

File: {web}

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 9] .rodata           PROGBITS        00000000 000400 00001c 00   A  0   0  4
"""

LIGHT_OBJECT_DUMP = f"""\
Relocation section '.rela.text.{SETUP}' at offset 0x7928 contains 2 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0
00000094  00003b14 R_XTENSA_SLOT0_OP      00000000   .text.{SETUP} + 82

Relocation section '.rela.text.{SAVE}' at offset 0x7d28 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Relocation section '.rela.xt.prop' at offset 0x8000 contains 2 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
000007d4  00003a01 R_XTENSA_32            00000000   .rodata + 0
00000b1c  00005701 R_XTENSA_32            00000000   .rodata.BUILTIN_LED + 0

Symbol table '.symtab' contains 348 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND
   347: 00000000     4 OBJECT  WEAK   DEFAULT  225 BUILTIN_LED

Hex dump of section '.rodata':
  0x00000000 0000803f 0000803f 0000803f 0000803f ...?...?...?...?
  0x00000010 0000803f 0000803f 0000803f 0000803f ...?...?...?...?
  0x00000020 0000803f 00000000 00000000          ...?........

Hex dump of section '.rodata.BUILTIN_LED':
  0x00000000 02000000                            ....
"""

WEB_OBJECT_DUMP = f"""\
Relocation section '.rela.text.{MATCH_URL}' at offset 0x100 contains 1 entry:
 Offset     Info    Type                Sym. Value  Symbol's Name + Addend
00000000  00003a01 R_XTENSA_32            00000000   .rodata + 0

Symbol table '.symtab' contains 1 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND

Hex dump of section '.rodata':
  0x00000000 00000000 00000000 00000000 00000000 ................
"""


def _completed(stdout: str) -> MagicMock:
    return MagicMock(returncode=0, stdout=stdout)


@pytest.fixture
def obj_dir(tmp_path: Path) -> Path:
    """Native ESP8266 build layout: objects under obj/, ELF next to it."""
    for rel in (LIGHT_OBJ, WEB_OBJ, "src/main.cpp.o"):
        path = tmp_path / "obj" / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    return tmp_path / "obj"


def _esp8266_analyzer(obj_dir: Path, kept: set[str]):
    analyzer = make_analyzer()
    analyzer.elf_path = obj_dir.parent / "firmware.elf"
    analyzer.rodata_in_ram = True
    analyzer.sections[".data"] = MemorySection(".data", total_size=1000)
    analyzer._elf_symbol_names = kept
    light = str(obj_dir / LIGHT_OBJ)
    web = str(obj_dir / WEB_OBJ)

    def run_tool(cmd: list[str], timeout: int = 30) -> MagicMock:
        if "-SW" in cmd:
            return _completed(SECTION_HEADERS.format(light=light, web=web))
        if cmd[-1] == light:
            assert cmd[cmd.index("-x") + 1] == "174"
            return _completed(LIGHT_OBJECT_DUMP)
        return _completed(WEB_OBJECT_DUMP)

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


@pytest.mark.parametrize(
    ("irom_section", "expected_rodata_section"),
    [(".irom0.text", ".data"), (".flash.text", ".rodata")],
)
def test_parse_sections_rodata_in_ram_on_esp8266(
    irom_section: str, expected_rodata_section: str
) -> None:
    """ESP8266 (.irom0.text) counts .rodata as RAM data; other platforms keep it as flash."""
    readelf = f"""\
  [ 1] .data             PROGBITS        3ffe8000 001000 000668 00 WAX  0   0 16
  [ 4] {irom_section}       PROGBITS        40201010 00b010 06612c 00 WAX  0   0 16
  [ 6] .rodata           PROGBITS        3ffe86b0 0016b0 000b90 00   A  0   0 16
"""
    analyzer = make_analyzer()
    with patch(
        "esphome.analyze_memory.subprocess.run",
        return_value=subprocess.CompletedProcess([], 0, stdout=readelf),
    ):
        analyzer._parse_sections()
    assert analyzer.rodata_in_ram is (irom_section == ".irom0.text")
    assert analyzer.sections[expected_rodata_section].total_size >= 0xB90
    if analyzer.rodata_in_ram:
        assert ".rodata" not in analyzer.sections
        assert analyzer.sections[".data"].total_size == 0x668 + 0xB90


def test_unnamed_constants_attributed_to_component(obj_dir: Path) -> None:
    """Unnamed .rodata in a kept function is listed and counted as the component's RAM."""
    analyzer, run_tool = _esp8266_analyzer(obj_dir, {SETUP, MATCH_URL})
    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_unnamed_constants()

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
        analyzer._analyze_unnamed_constants()
    assert [c.component for c in analyzer._unnamed_constants] == ["[esphome]web_server"]


def test_unnamed_constants_skipped_when_rodata_in_flash(obj_dir: Path) -> None:
    """Other platforms keep .rodata in flash, so there is nothing to scan."""
    analyzer, run_tool = _esp8266_analyzer(obj_dir, {SETUP, MATCH_URL})
    analyzer.rodata_in_ram = False
    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool) as mock:
        analyzer._analyze_unnamed_constants()
    mock.assert_not_called()
    assert analyzer._unnamed_constants == []


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
    with patch("esphome.analyze_memory.run_tool", return_value=_completed(dump)):
        (constant,) = analyzer._scan_object_constants("foo.o", {"5": (".rodata", 8)})
    assert constant.jump_table
    assert constant.functions == ["_Z3fooi"]


def test_unnamed_constants_inline_copies_counted_once(obj_dir: Path) -> None:
    """An inline function emitted in two objects brings the same constant twice."""
    analyzer, _ = _esp8266_analyzer(obj_dir, {SETUP})
    light = str(obj_dir / LIGHT_OBJ)
    web = str(obj_dir / WEB_OBJ)
    headers = SECTION_HEADERS.format(light=light, web=web).replace(
        "[ 9] .rodata           PROGBITS        00000000 000400 00001c",
        "[174] .rodata           PROGBITS        00000000 000400 00002c",
    )

    def run_tool(cmd: list[str], timeout: int = 30) -> MagicMock:
        return _completed(headers if "-SW" in cmd else LIGHT_OBJECT_DUMP)

    with patch("esphome.analyze_memory.run_tool", side_effect=run_tool):
        analyzer._analyze_unnamed_constants()
    assert len(analyzer._unnamed_constants) == 1
    assert analyzer.sections[".data"].symbol_size == 44


def test_report_lists_unnamed_constants() -> None:
    """The report names the component, the object, the functions and the bytes."""
    with (
        patch("esphome.analyze_memory.resolve_tool_path", return_value=None),
        patch("esphome.analyze_memory.find_tool", return_value="nm"),
    ):
        analyzer = MemoryAnalyzerCLI(__file__)
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
