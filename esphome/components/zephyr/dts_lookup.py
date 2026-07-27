from collections.abc import Callable
import contextlib
import logging
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from typing import Final

import yaml

import esphome.config_validation as cv
from esphome.core import CORE

from .const import KEY_BOARD_ROOT, KEY_ZEPHYR

_LOGGER = logging.getLogger(__name__)

_NOT_FOUND: Final = object()

# Bus label overrides: platform → board → label.
_BUS_OVERRIDES: dict[str, dict[str, str]] = {
    "i2c": {"native_sim/native/64": "i2c0"},
}


def resolve_zephyr_bus(platform: str, board: str, override: str | None = None) -> str:
    """Resolve the Zephyr bus label for a given ESPHome bus platform and board.

    Resolution order:
    1. ``override`` — explicit label from ``dts_node_override:`` on the bus component.
    2. ``_BUS_OVERRIDES`` — Python hardcoded table.
    3. DTS lookup via ``_BUS_LOOKUP`` (requires the ``cpp`` preprocessor).
    4. ``cv.Invalid`` — explains what was tried and how to set ``dts_node_override:``.

    To support a new peripheral type (e.g. "spi", "uart"):
    - Implement ``get_enabled_spi_buses(board) -> list[str] | None`` below.
    - Add it to ``_BUS_LOOKUP``: ``"spi": get_enabled_spi_buses``.
    - Call ``resolve_zephyr_bus("spi", board)`` from the spi component ``to_code()``.
    """
    if override is not None:
        _LOGGER.info(
            "[zephyr] %s bus for '%s': %s (from dts_node_override)",
            platform.upper(),
            board,
            override,
        )
        return override

    hardcoded = _BUS_OVERRIDES.get(platform, {}).get(board)
    if hardcoded is not None:
        _LOGGER.info(
            "[zephyr] %s bus for '%s': %s (from hardcoded override)",
            platform.upper(),
            board,
            hardcoded,
        )
        return hardcoded

    lookup_fn = _BUS_LOOKUP.get(platform)
    buses = lookup_fn(board) if lookup_fn is not None else None

    if buses:
        if len(buses) > 1:
            _LOGGER.info(
                "[zephyr] Multiple %s buses on '%s': %s; using '%s'",
                platform.upper(),
                board,
                buses,
                buses[0],
            )
        else:
            _LOGGER.info(
                "[zephyr] %s bus for '%s': %s (from DTS)",
                platform.upper(),
                board,
                buses[0],
            )
        return buses[0]

    if buses is not None:
        detail = (
            f"Board '{board}' has no enabled {platform.upper()} buses in its DTS. "
            "Verify the board name or check the board's DTS file."
        )
    else:
        detail = "Install gcc/cpp (C preprocessor) for automatic DTS detection."
    raise cv.Invalid(
        f"Cannot determine {platform.upper()} bus label for board '{board}'. "
        f"{detail}\n"
        f"To explicitly set the bus label, add 'dts_node_override' to your {platform}: configuration:\n"
        f"  {platform}:\n"
        f"    dts_node_override: {platform}0  # replace with your board's Zephyr "
        f"{platform.upper()} bus label"
    )


# ---------------------------------------------------------------------------
# Per-platform DTS lookup functions
# ---------------------------------------------------------------------------


def get_enabled_i2c_buses(board: str) -> list[str] | None:
    """Return enabled I2C bus labels for board, or None when DTS info is unavailable.

    Returns an empty list when the DTS is parseable but has no enabled I2C buses.
    Results are cached in CORE.data[KEY_ZEPHYR] and cleared between runs.
    """
    cache: dict[str, object] = CORE.data[KEY_ZEPHYR]["i2c_bus_cache"]
    if board in cache:
        result = cache[board]
        return None if result is _NOT_FOUND else list(result)  # type: ignore[arg-type]

    buses = _lookup_bus_labels(board, r"i2c\d+")
    cache[board] = _NOT_FOUND if buses is None else list(buses)
    return buses


