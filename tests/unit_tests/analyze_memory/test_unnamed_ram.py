"""Tests for charging RAM without symbols from the linker map."""

import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.analyze_memory import (
    MapInputSection,
    MemoryAnalyzer,
    MemorySection,
    _parse_map_input_sections,
)
from esphome.analyze_memory.cli import MemoryAnalyzerCLI

from . import completed, make_analyzer

SDK = "/sdk/lib/NONOSDK22x_190703"

# Trimmed from an ESP8266 firmware.map
MAP_TEXT = f"""\
Discarded input sections

 .text          0x0000000000000000        0x0 {SDK}/libpp.a(gone.o)

Linker script and memory map

LOAD {SDK}/libpp.a

.data           0x000000003ffe8000       0x30
 *(.data)
 .data          0x000000003ffe8000       0x20 {SDK}/libpp.a(trc.o)
                0x000000003ffe8000                rc11NSchedTbl
 *fill*         0x000000003ffe8020        0x8
 .data.some_long_section_name
                0x000000003ffe8028        0x8 obj/src/main.cpp.o

.rodata         0x000000003ffe8030       0x10
 .rodata        0x000000003ffe8030        0x8 libFrameworkArduino.a(core.cpp.o)
 .rodata        0x000000003ffe8038        0x8 {SDK}/libc.a(lib_a-ctype_.o)

.irom0.text     0x0000000040201010      0x300
 .irom0.text    0x0000000040201010      0x100 {SDK}/libpp.a(wdev.o)
                0x0000000040201010                wDev_ProcessFiq
 .irom0.text    0x0000000040201110      0x100 {SDK}/libpp.a(trc.o)
 .irom0.text    0x0000000040201210      0x100 {SDK}/libpp.a(lmac.o)

.a_very_long_output_name
                0x0000000040300000       0x10
 .bss           0x0000000040300000       0x10 {SDK}/libpp.a(ignored.o)

.bss            0x000000003ffe8040     0x3010
 .bss           0x000000003ffe8040     0x3000 {SDK}/libpp.a(wdev.o)
                0x000000003ffe8040                wDevCtrl
 COMMON         0x000000003ffeb040       0x10 {SDK}/libmain.a(nmi-vector.o)
"""

INPUTS = [
    MapInputSection(".data", 0x3FFE8000, 0x20, f"{SDK}/libpp.a(trc.o)"),
    MapInputSection(".data", 0x3FFE8028, 0x8, "obj/src/main.cpp.o"),
    MapInputSection(".rodata", 0x3FFE8030, 0x8, "libFrameworkArduino.a(core.cpp.o)"),
    MapInputSection(".rodata", 0x3FFE8038, 0x8, f"{SDK}/libc.a(lib_a-ctype_.o)"),
    MapInputSection(".irom0.text", 0x40201010, 0x100, f"{SDK}/libpp.a(wdev.o)"),
    MapInputSection(".irom0.text", 0x40201110, 0x100, f"{SDK}/libpp.a(trc.o)"),
    MapInputSection(".irom0.text", 0x40201210, 0x100, f"{SDK}/libpp.a(lmac.o)"),
    MapInputSection(
        ".a_very_long_output_name", 0x40300000, 0x10, f"{SDK}/libpp.a(ignored.o)"
    ),
    MapInputSection(".bss", 0x3FFE8040, 0x3000, f"{SDK}/libpp.a(wdev.o)"),
    MapInputSection(".bss", 0x3FFEB040, 0x10, f"{SDK}/libmain.a(nmi-vector.o)"),
]

# (address, size, name, component)
SYMBOLS = [
    (0x3FFE8000, 0x20, "rc11NSchedTbl", "rate_control"),
    (0x3FFE8040, 0x40, "wDevCtrl", "wifi_stack"),
    (0x40201010, 0x80, "wDev_ProcessFiq", "wifi_stack"),
    (0x40201090, 0x10, "wdev_trace", "wifi_phy_pp"),
    (0x40201110, 0x100, "rcUpdateTxDone", "rate_control"),
]


