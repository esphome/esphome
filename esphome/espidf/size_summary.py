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
import math
from pathlib import Path

_LOGGER = logging.getLogger(__name__)
_SIZE_SUFFIXES = {"K": 1024, "M": 1024 * 1024}


def _parse_size(token: str) -> int:
    token = token.strip()
    if token.startswith(("0x", "0X")):
        return int(token, 16)
    suffix = token[-1].upper()
    if suffix in _SIZE_SUFFIXES:
        return int(token[:-1]) * _SIZE_SUFFIXES[suffix]
    return int(token)


def _find_app_partition_size(partitions_csv: Path) -> int | None:
    """The firmware's app partition size; None when there is nothing to find.

    Mirrors PlatformIO's ``platform-espressif32/builder/main.py::
    _update_max_upload_size``: take the first ``app``-type partition
    whose subtype is ``factory`` or ``ota_0``. Order matters because
    layouts like Adafruit's ``partitions-4MB-tinyuf2.csv`` repurpose
    ``factory`` for a UF2 bootloader before the real OTA slot, so a
    naive "prefer factory" rule would pick the wrong row. No qualifying
    row is legitimate absence (None); a build cannot succeed with a
    missing or malformed table (gen_esp32part consumes it first), so
    those states belong to the backstop -- the missing-file raise just
    names that one cleanly.
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
    return None


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
    """Print PlatformIO-shaped RAM and Flash one-liners; never fails the build."""
    try:
        _print_summary(size_json, partitions_csv)
    except Exception as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Backstop for shapes the named guards below miss; warning so a
        # regression here cannot go missing indefinitely
        _LOGGER.warning(
            "Skipping size summary for %s: %s: %s",
            size_json,
            type(e).__name__,
            e,
            exc_info=True,
        )


def _print_summary(size_json: Path, partitions_csv: Path | None) -> None:
    # The build's own POST_BUILD step writes this file; its absence or an
    # unexpected shape is a regression signal, so these skips warn.
    # FileNotFoundError lands in the OSError arm with the path in its text.
    try:
        data = json.loads(size_json.read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        # ValueError covers JSONDecodeError and a non-UTF-8 (truncated) file
        _LOGGER.warning("Skipping size summary: cannot read %s: %s", size_json, e)
        return
    if not isinstance(data, dict):
        # Non-object JSON has no .get
        _LOGGER.warning("Skipping size summary: unexpected shape in %s", size_json)
        return

    if (ram := _ram_line(data, size_json)) is not None:
        print(ram)
    if (flash := _flash_line(data, size_json, partitions_csv)) is not None:
        print(flash)


def _dict_get(mapping: object, key: str) -> object:
    """dict.get that reads None from any non-dict."""
    return mapping.get(key) if isinstance(mapping, dict) else None


def _present_but_not_dict(value: object) -> bool:
    return value is not None and not isinstance(value, dict)


def _is_number(value: object) -> bool:
    # bool subclasses int; NaN/Infinity are valid JSON for json.loads
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
    )


def _ram_line(data: dict, size_json: Path) -> str | None:
    """The formatted RAM line, or None (already logged) to skip it."""
    memory_types = data.get("memory_types")
    ram_region = None
    if isinstance(memory_types, dict):
        # Key presence, not truthiness: a falsy DRAM value is corrupt, not
        # absent, and must not fall through to DIRAM
        for key in ("DRAM", "DIRAM"):
            if key in memory_types:
                ram_region = memory_types[key]
                break
    used = _dict_get(ram_region, "used")
    total = _dict_get(ram_region, "size")
    if _is_number(used) and _is_number(total) and total > 0:
        return f"RAM:   {_format_bar(int(used), int(total))}"
    malformed = (
        _present_but_not_dict(memory_types)
        or _present_but_not_dict(ram_region)
        or any(v is not None and not _is_number(v) for v in (used, total))
    )
    if malformed:
        # A structurally corrupt report, not a variant without the region
        _LOGGER.warning("Skipping RAM summary: malformed memory_types in %s", size_json)
    else:
        # A variant may name its RAM region differently; healthy builds
        # must not warn
        _LOGGER.debug(
            "Skipping RAM summary: no usable DRAM/DIRAM region in %s", size_json
        )
    return None


def _flash_line(data: dict, size_json: Path, partitions_csv: Path | None) -> str | None:
    """The formatted Flash line, or None (already logged) to skip it.

    Owns both sides of the bar, so nothing after a print can raise: the
    blanket guard is left for genuinely unforeseen shapes.
    """
    image_size = data.get("image_size")
    if not _is_number(image_size):
        _LOGGER.warning("Skipping Flash summary: no usable image_size in %s", size_json)
        return None
    if partitions_csv is None:
        _LOGGER.debug("Skipping Flash summary: no partition table given")
        return None
    app_size = _find_app_partition_size(partitions_csv)
    if not app_size:
        # No qualifying row (a zero-size row has nothing to report either):
        # legitimate for non-app layouts
        _LOGGER.debug("Skipping Flash summary: no app partition in %s", partitions_csv)
        return None
    return f"Flash: {_format_bar(int(image_size), app_size)}"