def get_enabled_spi_buses(board: str) -> list[str] | None:
    """Return present SPI bus labels for board, or None when DTS info is unavailable.

    Report-only (not part of _BUS_LOOKUP/resolve_zephyr_bus) -- no ESPHome component
    resolves a Zephyr SPI bus label from DTS yet, this just reports what's present.
    """
    return _lookup_bus_labels(board, r"spi\d+")


def get_enabled_uart_buses(board: str) -> list[str] | None:
    """Return present UART bus labels for board, or None when DTS info is unavailable.

    Report-only, same rationale as get_enabled_spi_buses().
    """
    return _lookup_bus_labels(board, r"uart\d+")


def _lookup_bus_labels(board: str, label_pattern: str) -> list[str] | None:
    r"""Return sorted DTS node labels matching label_pattern (e.g. r"i2c\d+"), or None.

    Prefers enabled (status = "okay") nodes; falls back to disabled-but-present ones,
    since some SoC families (e.g. Espressif) disable all peripherals by default in the
    SoC DTS and rely on application overlays to enable them -- callers that just want
    to know what exists on the board still need those labels.
    """
    edt = _get_edt(board)
    if edt is None:
        return None

    enabled: list[str] = []
    disabled: list[str] = []
    for node in _iter_nodes(edt):
        for label in node.labels:
            if re.fullmatch(label_pattern, label):
                (enabled if node.status == "okay" else disabled).append(label)
                break

    result = sorted(set(enabled or disabled))
    _LOGGER.debug(
        "[zephyr] Buses matching '%s' for '%s': %s", label_pattern, board, result
    )
    return result


# ---------------------------------------------------------------------------
# Shared DTS helpers
# ---------------------------------------------------------------------------


def _load_edtlib(zephyr_base: Path | None):
    """Import edtlib, preferring Zephyr's own bundled copy over the PyPI package.

    The PyPI devicetree 0.0.2 snapshot is older than the in-tree version and may
    reject binding keys added in recent Zephyr releases (e.g. 'examples:').
    Inserting the bundled path before any existing devicetree in sys.modules ensures
    we get the version that matches the bindings we're parsing.
    """
    import importlib
    import sys

    if zephyr_base is not None:
        bundled = (
            zephyr_base / "scripts" / "dts" / "python-devicetree" / "src"
        ).resolve()
        if bundled.is_dir():
            # Evict any previously-loaded (PyPI) devicetree so the bundled version wins.
            for key in [
                k
                for k in sys.modules
                if k == "devicetree" or k.startswith("devicetree.")
            ]:
                del sys.modules[key]
            sys.path.insert(0, str(bundled))
            importlib.invalidate_caches()
            try:
                from devicetree import edtlib

                return edtlib
            except ImportError as exc:
                _LOGGER.debug("[zephyr] bundled edtlib load failed: %s", exc)
            finally:
                # Path only needed during import; modules stay in sys.modules cache.
                with contextlib.suppress(ValueError):
                    sys.path.remove(str(bundled))

    try:
        from devicetree import edtlib

        return edtlib
    except ImportError:
        _LOGGER.debug("[zephyr] devicetree/edtlib not available; DTS lookup disabled")
        return None


def _get_edt(board: str):
    """Return a parsed edtlib.EDT for the board, or None when unavailable.

    Result is cached in CORE.data[KEY_ZEPHYR]["board_edt_cache"] so cpp is
    only invoked once per board per run even when multiple lookups need it.
    """
    cache = CORE.data[KEY_ZEPHYR]["board_edt_cache"]
    if board in cache:
        result = cache[board]
        return None if result is _NOT_FOUND else result

    zd = CORE.data.get(KEY_ZEPHYR, {})
    dts_base = zd.get("dts_base_path")

    edtlib = _load_edtlib(Path(dts_base) if dts_base else None)
    if edtlib is None:
        cache[board] = _NOT_FOUND
        return None
    if not dts_base:
        cache[board] = _NOT_FOUND
        return None

    zephyr_base = Path(dts_base)

    board_dir = _find_board_dir(zephyr_base, board)
    if board_dir is None:
        _LOGGER.debug("[zephyr] Board directory not found for '%s'", board)
        cache[board] = _NOT_FOUND
        return None

    dts_file = _find_dts_file(board_dir, board)
    if dts_file is None:
        _LOGGER.debug("[zephyr] No .dts file found for '%s' in %s", board, board_dir)
        cache[board] = _NOT_FOUND
        return None

    preprocessed = _preprocess_dts(dts_file, zephyr_base, board_dir)
    if preprocessed is None:
        cache[board] = _NOT_FOUND
        return None

    with tempfile.NamedTemporaryFile(suffix=".dts", mode="w", delete=False) as f:
        f.write(preprocessed)
        tmp_path = Path(f.name)

    try:
        bindings_dir = zephyr_base / "dts" / "bindings"
        bindings_dirs = [str(bindings_dir)] if bindings_dir.is_dir() else []
        edt = edtlib.EDT(
            str(tmp_path), bindings_dirs, warn_reg_unit_address_mismatch=False
        )
        cache[board] = edt
        return edt
    except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        _LOGGER.debug("[zephyr] edtlib parsing failed for '%s': %s", board, exc)
        cache[board] = _NOT_FOUND
        return None
    finally:
        tmp_path.unlink(missing_ok=True)