@pytest.mark.parametrize(
    ("text", "expected"), [(MAP_TEXT, INPUTS), ("not a map\n", [])]
)
def test_parse_map_input_sections(text: str, expected: list[MapInputSection]) -> None:
    """Input sections come from the memory map only, with wrapped names, without padding."""
    assert _parse_map_input_sections(text) == expected


def _analyzer[T: MemoryAnalyzer](
    *, rodata_in_ram: bool, cls: type[T] = MemoryAnalyzer
) -> T:
    analyzer = make_analyzer(cls=cls)
    analyzer.rodata_in_ram = rodata_in_ram
    analyzer.__dict__["_map_text"] = MAP_TEXT
    for section in (".data", ".bss"):
        analyzer.sections[section] = MemorySection(section)
    analyzer._symbol_spans = [(a, s, n) for a, s, n, _ in SYMBOLS]
    analyzer._symbol_components = {n: c for _, _, n, c in SYMBOLS}
    return analyzer


def test_attribute_unnamed_ram() -> None:
    """Bytes no symbol covers go to the component most of the object's symbols name."""
    analyzer = _analyzer(rodata_in_ram=False)
    analyzer._attribute_unnamed_ram()

    # trc.o's .data is all named; .rodata is flash here
    assert analyzer.get_unnamed_ram() == [
        ("libpp.a(wdev.o)", ".bss", "wifi_stack", 0x3000 - 0x40),
        ("libmain.a(nmi-vector.o)", ".bss", "[lib]main", 0x10),
        ("main.cpp.o", ".data", "[esphome]core", 8),
    ]
    assert analyzer.components["wifi_stack"].bss_size == 0x3000 - 0x40
    assert analyzer.sections[".bss"].symbol_size == 0x3000 - 0x40 + 0x10
    assert analyzer.sections[".data"].symbol_size == 8
    assert (
        "libpp.a(wdev.o)",
        "libpp.a(wdev.o) (no symbol)",
        0x3000 - 0x40,
        ".bss",
    ) in analyzer._ram_symbols["wifi_stack"]


@pytest.mark.parametrize("scanned", [True, False])
def test_attribute_unnamed_ram_rodata_in_ram(scanned: bool) -> None:
    """ESP8266 .rodata is RAM; objects the .rodata pass read were already charged."""
    analyzer = _analyzer(rodata_in_ram=True)
    if scanned:
        build_dir = analyzer.elf_path.parent.resolve()
        analyzer._rodata_scanned_objects = {build_dir / "obj" / "core" / "core.cpp.o"}
    analyzer._attribute_unnamed_ram()
    unnamed = analyzer.get_unnamed_ram()
    assert ("libc.a(lib_a-ctype_.o)", ".data", "[lib]c", 8) in unnamed
    framework = (
        "libFrameworkArduino.a(core.cpp.o)",
        ".data",
        "[lib]FrameworkArduino",
        8,
    )
    assert (framework in unnamed) is not scanned


def test_attribute_unnamed_ram_long_symbol_before_short_one() -> None:
    """A long symbol that starts before a shorter one still covers the bytes after it."""
    analyzer = _analyzer(rodata_in_ram=False)
    analyzer.__dict__["_map_text"] = f"""\
Linker script and memory map

.bss            0x000000003ffe8000      0x100
 .bss           0x000000003ffe8000      0x100 {SDK}/libpp.a(wdev.o)
 .bss           0x000000003ffe8100       0x10 {SDK}/libpp.a(pp.o)
"""
    analyzer._symbol_spans = [(0x3FFE8000, 0x100, "big"), (0x3FFE8010, 0x4, "small")]
    analyzer._symbol_components = {"big": "wifi_stack", "small": "wifi_stack"}
    analyzer._attribute_unnamed_ram()
    assert analyzer.get_unnamed_ram() == [("libpp.a(pp.o)", ".bss", "[lib]pp", 0x10)]


