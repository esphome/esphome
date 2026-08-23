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
        raise ValueError("blank partition size cell")
    if token.startswith(("0x", "0X")):
        return int(token, 16)
    suffix = token[-1].upper()
    if suffix in _SIZE_SUFFIXES:
        return int(token[:-1]) * _SIZE_SUFFIXES[suffix]
    return int(token)


class _MalformedPartitionRow(ValueError):
    """A matched app row whose size cell cannot be parsed; warned, since the
    table is present but broken."""


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
            try:
                return _parse_size(psize)
            except ValueError as err:
                raise _MalformedPartitionRow(
                    f"{err} for partition {cells[0]} in {partitions_csv}"
                ) from err
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
    """Print PlatformIO-shaped RAM and Flash one-liners; never fails the build."""
    try:
        _print_summary(size_json, partitions_csv)
    except Exception as e:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Backstop for shapes the named guards below miss; warning so a
        # regression here cannot go missing indefinitely
        _LOGGER.warning("Skipping size summary for %s: %s", size_json, e)
        _LOGGER.debug("Size summary failure detail", exc_info=True)


def _print_summary(size_json: Path, partitions_csv: Path | None) -> None:
    # The build's own POST_BUILD step writes this file; its absence or an
    # unexpected shape is a regression signal, so these skips warn
    if not size_json.is_file():
        _LOGGER.warning("Skipping size summary: %s not found", size_json)
        return
    try:
        data = json.loads(size_json.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        _LOGGER.warning("Skipping size summary: %s", e)
        return
    if not isinstance(data, dict):
        # Non-object JSON has no .get
        _LOGGER.warning("Skipping size summary: unexpected shape in %s", size_json)
        return

    # Buffered so a late failure prints nothing instead of half a report
    lines: list[str] = []
    memory_types = data.get("memory_types")
    if not isinstance(memory_types, dict):
        memory_types = {}
    ram_region = memory_types.get("DRAM") or memory_types.get("DIRAM") or {}
    if not isinstance(ram_region, dict):
        ram_region = {}
    ram_used = ram_region.get("used")
    ram_total = ram_region.get("size")
    if (
        isinstance(ram_used, (int, float))
        and isinstance(ram_total, (int, float))
        and ram_total > 0
    ):
        lines.append(f"RAM:   {_format_bar(int(ram_used), int(ram_total))}")
    else:
        _LOGGER.warning(
            "Skipping RAM summary: no usable DRAM/DIRAM region in %s", size_json
        )

    app_size = _resolve_app_size(data, partitions_csv)
    if app_size is not None:
        lines.append(f"Flash: {_format_bar(data['image_size'], app_size)}")
    for line in lines:
        print(line)


def _resolve_app_size(data: dict, partitions_csv: Path | None) -> int | None:
    """The Flash bar's denominator, or None (already logged) to skip it."""
    image_size = data.get("image_size")
    if image_size is None:
        _LOGGER.warning("Skipping Flash summary: no image_size in the size report")
        return None
    if partitions_csv is None:
        _LOGGER.debug("Skipping Flash summary: no partition table given")
        return None
    try:
        app_size = _find_app_partition_size(partitions_csv)
    except (_MalformedPartitionRow, OSError) as e:
        # The table is there but broken/unreadable: visible, like size 0
        _LOGGER.warning("Skipping Flash summary: %s", e)
        return None
    except ValueError as e:
        # Missing file or no qualifying partition: legitimate for
        # non-app layouts, keep quiet
        _LOGGER.debug("Skipping Flash summary: %s", e)
        return None
    if app_size <= 0:
        # Skipping also fails CI's Flash extraction, the right outcome here
        _LOGGER.warning(
            "Skipping Flash summary: app partition size is %s in %s",
            app_size,
            partitions_csv,
        )
        return None
    return app_size