def _find_board_dir(zephyr_base: Path, board: str) -> Path | None:
    cache = CORE.data[KEY_ZEPHYR]["board_dir_cache"]
    if board in cache:
        cached = cache[board]
        return Path(cached) if cached else None

    # HWMv2 board names are "<board>/<soc>" (e.g. "esp32h2_devkitm/esp32h2") but on
    # disk the directory is just "<board>" under a vendor folder. All supported
    # variants require Zephyr >= 4.4.0, which is always HWMv2 (introduced in 3.7.0).
    board_dirname = board.split("/", maxsplit=1)[0]

    # A board_source: root is searched first, mirroring west's own BOARD_ROOT precedence.
    board_root = CORE.data.get(KEY_ZEPHYR, {}).get(KEY_BOARD_ROOT)
    search_roots = [Path(board_root)] if board_root else []
    search_roots.append(zephyr_base)

    result = None
    for boards_root in (root / "boards" for root in search_roots):
        if not boards_root.is_dir():
            continue
        for parent in boards_root.iterdir():
            if not parent.is_dir():
                continue
            candidate = parent / board_dirname
            if candidate.is_dir():
                result = candidate
                break
        if result is not None:
            break

    # Some HWMv2 board directories drop the vendor prefix from the dirname (e.g.
    # boards/adafruit/itsybitsy/ for board "adafruit_itsybitsy") even though the
    # board.yml "name:" field keeps the full name. Fall back to scanning board.yml
    # files for a matching name when the direct dirname guess misses.
    if result is None:
        for boards_root in (root / "boards" for root in search_roots):
            if not boards_root.is_dir():
                continue
            for board_yml in boards_root.glob("*/*/board.yml"):
                try:
                    doc = yaml.safe_load(board_yml.read_text())
                except (OSError, yaml.YAMLError):
                    continue
                if (
                    isinstance(doc, dict)
                    and doc.get("board", {}).get("name") == board_dirname
                ):
                    result = board_yml.parent
                    break
            if result is not None:
                break

    cache[board] = str(result) if result is not None else ""
    return result


def _find_dts_file(board_dir: Path, board: str) -> Path | None:
    # HWMv2 board strings are "<board>" or "<board>/<soc>" or, for boards with
    # multiple cores (e.g. esp32c6_devkitc/esp32c6/hpcore), "<board>/<soc>/<qualifier>".
    # The .dts filename on disk is "<board>[_<qualifier>].dts" -- the middle <soc>
    # segment never appears in the filename. A board directory can contain more
    # than one .dts file (e.g. esp32c6_devkitc_hpcore.dts and _lpcore.dts side by
    # side) so an unqualified glob is ambiguous; the qualifier must disambiguate.
    parts = board.split("/")
    board_dirname = parts[0]
    qualifier = parts[2] if len(parts) >= 3 else None

    if qualifier:
        qualified = board_dir / f"{board_dirname}_{qualifier}.dts"
        if qualified.exists():
            return qualified

    exact = board_dir / f"{board_dirname}.dts"
    if exact.exists():
        return exact

    candidates = [
        f for f in board_dir.glob("*.dts") if not f.name.endswith("_defconfig.dts")
    ]
    return candidates[0] if candidates else None


