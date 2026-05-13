"""PlatformIO-format RAM/Flash one-liners after a native ESP-IDF build.

``idf.py size`` (chained onto ``idf.py build`` in
``toolchain.run_compile``) prints the per-region table inline as part
of the build. This module adds two summary lines underneath,
byte-identical to PlatformIO's output:

    RAM:   [====      ]  26.5% (used 47932 bytes from 180736 bytes)
    Flash: [===       ]  48.4% (used 888511 bytes from 1835008 bytes)

The format matches ``script/ci_memory_impact_extract.py`` so CI memory
analysis works unchanged on native ESP-IDF builds. RAM total is the
DRAM region size from the linker map; Flash total is taken from
``partitions.csv`` using PlatformIO's rule (first app partition whose
subtype is ``factory`` or ``ota_0``; see
``platform-espressif32/builder/main.py::_update_max_upload_size``).

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
    for row in csv.reader(partitions_csv.read_text().splitlines()):
        cells = [c.strip() for c in row]
        if not cells or cells[0].startswith("#") or len(cells) < 5:
            continue
        ptype, psubtype, psize = cells[1], cells[2], cells[4]
        if ptype in ("app", "0") and psubtype in ("factory", "ota_0"):
            return _parse_size(psize)
    raise ValueError(f"No app+factory or app+ota_0 partition in {partitions_csv}")


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
