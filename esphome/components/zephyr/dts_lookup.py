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

from .board_revision import parse_board_string, resolve_revision
from .const import (
    KEY_BOARD_ROOT,
    KEY_SHIELD_ROOT,
    KEY_SHIELDS,
    KEY_SNIPPET_ROOT,
    KEY_SNIPPETS,
    KEY_ZEPHYR,
)

_LOGGER = logging.getLogger(__name__)

_NOT_FOUND: Final = object()

# Bus label overrides: platform → board → label.
_BUS_OVERRIDES: dict[str, dict[str, str]] = {
    "i2c": {"native_sim/native/64": "i2c0"},
}


def resolve_zephyr_bus(platform: str, board: str, override: str | None = None) -> str:
    """Resolve the Zephyr bus label for a given ESPHome bus platform and board.

    Resolution order: ``override`` (caller-supplied bus label) -> hardcoded
    ``_BUS_OVERRIDES`` table -> DTS lookup via ``_BUS_LOOKUP`` -> ``cv.Invalid``.
    """
    if override is not None:
        _LOGGER.info(
            "[zephyr] %s bus for '%s': %s (explicit override)",
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
    # spi uses 'interface:' for this now; i2c still has its own 'dts_node_override:'.
    override_hint = (
        f"    interface: {platform}0  # replace with your board's Zephyr {platform.upper()} bus label"
        if platform == "spi"
        else f"    dts_node_override: {platform}0  # replace with your board's Zephyr {platform.upper()} bus label"
    )
    raise cv.Invalid(
        f"Cannot determine {platform.upper()} bus label for board '{board}'. "
        f"{detail}\n"
        f"To explicitly set the bus label, configure it directly on your {platform}: entry:\n"
        f"  {platform}:\n"
        f"{override_hint}"
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
    """Return enabled SPI bus labels for board, or None when DTS info is unavailable.

    Returns an empty list when the DTS is parseable but has no enabled SPI buses.
    Results are cached in CORE.data[KEY_ZEPHYR] and cleared between runs.
    """
    cache: dict[str, object] = CORE.data[KEY_ZEPHYR]["spi_bus_cache"]
    if board in cache:
        result = cache[board]
        return None if result is _NOT_FOUND else list(result)  # type: ignore[arg-type]

    buses = _lookup_bus_labels(board, r"spi\d+")
    cache[board] = _NOT_FOUND if buses is None else list(buses)
    return buses


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


def _find_node_label_by_compat(
    board: str, compat: str, *, enabled_only: bool
) -> str | None:
    """Return the label of the first node whose `compatible` includes `compat`, or
    None if the board's DTS declares no such node (or DTS info is unavailable).

    `enabled_only` requires `status = "okay"` -- some peripherals instead ship
    `status = "disabled"` in the SoC dtsi itself with no board turning them on by
    default (e.g. STM32's `rng`), so presence has to be checked independently of
    enablement there.
    """
    edt = _get_edt(board)
    if edt is None:
        return None
    for node in _iter_nodes(edt):
        if (
            compat in node.compats
            and node.labels
            and (not enabled_only or node.status == "okay")
        ):
            return node.labels[0]
    return None


def get_existing_cdc_acm_uart_label(board: str) -> str | None:
    r"""Return the label of an already-enabled `zephyr,cdc-acm-uart` node on the
    board, or None if the board doesn't declare one.

    Unlike _lookup_bus_labels() (which matches by label name pattern, e.g.
    r"uart\d+"), this matches by `compatible` string -- board-provided CDC-ACM
    nodes aren't guaranteed to follow any particular label naming convention
    (e.g. the generic zephyr/boards/common/usb/cdc_acm_serial.dtsi snippet
    labels its node `board_cdc_acm_uart`, not `cdc_acm_uart0`).
    """
    return _find_node_label_by_compat(board, "zephyr,cdc-acm-uart", enabled_only=True)


def get_rng_node_label(board: str) -> str | None:
    """Return the label of the board's `st,stm32-rng` node, or None if its SoC
    declares no such node at all."""
    return _find_node_label_by_compat(board, "st,stm32-rng", enabled_only=False)


def get_console_uart_label(board: str) -> str | None:
    edt = _get_edt(board)
    if edt is None:
        return None
    node = edt.chosen_node("zephyr,console")
    if node is None or not node.labels:
        return None
    return node.labels[0]


def get_can_controller_labels(board: str) -> list[str] | None:
    r"""Return CAN controller node labels for board, or None when DTS info is
    unavailable.

    Matched by label name (r"(fd)?can\d+"), not by `compatible`: STM32 alone spells the
    peripheral two ways (bxCAN's `can1`/`can2` on F0/F1/F3/F4/L4, FDCAN's `fdcan1` on
    U5/C0), and a `bus: can` property does not exist the way it does for uart. Disabled
    nodes count -- every STM32 SoC dtsi ships its CAN nodes `status = "disabled"` and
    expects the application overlay to turn one on, which is exactly what zephyr_can does.
    """
    return _lookup_bus_labels(board, r"(fd)?can\d+")


def get_uart_controller_labels(board: str) -> list[str] | None:
    """Matches by `bus: uart` (every uart-controller binding sets this), not
    `current-speed` -- a real but unconfigured peripheral (e.g. RA4M1's sci0/sci9)
    has no current-speed until enabled, and would otherwise be missed. Returns every
    enabled node, then every disabled one -- unlike _lookup_bus_labels(), which only
    falls back to disabled when nothing enabled matched.
    """
    edt = _get_edt(board)
    if edt is None:
        return None
    enabled: list[str] = []
    disabled: list[str] = []
    for node in _iter_nodes(edt):
        if "uart" in node.buses and node.labels:
            (enabled if node.status == "okay" else disabled).append(node.labels[0])
    return enabled + disabled


def _discover_uart_node_labels(board: str) -> dict[str, str] | None:
    """UART0 is always the board's real `zephyr,console` node, by convention -- not
    just whichever UART happens to be discovered first.

    No logging, no hard failure -- resolve_uart_node_label() and
    log_board_capabilities() both build on this and log it their own way.
    """
    console = get_console_uart_label(board)
    if console is None:
        return None
    others = [
        label for label in (get_uart_controller_labels(board) or []) if label != console
    ]
    mapping = {"UART0": console}
    mapping.update({f"UART{i + 1}": label for i, label in enumerate(others)})
    return mapping


def resolve_uart_node_label(
    board: str, hw_uart: str, static_labels: dict[str, str]
) -> str:
    """`static_labels` empty means the variant declares no portable mapping across
    its boards (e.g. stm32l4, where UART naming and which one is console both vary
    per board) -- labels are discovered from DTS instead (_discover_uart_node_labels()).
    Otherwise the hand-declared label is used, with UART0 verified (best effort)
    against the board's real console -- a mismatch warns rather than hard-fails,
    since the declared UART is still real and working, just not that board's console.
    """
    if static_labels:
        label = static_labels.get(hw_uart)
        if label is None:
            raise cv.Invalid(
                f"'{hw_uart}' is not a valid hardware_uart for board '{board}'. "
                f"Valid values: {', '.join(static_labels)}"
            )
        _LOGGER.info(
            "[zephyr] %s for '%s': %s (from hardcoded uart_node_labels)",
            hw_uart,
            board,
            label,
        )
        if hw_uart == "UART0":
            console = get_console_uart_label(board)
            if console is not None and console != label:
                _LOGGER.warning(
                    "[zephyr] hardware_uart: UART0 is documented to always be the "
                    "board's console UART, but the hardcoded UART0 label for board "
                    "'%s' ('%s') does not match its actual zephyr,console ('%s'). "
                    "Logging will still work, but not on the UART you may expect.",
                    board,
                    label,
                    console,
                )
        return label

    mapping = _discover_uart_node_labels(board)
    if mapping is None:
        raise cv.Invalid(
            f"Cannot determine the console UART for board '{board}' -- its DTS "
            "could not be resolved. Install gcc/cpp (C preprocessor) for automatic "
            "DTS detection, or verify the board name."
        )
    label = mapping.get(hw_uart)
    if label is None:
        others = [v for k, v in mapping.items() if k != "UART0"]
        raise cv.Invalid(
            f"Board '{board}' has no '{hw_uart}' -- besides its console "
            f"({mapping['UART0']}), its DTS has {len(others)} other enabled "
            f"UART(s): {others or 'none'}."
        )
    source = "board's zephyr,console, from DTS" if hw_uart == "UART0" else "from DTS"
    _LOGGER.info("[zephyr] %s for '%s': %s (%s)", hw_uart, board, label, source)
    return label


def format_uart_node_label_map(board: str, static_labels: dict[str, str]) -> str:
    """Same source data resolve_uart_node_label() resolves against -- purely
    informational here, no logging or hard failure of its own.
    """
    mapping = (
        dict(static_labels) if static_labels else _discover_uart_node_labels(board)
    )
    if not mapping:
        return "(unavailable)"
    return ", ".join(f"{slot}={label}" for slot, label in mapping.items())


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
    """Return a parsed edtlib.EDT for the board (merged with any selected shields:
    and snippets: devicetree overlays), or None when unavailable.

    Result is cached in CORE.data[KEY_ZEPHYR]["board_edt_cache"], keyed on the
    board plus the selected shields/snippets (a different selection can add or
    remove nodes), so cpp is only invoked once per unique combination per run.
    """
    zd = CORE.data.get(KEY_ZEPHYR, {})
    shields: list[str] = zd.get(KEY_SHIELDS) or []
    snippets: list[str] = zd.get(KEY_SNIPPETS) or []
    # Revision is already embedded in `board` itself, but spelling it out keeps the
    # cache key's intent obvious and matches the other two selections' explicitness.
    cache_key = (board, tuple(shields), tuple(snippets))

    cache = CORE.data[KEY_ZEPHYR]["board_edt_cache"]
    if cache_key in cache:
        result = cache[cache_key]
        return None if result is _NOT_FOUND else result

    dts_base = zd.get("dts_base_path")

    edtlib = _load_edtlib(Path(dts_base) if dts_base else None)
    if edtlib is None:
        cache[cache_key] = _NOT_FOUND
        return None
    if not dts_base:
        cache[cache_key] = _NOT_FOUND
        return None

    zephyr_base = Path(dts_base)

    board_dir = _find_board_dir(zephyr_base, board)
    if board_dir is None:
        _LOGGER.debug("[zephyr] Board directory not found for '%s'", board)
        cache[cache_key] = _NOT_FOUND
        return None

    dts_file = _find_dts_file(board_dir, board)
    if dts_file is None:
        _LOGGER.debug("[zephyr] No .dts file found for '%s' in %s", board, board_dir)
        cache[cache_key] = _NOT_FOUND
        return None

    preprocessed = _preprocess_dts(dts_file, zephyr_base, board_dir)
    if preprocessed is None:
        cache[cache_key] = _NOT_FOUND
        return None

    # Shields/snippets contribute overlay fragments on top of the base board tree --
    # concatenated after the base dts text, mirroring Zephyr's own build (cmake/
    # modules/dts.cmake), so `&label { ... };` overlay-override syntax resolves
    # against the preceding base tree exactly as it would for real.
    overlay_texts = [preprocessed]

    # A `board@revision` overlay is applied first so shields/snippets below can
    # still override anything it sets.
    requested_revision = parse_board_string(board).revision
    if requested_revision is not None:
        resolved_revision, declares_revisions = resolve_revision(
            board_dir, requested_revision
        )
        if declares_revisions and resolved_revision is not None:
            overlay_file = _find_revision_overlay(board_dir, board, resolved_revision)
            if overlay_file is not None:
                text = _preprocess_dts_file(overlay_file, zephyr_base, [str(board_dir)])
                if text is not None:
                    overlay_texts.append(text)
            else:
                _LOGGER.debug(
                    "[zephyr] No revision overlay file found for '%s' (resolved "
                    "revision '%s'); DTS validation won't see its changes",
                    board,
                    resolved_revision,
                )
        elif declares_revisions:
            _LOGGER.debug(
                "[zephyr] Revision '%s' does not resolve against board '%s''s "
                "declared revisions; DTS validation uses the base board tree only",
                requested_revision,
                board,
            )

    shield_root = zd.get(KEY_SHIELD_ROOT)
    shield_search_roots = (
        [Path(shield_root), zephyr_base] if shield_root else [zephyr_base]
    )
    for shield in shields:
        shield_dir = _find_shield_dir(shield_search_roots, shield)
        if shield_dir is None:
            _LOGGER.debug(
                "[zephyr] Shield directory not found for '%s'; DTS validation "
                "won't see its nodes",
                shield,
            )
            continue
        for overlay_file in _shield_overlay_files(shield_dir, board):
            text = _preprocess_dts_file(overlay_file, zephyr_base, [str(shield_dir)])
            if text is not None:
                overlay_texts.append(text)

    snippet_root = zd.get(KEY_SNIPPET_ROOT)
    snippet_search_roots = (
        [Path(snippet_root), zephyr_base] if snippet_root else [zephyr_base]
    )
    for snippet in snippets:
        snippet_dir = _find_snippet_dir(snippet_search_roots, snippet)
        if snippet_dir is None:
            _LOGGER.debug(
                "[zephyr] Snippet directory not found for '%s'; DTS validation "
                "won't see its nodes",
                snippet,
            )
            continue
        for overlay_file in _snippet_overlay_files(snippet_dir, board):
            text = _preprocess_dts_file(
                overlay_file, zephyr_base, [str(overlay_file.parent)]
            )
            if text is not None:
                overlay_texts.append(text)

    with tempfile.NamedTemporaryFile(
        suffix=".dts", mode="w", delete=False, encoding="utf-8"
    ) as f:
        f.write("\n".join(overlay_texts))
        tmp_path = Path(f.name)

    try:
        bindings_dir = zephyr_base / "dts" / "bindings"
        bindings_dirs = [str(bindings_dir)] if bindings_dir.is_dir() else []
        edt = edtlib.EDT(
            str(tmp_path), bindings_dirs, warn_reg_unit_address_mismatch=False
        )
        cache[cache_key] = edt
        return edt
    except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        _LOGGER.debug("[zephyr] edtlib parsing failed for '%s': %s", board, exc)
        cache[cache_key] = _NOT_FOUND
        return None
    finally:
        tmp_path.unlink(missing_ok=True)


def _find_board_dir(zephyr_base: Path, board: str) -> Path | None:
    cache = CORE.data[KEY_ZEPHYR]["board_dir_cache"]
    if board in cache:
        cached = cache[board]
        return Path(cached) if cached else None

    # HWMv2 board names are "<board>/<soc>" but on disk the directory is just
    # "<board>" under a vendor folder; an optional "@<revision>" suffix must also be
    # stripped before matching the dirname.
    board_dirname = parse_board_string(board).name

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

    # Some HWMv2 board directories drop the vendor prefix from the dirname even
    # though board.yml's "name:" keeps the full name -- fall back to scanning
    # board.yml files. Recursive, not one level: some vendors nest an extra category
    # directory under the vendor folder.
    if result is None:
        for boards_root in (root / "boards" for root in search_roots):
            if not boards_root.is_dir():
                continue
            for board_yml in boards_root.glob("*/**/board.yml"):
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


def _find_qualified_file(board_dir: Path, board: str, suffix: str) -> Path | None:
    # HWMv2 board strings are "<board>[/<soc>[/<variant>[/...]]]" (arbitrarily deep,
    # e.g. rpi_pico2's rp2350a/m33/w/mcuboot). The on-disk filename joins every
    # remaining segment with "_", except <soc> is dropped when it's already part of
    # the board dirname (esp32c6_devkitc/esp32c6/hpcore -> _hpcore.dts, not
    # _esp32c6_hpcore.dts) -- hence trying fully-qualified, then soc-dropped, then bare.
    #
    # A "@<revision>" suffix never appears in base .dts/.yaml filenames -- only
    # revision *overlay* filenames include it (built by _find_revision_overlay()
    # instead) -- so it's always stripped here.
    parts = board.split("/")
    board_dirname = parts[0].split("@", 1)[0]
    rest = parts[1:]

    if rest:
        qualified = board_dir / f"{board_dirname}_{'_'.join(rest)}{suffix}"
        if qualified.exists():
            return qualified

    if len(rest) > 1:
        qualified = board_dir / f"{board_dirname}_{'_'.join(rest[1:])}{suffix}"
        if qualified.exists():
            return qualified

    exact = board_dir / f"{board_dirname}{suffix}"
    if exact.exists():
        return exact

    return None


def _find_revision_overlay(
    board_dir: Path, board: str, resolved_revision: str
) -> Path | None:
    """Find a board@revision target's devicetree overlay file, if any.

    Revision overlays are named like base .dts files but with the resolved revision
    (dots replaced by underscores) appended as an extra trailing segment after any
    qualifiers, e.g. boards/actinius/icarus/actinius_icarus_nrf9160_ns_2_0_0.overlay
    for board "actinius_icarus/nrf9160/ns" at revision "2.0.0". Reuses
    _find_qualified_file()'s existing full/soc-dropped/bare fallback chain by
    building a synthetic board string with the revision as its own segment.
    """
    parts = parse_board_string(board)
    revision_segment = resolved_revision.replace(".", "_")
    qualifiers = parts.qualifiers or ""  # Already has a leading "/", or is empty.
    synthetic = f"{parts.name}{qualifiers}/{revision_segment}"
    return _find_qualified_file(board_dir, synthetic, ".overlay")


def _find_dts_file(board_dir: Path, board: str) -> Path | None:
    # A board directory can contain more than one .dts file (e.g.
    # esp32c6_devkitc_hpcore.dts and _lpcore.dts side by side) so an unqualified glob
    # is ambiguous; _find_qualified_file's naming match must disambiguate first.
    found = _find_qualified_file(board_dir, board, ".dts")
    if found is not None:
        return found

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
    found = _find_qualified_file(board_dir, board, ".yaml")
    if found is not None:
        return found

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
        text = dts_file.read_text(encoding="utf-8")
    except OSError:
        return ""

    def _inline_include(m: re.Match) -> str:
        inc_path = base_dir / m.group(1)
        if not inc_path.exists():
            return ""
        return _read_dts_with_includes(inc_path, base_dir, _seen)

    return re.sub(r'#include\s+"([^"]+)"', _inline_include, text)


def _preprocess_dts_file(
    src_file: Path, zephyr_base: Path, extra_include_dirs: list[str]
) -> str | None:
    cpp = _find_cpp()
    if cpp is None:
        _LOGGER.debug(
            "[zephyr] 'cpp' not found in PATH; DTS preprocessing unavailable. "
            "Install gcc/cpp to enable automatic bus detection."
        )
        return None

    include_dirs = _get_dts_include_paths(zephyr_base) + extra_include_dirs

    cmd = [cpp, "-x", "assembler-with-cpp", "-nostdinc", "-E", "-P"]
    for d in include_dirs:
        cmd += ["-I", d]
    cmd.append(str(src_file))

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)  # noqa: S603
        return result.stdout
    except subprocess.CalledProcessError as exc:
        _LOGGER.debug(
            "[zephyr] cpp failed on %s: %s", src_file.name, exc.stderr.strip()
        )
        return None


def _preprocess_dts(dts_file: Path, zephyr_base: Path, board_dir: Path) -> str | None:
    return _preprocess_dts_file(dts_file, zephyr_base, [str(board_dir)])


def _find_in_search_roots(
    search_roots: list[Path], subpath: str, name: str
) -> Path | None:
    """Return the first `<root>/<subpath>/<name>` directory that exists, or None.

    Shared by _find_shield_dir()/_find_snippet_dir() -- both just need the first
    matching root under a different fixed subpath (shields have no vendor
    subdirectory layer, unlike boards, so the path is always directly
    boards/shields/<shield>/; snippets are always directly under snippets/<name>/).
    """
    for root in search_roots:
        candidate = root / subpath / name
        if candidate.is_dir():
            return candidate
    return None


def _find_shield_dir(search_roots: list[Path], shield: str) -> Path | None:
    """Find a shield's directory (boards/shields/<shield>/) under any search root."""
    return _find_in_search_roots(search_roots, "boards/shields", shield)


def _shield_overlay_files(shield_dir: Path, board: str) -> list[Path]:
    """Return a shield's devicetree overlay file(s) for the given board.

    A shield always contributes <shield_dir>/<shield>.overlay (applied to every
    board it's used with), and may additionally contribute a board-specific
    override at <shield_dir>/boards/<qualified board name>.overlay -- unlike some
    base board .dts files, this filename keeps every board-string segment (e.g.
    nrf5340dk_nrf5340_cpuapp.overlay), so it reuses _find_qualified_file()'s own
    fallback convention rather than a bare-name-only lookup.
    """
    files = []
    base_overlay = shield_dir / f"{shield_dir.name}.overlay"
    if base_overlay.is_file():
        files.append(base_overlay)
    board_overlay = _find_qualified_file(shield_dir / "boards", board, ".overlay")
    if board_overlay is not None:
        files.append(board_overlay)
    return files


def _find_snippet_dir(search_roots: list[Path], snippet: str) -> Path | None:
    """Find a snippet's directory (snippets/<snippet>/) under any search root."""
    return _find_in_search_roots(search_roots, "snippets", snippet)


def _as_str_list(value: object) -> list[str]:
    """Normalize a snippet.yml append: value into a list of strings.

    CMake's EXTRA_DTC_OVERLAY_FILE is itself a list-typed cache variable, so a
    snippet.yml is free to declare it as a YAML list rather than a single string --
    without this, a list value would reach `snippet_dir / p` and raise TypeError.
    """
    if not value:
        return []
    if isinstance(value, list):
        return [str(v) for v in value]
    return [str(value)]


def _snippet_overlay_files(snippet_dir: Path, board: str) -> list[Path]:
    """Return a snippet's devicetree overlay file(s) for board, from snippet.yml.

    A snippet.yml declares overlay paths rather than shipping a fixed <name>.overlay
    at its root, keyed per-SoC under boards: <regex>:. Both the top-level append: and
    any matching boards: entry are included -- CMake's zephyr_get() accumulates
    rather than replaces, so a real `west build` applies both when both are present.
    """
    yml_file = snippet_dir / "snippet.yml"
    if not yml_file.is_file():
        return []
    try:
        doc = yaml.safe_load(yml_file.read_text(encoding="utf-8"))
    except (OSError, yaml.YAMLError) as exc:
        _LOGGER.debug(
            "[zephyr] Failed to parse snippet.yml for '%s': %s", snippet_dir.name, exc
        )
        return []
    if not isinstance(doc, dict):
        return []

    rel_paths: list[str] = []
    rel_paths.extend(
        _as_str_list((doc.get("append") or {}).get("EXTRA_DTC_OVERLAY_FILE"))
    )

    for pattern, board_conf in (doc.get("boards") or {}).items():
        # Patterns are written /<regex>/ -- strip the delimiters before matching.
        regex = pattern.strip("/")
        if not isinstance(board_conf, dict):
            continue
        try:
            matched = re.fullmatch(regex, board) is not None
        except re.error:
            matched = False
        if matched:
            rel_paths.extend(
                _as_str_list(
                    (board_conf.get("append") or {}).get("EXTRA_DTC_OVERLAY_FILE")
                )
            )

    files = [snippet_dir / p for p in rel_paths]
    return [f for f in files if f.is_file()]


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
        # Some vendors (STM32, NXP, TI, ...) ship SoC/pinctrl dts fragments in their
        # own HAL module, a sibling `modules/hal/<vendor>/dts` boards #include
        # directly -- without this, cpp fails on that include, silently breaking DTS
        # lookups for those vendors only.
        hal_dir = zephyr_base.parent / "modules" / "hal"
        if hal_dir.is_dir():
            paths.extend(
                str(vendor_dts)
                for vendor_dir in hal_dir.iterdir()
                if (vendor_dts := vendor_dir / "dts").is_dir()
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

    Reads the raw (un-preprocessed) DTS and quoted .dtsi includes to find
    {BUS_LABEL}_SDA_GPIO{n} / {BUS_LABEL}_SCL_GPIO{n} pinctrl macros (e.g.
    I2C0_SDA_GPIO6) in the ``{bus_label}_default`` pinctrl node -- the pin number is
    embedded directly in the macro name, no bit-field to decode.
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


def get_i2c_pinctrl_silabs(board: str, bus_label: str) -> dict[str, int] | None:
    """Return default I2C SDA/SCL flat GPIO numbers for a Silicon Labs (silabs) board.

    Same approach as get_i2c_pinctrl_esp32, but Silicon Labs' pinctrl macros are
    lettered-port form ({BUS_LABEL}_SDA_P{PORT}{n}, e.g. I2C0_SDA_PC5) rather than
    ESP32's flat GPIO{n} form.
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
    pins = _extract_silabs_i2c_pins(text, bus_label)
    if pins is not None:
        _LOGGER.debug(
            "[zephyr] DTS pinctrl defaults for '%s' %s: SDA=%d SCL=%d",
            board,
            bus_label,
            pins["sda"],
            pins["scl"],
        )
    return pins


def _extract_silabs_i2c_pins(text: str, bus_label: str) -> dict[str, int] | None:
    """Extract SDA/SCL flat GPIO numbers from I2C{n}_SDA/SCL_P{port}{pin} macro names.

    Scoped to the ``{bus_label}_default { ... }`` pinctrl node, same brace-matching
    approach as _extract_esp32_i2c_pins.
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
    sda_m = re.search(rf"{prefix}_SDA_P([A-D])(\d+)", node_text)
    scl_m = re.search(rf"{prefix}_SCL_P([A-D])(\d+)", node_text)
    if sda_m is None or scl_m is None:
        _LOGGER.debug(
            "[zephyr] %s_SDA_P.../%s_SCL_P... not both found in '%s_default'",
            prefix,
            prefix,
            bus_label,
        )
        return None
    sda = (ord(sda_m.group(1)) - ord("A")) * 16 + int(sda_m.group(2))
    scl = (ord(scl_m.group(1)) - ord("A")) * 16 + int(scl_m.group(2))
    return {"sda": sda, "scl": scl}


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
        "st,stm32-fdcan",
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
    shields: list[str] | None = None,
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
    ]
    if shields:
        lines.append(f"[zephyr] Shields: {', '.join(shields)}")
    lines.append(
        "[zephyr] hardware, as defined by the board itself"
        + (" and its shield(s)" if shields else "")
        + " (independent of what ESPHome currently uses):"
    )

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
        "[zephyr]   hardware_uart label mapping: "
        + format_uart_node_label_map(board, variant.uart_node_labels)
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


def validate_board_revision(board: str) -> bool | None:
    """Check whether `board`'s "@<revision>" (if any) resolves against its board.yml.

    Returns None when there's nothing to check: the standard tree isn't available, the
    board itself doesn't resolve, `board` has no "@<revision>" suffix, or the resolved
    board declares no revisions at all (a revision on such a board is meaningless, but
    not this function's job to reject -- Zephyr's own build would just ignore it).
    """
    requested_revision = parse_board_string(board).revision
    if requested_revision is None:
        return None
    zd = CORE.data.get(KEY_ZEPHYR, {})
    dts_base = zd.get("dts_base_path")
    if not dts_base:
        return None
    board_dir = _find_board_dir(Path(dts_base), board)
    if board_dir is None:
        return None
    resolved, declares_revisions = resolve_revision(board_dir, requested_revision)
    if not declares_revisions:
        return None
    return resolved is not None


# ---------------------------------------------------------------------------
# Bus lookup registry — populated after function definitions
# ---------------------------------------------------------------------------

_BUS_LOOKUP: dict[str, Callable[[str], list[str] | None]] = {
    "i2c": get_enabled_i2c_buses,
    "spi": get_enabled_spi_buses,
}