def _find_board_yaml(board_dir: Path, board: str) -> Path | None:
    """Find the Twister board-metadata YAML (<board>[_<qualifier>].yaml) for board.

    Same naming/qualifier convention as _find_dts_file, different extension -- this
    file sits next to the .dts and carries the board author's own declared
    `supported:` capability list, maintained independently of the DTS.
    """
    parts = board.split("/")
    board_dirname = parts[0]
    qualifier = parts[2] if len(parts) >= 3 else None

    if qualifier:
        qualified = board_dir / f"{board_dirname}_{qualifier}.yaml"
        if qualified.exists():
            return qualified

    exact = board_dir / f"{board_dirname}.yaml"
    if exact.exists():
        return exact

    candidates = list(board_dir.glob("*.yaml"))
    return candidates[0] if candidates else None


def _read_dts_with_includes(
    dts_file: Path, base_dir: Path, _seen: set[Path] | None = None
) -> str:
    """Read a DTS file and recursively inline any quoted #include files from the same directory.

    Unlike _preprocess_dts (which runs the full C preprocessor and expands pinctrl
    macros like I2C0_SDA_GPIO0 down to opaque packed integers), this keeps macro
    names intact -- needed by extractors that read the pin number back out of the
    macro name itself (e.g. _extract_esp32_i2c_pins) rather than decoding it from
    an encoded bit-field value.
    """
    if _seen is None:
        _seen = set()
    abs_file = dts_file.resolve()
    if abs_file in _seen:
        return ""
    _seen.add(abs_file)

    try:
        text = dts_file.read_text()
    except OSError:
        return ""

    def _inline_include(m: re.Match) -> str:
        inc_path = base_dir / m.group(1)
        if not inc_path.exists():
            return ""
        return _read_dts_with_includes(inc_path, base_dir, _seen)

    return re.sub(r'#include\s+"([^"]+)"', _inline_include, text)


def _preprocess_dts(dts_file: Path, zephyr_base: Path, board_dir: Path) -> str | None:
    cpp = _find_cpp()
    if cpp is None:
        _LOGGER.debug(
            "[zephyr] 'cpp' not found in PATH; DTS preprocessing unavailable. "
            "Install gcc/cpp to enable automatic bus detection."
        )
        return None

    include_dirs = _get_dts_include_paths(zephyr_base) + [str(board_dir)]

    cmd = [cpp, "-x", "assembler-with-cpp", "-nostdinc", "-E", "-P"]
    for d in include_dirs:
        cmd += ["-I", d]
    cmd.append(str(dts_file))

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)  # noqa: S603
        return result.stdout
    except subprocess.CalledProcessError as exc:
        _LOGGER.debug(
            "[zephyr] cpp failed on %s: %s", dts_file.name, exc.stderr.strip()
        )
        return None


def _find_cpp() -> str | None:
    zd = CORE.data[KEY_ZEPHYR]
    if zd["cpp_path"] == "":
        found = shutil.which("cpp") or shutil.which("gcc") or None
        zd["cpp_path"] = found
    return zd["cpp_path"]


def _get_dts_include_paths(zephyr_base: Path) -> list[str]:
    zd = CORE.data[KEY_ZEPHYR]
    if zd["dts_include_paths"] is None:
        dts_dir = zephyr_base / "dts"
        paths = [str(zephyr_base / "include"), str(dts_dir)]
        if dts_dir.is_dir():
            paths.extend(
                str(subdir)
                for subdir in dts_dir.iterdir()
                if subdir.is_dir() and subdir.name != "bindings"
            )
        zd["dts_include_paths"] = paths
    return zd["dts_include_paths"]


def _iter_nodes(edt):
    """Yield individual nodes from edtlib's scc_order (which is a list of SCCs)."""
    for scc in edt.scc_order:
        if isinstance(scc, list):
            yield from scc
        else:
            yield scc


# ---------------------------------------------------------------------------
# Per-vendor I2C pinctrl extractors -- registered via ZephyrVariant.pinctrl_extractors
# ---------------------------------------------------------------------------