def test_map_text_ignores_stale_map(tmp_path: Path) -> None:
    """A map much older than the ELF is from an earlier link."""
    elf = tmp_path / "firmware.elf"
    elf.write_bytes(b"")
    map_path = tmp_path / "firmware.map"
    map_path.write_text(MAP_TEXT * 100)
    os.utime(map_path, (elf.stat().st_mtime - 3600,) * 2)
    analyzer = make_analyzer()
    analyzer.elf_path = elf
    assert analyzer._map_text is None


def test_attribute_unnamed_ram_unparsable_map(caplog: pytest.LogCaptureFixture) -> None:
    """A map without a GNU ld memory map is reported and not used."""
    analyzer = _analyzer(rodata_in_ram=False)
    analyzer.__dict__["_map_text"] = "not a map\n"
    assert not analyzer._attribute_unnamed_ram()
    assert "No input sections in the linker map" in caplog.text


def test_attribute_unnamed_ram_without_map() -> None:
    """Without a map nothing is charged."""
    analyzer = _analyzer(rodata_in_ram=True)
    analyzer.__dict__["_map_text"] = None
    analyzer._attribute_unnamed_ram()
    assert analyzer.get_unnamed_ram() == []
    assert analyzer.sections[".bss"].symbol_size == 0


def test_report_lists_unnamed_ram() -> None:
    """The report lists the objects and the components they were charged to."""
    analyzer = _analyzer(rodata_in_ram=False, cls=MemoryAnalyzerCLI)
    analyzer._attribute_unnamed_ram()
    report = analyzer.generate_report()
    assert "RAM without symbols, charged by the linker map: 12,248 B" in report
    assert "12,224 B [bss] libpp.a(wdev.o) -> wifi_stack" in report
    assert "SDK library breakdown" not in report


def test_attribute_unnamed_ram_merged_sections_count_once() -> None:
    """Merged string sections the map lists at one address are charged once."""
    analyzer = _analyzer(rodata_in_ram=False)
    analyzer.__dict__["_map_text"] = f"""\
Linker script and memory map

.dram0.data     0x000000003fc9ae00       0x40
 .rodata.str1.1
                0x000000003fc9ae00       0x30 {SDK}/libhal.a(cache_hal.c.obj)
 .rodata.str1.1
                0x000000003fc9ae00       0x20 {SDK}/libphy.a(phy_debug.o)
 .dram0.data    0x000000003fc9ae30       0x10 {SDK}/libphy.a(phy_debug.o)
"""
    analyzer._symbol_spans = []
    analyzer._attribute_unnamed_ram()
    assert analyzer.get_unnamed_ram() == [
        ("libhal.a(cache_hal.c.obj)", ".data", "[lib]hal", 0x30),
        ("libphy.a(phy_debug.o)", ".data", "[lib]phy", 0x10),
    ]
    assert analyzer.sections[".data"].symbol_size == 0x40


def test_parse_sections_skips_placeholders() -> None:
    """ESP-IDF sections that only reserve address space are not RAM or flash."""
    readelf = """\
  [ 8] .dram0.dummy      NOBITS          3fc88000 03c000 012e00 00  WA  0   0  1
  [ 9] .dram0.data       PROGBITS        3fc9ae00 04ee00 004de0 00  WA  0   0 16
  [13] .flash_rodata_dummy NOBITS          3c000020 001020 110000 00  WA  0   0  1
  [15] .flash.rodata     PROGBITS        3c110120 001120 03aa20 00  WA  0   0 16
  [16] .flash.rodata_noload NOBITS          3c14ab40 03bb40 006114 00   A  0   0  1
"""
    analyzer = make_analyzer()
    with patch(
        "esphome.analyze_memory.subprocess.run", return_value=completed(readelf)
    ):
        analyzer._parse_sections()
    assert analyzer.sections[".data"].total_size == 0x4DE0
    assert analyzer.sections[".rodata"].total_size == 0x3AA20
