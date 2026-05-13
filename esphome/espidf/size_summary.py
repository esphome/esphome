"""PlatformIO-format RAM/Flash one-liners after a native ESP-IDF build.

``idf.py size`` (chained onto ``idf.py build`` in
``toolchain.run_compile``) prints the per-region table inline as part
of the build. This module adds two summary lines underneath,
byte-identical to PlatformIO's output:

    RAM:   [====      ]  26.5% (used 47932 bytes from 180736 bytes)
    Flash: [===       ]  48.4% (used 888511 bytes from 1835008 bytes)

The format matches ``script/ci_memory_impact_extract.py`` so CI memory
analysis works unchanged on native ESP-IDF builds. RAM total is the
DRAM region size from the linker map; Flash total is the ``app+ota_0``
partition size from ``partitions.csv``, mirroring PlatformIO's
``platforms/espressif32/builder/main.py::_set_default_size`` rule.

Structured size data is produced at link time by a CMake POST_BUILD
custom command (see ``build_gen/espidf.py``) which writes
``esp_idf_size.json`` next to the ELF. We read that file here rather
than re-running ``esp_idf_size`` from Python.
"""

from __future__ import annotations

import csv
import json
import logging
from pathlib import Path

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
    """Return the size of the partition the firmware lands in.

    Prefers ``app + ota_0`` (PlatformIO's
    ``platforms/espressif32/builder/main.py::_set_default_size`` rule);
    accepts ``app + factory`` for non-OTA single-image layouts. Raises
    ``ValueError`` if neither is present so callers don't silently misreport
    the Flash budget against the wrong partition.
    """
    if not partitions_csv.is_file():
        raise ValueError(f"partitions.csv not found at {partitions_csv}")
    factory_size: int | None = None
    for row in csv.reader(partitions_csv.read_text().splitlines()):
        cells = [c.strip() for c in row]
        if not cells or cells[0].startswith("#") or len(cells) < 5:
            continue
        ptype, psubtype, psize = cells[1], cells[2], cells[4]
        if ptype not in ("app", "0"):
            continue
        size = _parse_size(psize)
        if psubtype == "ota_0":
            return size
        if psubtype == "factory":
            factory_size = size
    if factory_size is not None:
        return factory_size
    raise ValueError(f"No app+ota_0 or app+factory partition in {partitions_csv}")


def _format_bar(used: int, total: int) -> str:
    """Match PlatformIO's ``_format_availale_bytes`` (pioupload.py) exactly."""
    pct_raw = used / total if total else 0
    blocks = 10
    filled = min(int(round(blocks * pct_raw)), blocks)
    progress = "=" * filled
    return (
        f"[{progress:<{blocks}}] {pct_raw: 6.1%} "
        f"(used {used:d} bytes from {total:d} bytes)"
    )


def print_summary(size_json: Path, partitions_csv: Path | None) -> None:
    """Print PlatformIO-shaped RAM and Flash one-liners.

    Failures are non-fatal: the build has already succeeded, we just couldn't
    summarize. Logs the cause at debug level.
    """
    if not size_json.is_file():
        _LOGGER.debug("Skipping size summary: %s not found", size_json)
        return
    try:
        data = json.loads(size_json.read_text())
    except (OSError, json.JSONDecodeError) as e:
        _LOGGER.debug("Skipping size summary: %s", e)
        return

    dram = data.get("memory_types", {}).get("DRAM") or {}
    ram_used = dram.get("used")
    ram_total = dram.get("size")
    if ram_total and ram_used is not None:
        print(f"RAM:   {_format_bar(ram_used, ram_total)}")

    image_size = data.get("image_size")
    if image_size is None or partitions_csv is None:
        return
    try:
        app_size = _find_app_partition_size(partitions_csv)
    except ValueError as e:
        _LOGGER.debug("Skipping Flash summary: %s", e)
        return
    print(f"Flash: {_format_bar(image_size, app_size)}")