def get_i2c_pinctrl_esp32(board: str, bus_label: str) -> dict[str, int] | None:
    """Return default I2C SDA/SCL flat GPIO numbers for an Espressif esp32-family board.

    Reads the raw (un-preprocessed) DTS and any quoted .dtsi includes to find
    {BUS_LABEL}_SDA_GPIO{n} / {BUS_LABEL}_SCL_GPIO{n} pinctrl macros (e.g.
    I2C0_SDA_GPIO6) in the ``{bus_label}_default`` pinctrl node. The pin number
    is embedded directly in the macro name -- unlike Nordic's NRF_PSEL(signal,
    port, pin) form, there's no packed bit-field to decode. Returns
    ``{"sda": N, "scl": N}`` as flat GPIO numbers (ESP32 pins are already flat,
    same "GPIO#" convention nrf52/gpio.py normalizes port.pin notation to), or
    None when not found.
    """
    zd = CORE.data.get(KEY_ZEPHYR, {})
    dts_base = zd.get("dts_base_path")
    if not dts_base:
        _LOGGER.debug("[zephyr] DTS base path not set; skipping pinctrl extraction")
        return None

    zephyr_base = Path(dts_base)
    board_dir = _find_board_dir(zephyr_base, board)
    if board_dir is None:
        _LOGGER.debug("[zephyr] Board dir not found for '%s'", board)
        return None

    dts_file = _find_dts_file(board_dir, board)
    if dts_file is None:
        _LOGGER.debug("[zephyr] DTS file not found for '%s'", board)
        return None

    text = _read_dts_with_includes(dts_file, board_dir)
    pins = _extract_esp32_i2c_pins(text, bus_label)
    if pins is not None:
        _LOGGER.debug(
            "[zephyr] DTS pinctrl defaults for '%s' %s: SDA=%d SCL=%d",
            board,
            bus_label,
            pins["sda"],
            pins["scl"],
        )
    return pins


def _extract_esp32_i2c_pins(text: str, bus_label: str) -> dict[str, int] | None:
    """Extract SDA/SCL flat GPIO numbers from I2C{n}_SDA/SCL_GPIO{pin} macro names.

    Scoped to the ``{bus_label}_default { ... }`` pinctrl node specifically
    (brace-matched), not a blind whole-file search -- a board's DTS can define
    other boards' overlay variants or comments mentioning the same macro name
    outside the node that's actually active for this board.
    """
    node_start_re = re.compile(
        rf"{re.escape(bus_label)}_default"
        rf"(?:\s*:\s*{re.escape(bus_label)}_default)?\s*\{{",
        re.DOTALL,
    )
    m = node_start_re.search(text)
    if m is None:
        _LOGGER.debug("[zephyr] No pinctrl node '%s_default' found", bus_label)
        return None

    start = m.end() - 1  # position of opening '{'
    depth = 0
    end = start
    for i, ch in enumerate(text[start:], start):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    node_text = text[start : end + 1]

    prefix = bus_label.upper()
    sda_m = re.search(rf"{prefix}_SDA_GPIO(\d+)", node_text)
    scl_m = re.search(rf"{prefix}_SCL_GPIO(\d+)", node_text)
    if sda_m is None or scl_m is None:
        _LOGGER.debug(
            "[zephyr] %s_SDA_GPIO/%s_SCL_GPIO not both found in '%s_default'",
            prefix,
            prefix,
            bus_label,
        )
        return None
    return {"sda": int(sda_m.group(1)), "scl": int(scl_m.group(1))}


# ---------------------------------------------------------------------------
# Board hardware feature detection
# ---------------------------------------------------------------------------

# Maps feature name → list of DTS compatible strings that indicate its presence.
# A feature is reported when any of its compatibles appears in an enabled node.
_FEATURE_COMPATIBLES: dict[str, list[str]] = {
    "BLE": ["nordic,nrf-radio"],
    "IEEE802154": ["nordic,nrf-ieee802154", "espressif,esp32-ieee802154"],
    "NFC": ["nordic,nrf-nfct"],
    "USB": [
        "nordic,nrf-usbd",
        "nordic,nrf-udc",
        "nordic,nrf-usbhs",
        "zephyr,usb-device",
    ],
    "WiFi": [
        "espressif,esp32-wifi",
        "espressif,esp32s2-wifi",
        "espressif,esp32s3-wifi",
        "infineon,cat1-wifi",
        "nordic,wlan",
    ],
    "CAN": [
        "bosch,m_can",
        "microchip,mcp2515",
        "nordic,nrf-can",
        "nxp,flexcan",
        "st,stm32-can",
    ],
}


