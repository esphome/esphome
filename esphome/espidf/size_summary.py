"""PlatformIO-format RAM/Flash one-liners after a native ESP-IDF build.

``idf.py size`` (chained onto ``idf.py build`` in
``toolchain.run_compile``) prints the per-region table inline as part
of the build. This module adds two summary lines underneath,
byte-identical to PlatformIO's output:

    RAM:   [====      ]  26.5% (used 47932 bytes from 180736 bytes)
    Flash: [===       ]  48.4% (used 888511 bytes from 1835008 bytes)

The format matches ``script/ci_memory_impact_extract.py`` so CI memory
analysis works unchanged on native ESP-IDF builds. RAM usage comes from
the DRAM (or unified DIRAM) region of the linker map. Flash used is the
exact image size matching the ``Total image size`` line: the json2
``total_size`` field when present (esp-idf-size >= 2.1); older 1.x
json2 lacks it, so we derive the same figure from the ELF by summing
loadable PROGBITS sections, the rule esp_idf_size itself uses.
Flash total is taken from
``partitions.csv`` using PlatformIO's rule (first app partition whose
subtype is ``factory`` or ``ota_0``; see
``platform-espressif32/builder/main.py::_update_max_upload_size``).

Structured size data is produced at link time by a CMake POST_BUILD
custom command (see ``build_gen/espidf.py``) which writes
``esp_idf_size.json`` (``--format=json2``: a per-memory-type summary,
``{"version": ..., "layout": [{"name", "total", "used", ...}]}``) next
to the ELF. We read that file here rather than re-running
``esp_idf_size`` from Python.
"""

from __future__ import annotations

import csv
import json
import logging
from pathlib import Path
import struct

from esphome.build_helpers.size_summary import print_size_line

_LOGGER = logging.getLogger(__name__)
_SIZE_SUFFIXES = {"K": 1024, "M": 1024 * 1024}


def _parse_size(token: str) -> int:
    token = token.strip()
    if not token:
        return 0
    if token.startswith(("0x", "0X")):
        return int(token, 16)
    suffix = token[-1].upper()
    if suffix in _SIZE_SUFFIXES:
        return int(token[:-1]) * _SIZE_SUFFIXES[suffix]
    return int(token)


def _find_app_partition_size(partitions_csv: Path) -> int:
    """Return the size of the firmware's app partition.

    Mirrors PlatformIO's ``platform-espressif32/builder/main.py::
    _update_max_upload_size``: take the first ``app``-type partition
    whose subtype is ``factory`` or ``ota_0``. Order matters because
    layouts like Adafruit's ``partitions-4MB-tinyuf2.csv`` repurpose
    ``factory`` for a UF2 bootloader before the real OTA slot, so a
    naive "prefer factory" rule would pick the wrong row. Raises
    ``ValueError`` if no qualifying partition is present.
    """
    if not partitions_csv.is_file():
        raise ValueError(f"partitions.csv not found at {partitions_csv}")
    for row in csv.reader(partitions_csv.read_text(encoding="utf-8").splitlines()):
        cells = [c.strip() for c in row]
        if not cells or cells[0].startswith("#") or len(cells) < 5:
            continue
        ptype, psubtype, psize = cells[1], cells[2], cells[4]
        if ptype in ("app", "0") and psubtype in ("factory", "ota_0"):
            return _parse_size(psize)
    raise ValueError(f"No app+factory or app+ota_0 partition in {partitions_csv}")


def _image_size_from_elf(elf: Path) -> int:
    """Sum the loadable PROGBITS section sizes from an ELF32 file.

    This is the rule ``esp_idf_size.ng.memorymap._get_image_size`` uses for
    its image size figure, so the result is byte-identical to the tool's.
    Raises ``ValueError`` if the file is not a 32-bit little-endian ELF.
    """
    data = elf.read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError(f"{elf} is not a 32-bit little-endian ELF")
    (e_shoff,) = struct.unpack_from("<I", data, 0x20)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x2E)
    total = 0
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh_type, sh_flags = struct.unpack_from("<II", data, off + 4)
        (sh_size,) = struct.unpack_from("<I", data, off + 20)
        # SHT_PROGBITS with SHF_ALLOC: sections with data that occupy memory
        if sh_size and sh_type == 1 and sh_flags & 0x2:
            total += sh_size
    return total


def print_summary(size_json: Path, partitions_csv: Path, firmware_elf: Path) -> None:
    """Print PlatformIO-shaped RAM and Flash one-liners.

    Failures are non-fatal: the build has already succeeded, we just couldn't
    summarize. Logs the cause at debug level.
    """
    if not size_json.is_file():
        _LOGGER.debug("Skipping size summary: %s not found", size_json)
        return
    try:
        data = json.loads(size_json.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        _LOGGER.debug("Skipping size summary: %s", e)
        return

    regions = {
        entry.get("name"): entry
        for entry in data.get("layout", [])
        if isinstance(entry, dict)
    }
    ram_region = regions.get("DRAM") or regions.get("DIRAM") or {}
    ram_used = ram_region.get("used")
    ram_total = ram_region.get("total")
    if ram_total and ram_used is not None:
        print_size_line("RAM", ram_used, ram_total)

    # esp-idf-size >= 2.1 (IDF >= 6.0) reports the exact image size in
    # json2; older 1.x omits it, so derive the same figure from the ELF.
    flash_used = data.get("total_size")
    try:
        if flash_used is None:
            flash_used = _image_size_from_elf(firmware_elf)
        app_size = _find_app_partition_size(partitions_csv)
    except (OSError, ValueError, struct.error) as e:
        _LOGGER.debug("Skipping Flash summary: %s", e)
        return
    print_size_line("Flash", flash_used, app_size)