def get_board_features(board: str) -> list[str] | None:
    """Return sorted list of detected hardware feature names for the board.

    Returns None when DTS info is unavailable (cpp absent or DTS not fetched).
    Callers that want this surfaced to the user should log it themselves --
    log_board_capabilities() folds this into its combined report.
    """
    edt = _get_edt(board)
    if edt is None:
        return None

    enabled_compats: set[str] = set()
    for node in _iter_nodes(edt):
        if node.status == "okay":
            enabled_compats.update(node.compats)

    features = sorted(
        name
        for name, compats in _FEATURE_COMPATIBLES.items()
        if any(c in enabled_compats for c in compats)
    )
    _LOGGER.debug(
        "[zephyr] DTS-detected hardware features for '%s': %s", board, features
    )
    return features


# ---------------------------------------------------------------------------
# Board metadata (Twister board YAML) -- no cpp/gcc dependency
# ---------------------------------------------------------------------------


def get_board_yaml_supported(board: str) -> list[str] | None:
    """Return the board's own `supported:` capability list from its Twister board YAML.

    This is the board author's own maintained list (adc, i2c, spi, netif:wifi, ...),
    read straight from <board>[_<qualifier>].yaml next to the .dts -- independent of
    DTS parsing, so no cpp/gcc dependency and it still works when DTS preprocessing
    is unavailable. Returns None when the file can't be found/parsed. Results are
    cached in CORE.data[KEY_ZEPHYR] and cleared between runs.
    """
    cache: dict[str, object] = CORE.data[KEY_ZEPHYR]["board_yaml_cache"]
    if board in cache:
        result = cache[board]
        return None if result is _NOT_FOUND else list(result)  # type: ignore[arg-type]

    supported = _lookup_board_yaml_supported(board)
    cache[board] = _NOT_FOUND if supported is None else list(supported)
    return supported


def _lookup_board_yaml_supported(board: str) -> list[str] | None:
    zd = CORE.data.get(KEY_ZEPHYR, {})
    dts_base = zd.get("dts_base_path")
    if not dts_base:
        return None

    board_dir = _find_board_dir(Path(dts_base), board)
    if board_dir is None:
        return None

    yaml_file = _find_board_yaml(board_dir, board)
    if yaml_file is None:
        _LOGGER.debug("[zephyr] No board metadata YAML found for '%s'", board)
        return None

    try:
        doc = yaml.safe_load(yaml_file.read_text())
    except (OSError, yaml.YAMLError) as exc:
        _LOGGER.debug("[zephyr] Failed to parse board YAML for '%s': %s", board, exc)
        return None

    if not isinstance(doc, dict):
        return None
    return sorted(str(s) for s in doc.get("supported") or [])


# ---------------------------------------------------------------------------
# Flash partition detection
# ---------------------------------------------------------------------------

# A partition node either lives under a parent with one of these compatibles
# (mainline convention), or sets one of these on itself directly (this project's
# custom boards, which use "zephyr,mapped-partition" per-child instead).
_PARTITION_PARENT_COMPATS = {"fixed-partitions"}
_PARTITION_NODE_COMPATS = {"zephyr,mapped-partition", "fixed-subpartitions"}


def get_board_partitions(board: str) -> list[tuple[str, int, int]] | None:
    """Return (label, offset, size) flash partitions defined by the board's DTS,
    sorted by offset, or None when DTS info is unavailable (cpp absent).
    """
    edt = _get_edt(board)
    if edt is None:
        return None

    partitions: list[tuple[str, int, int]] = []
    for node in _iter_nodes(edt):
        if node.label is None or not node.regs:
            continue
        own_compats = set(node.compats)
        parent_compats = set(node.parent.compats) if node.parent else set()
        if (
            own_compats & _PARTITION_NODE_COMPATS
            or parent_compats & _PARTITION_PARENT_COMPATS
        ):
            partitions.append((node.label, node.regs[0].addr, node.regs[0].size))

    partitions.sort(key=lambda p: p[1])
    _LOGGER.debug("[zephyr] Flash partitions for '%s': %s", board, partitions)
    return partitions


# ---------------------------------------------------------------------------
# Combined capability report -- what Zephyr thinks the board can do
# ---------------------------------------------------------------------------


def _format_size(num_bytes: int) -> str:
    if num_bytes % 1024 == 0:
        return f"{num_bytes // 1024}K"
    return f"{num_bytes}B"


def log_board_capabilities(
    board: str,
    variant_name: str,
    variant,
    framework_ver: str,
    board_root: Path | None,
) -> None:
    """Log one combined block describing what Zephyr thinks this board can do.

    Purely informational: never restricts or warns about the user's config, just
    surfaces the board's own declared/DTS-derived hardware and partition layout so
    a user doesn't have to go digging through the Zephyr board definition folder
    themselves. Always logs at INFO with every line present -- "(none)"/
    "(unavailable)" shown explicitly rather than the line being omitted.
    """
    source = (
        f"custom board, from board_source: {Path(board_root).resolve()}"
        if board_root is not None
        else "stock Zephyr board"
    )
    lines = [
        f"[zephyr] Board '{board}' ({source}), Zephyr {framework_ver}",
        "[zephyr] hardware, as defined by the board itself "
        "(independent of what ESPHome currently uses):",
    ]

    yaml_supported = get_board_yaml_supported(board)
    lines.append(
        "[zephyr]   Declared in board definition: "
        + (", ".join(yaml_supported) if yaml_supported else "(unavailable)")
    )

    dts_features = get_board_features(board)
    lines.append(
        "[zephyr]   Also detected via DTS scan (BLE/IEEE802154/NFC/USB/WiFi/CAN only): "
        + (", ".join(dts_features) if dts_features else "(none or unavailable)")
    )

    for name, buses in (
        ("I2C", get_enabled_i2c_buses(board)),
        ("SPI", get_enabled_spi_buses(board)),
        ("UART", get_enabled_uart_buses(board)),
    ):
        lines.append(
            f"[zephyr]   {name} buses present: "
            + (", ".join(buses) if buses else "(none or unavailable)")
        )

    lines.append(
        "[zephyr]   MCUboot swap methods this variant's port supports: "
        + (
            ", ".join(sorted(variant.swap_methods))
            if variant.swap_methods
            else "(none)"
        )
    )

    # Only shown when this variant has real MCUboot/OTA involvement -- native_sim's
    # board DTS defines a partition layout too, but it's unused boilerplate there.
    if variant.swap_methods:
        partitions = get_board_partitions(board)
        if partitions:
            lines.append("[zephyr]   Flash partitions defined by the board:")
            label_width = max(len(label) for label, _, _ in partitions)
            for label, addr, size in partitions:
                lines.append(
                    f"[zephyr]     {label:<{label_width}}  0x{addr:06x} - {_format_size(size):>6}"
                )
        else:
            lines.append(
                "[zephyr]   Flash partitions defined by the board: (unavailable)"
            )

    _LOGGER.info("\n".join(lines))


# ---------------------------------------------------------------------------
# Board existence validation
# ---------------------------------------------------------------------------


def validate_board(board: str) -> bool | None:
    """Check whether `board` resolves to a real board directory.

    Returns None (can't tell, not invalid) when the standard tree isn't available to
    check against.
    """
    zd = CORE.data.get(KEY_ZEPHYR, {})
    dts_base = zd.get("dts_base_path")
    if not dts_base:
        return None
    return _find_board_dir(Path(dts_base), board) is not None


# ---------------------------------------------------------------------------
# Bus lookup registry — populated after function definitions
# ---------------------------------------------------------------------------

_BUS_LOOKUP: dict[str, Callable[[str], list[str] | None]] = {
    "i2c": get_enabled_i2c_buses,
}
