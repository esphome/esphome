"""Symbol demangling utilities for memory analysis.

This module provides functions for demangling C++ symbol names using c++filt.
"""

from __future__ import annotations

import logging
import re
import subprocess

from .toolchain import find_tool

_LOGGER = logging.getLogger(__name__)

# GCC global constructor/destructor prefix annotations
GCC_PREFIX_ANNOTATIONS = {
    "_GLOBAL__sub_I_": "global constructor for",
    "_GLOBAL__sub_D_": "global destructor for",
}

# GCC optimization suffix pattern (e.g., $isra$0, $part$1, $constprop$2)
GCC_OPTIMIZATION_SUFFIX_PATTERN = re.compile(r"(\$(?:isra|part|constprop)\$\d+)")


def _strip_gcc_annotations(symbol: str) -> tuple[str, str]:
    """Strip GCC optimization suffixes and prefixes from a symbol.

    Args:
        symbol: The mangled symbol name

    Returns:
        Tuple of (stripped_symbol, removed_prefix)
    """
    # Remove GCC optimization markers
    stripped = GCC_OPTIMIZATION_SUFFIX_PATTERN.sub("", symbol)

    # Handle GCC global constructor/initializer prefixes
    prefix = ""
    for gcc_prefix in GCC_PREFIX_ANNOTATIONS:
        if stripped.startswith(gcc_prefix):
            prefix = gcc_prefix
            stripped = stripped[len(prefix) :]
            break

    return stripped, prefix


def _restore_symbol_prefix(prefix: str, stripped: str, demangled: str) -> str:
    """Restore prefix that was removed before demangling.

    Args:
        prefix: Prefix that was removed (e.g., "_GLOBAL__sub_I_")
        stripped: Stripped symbol name
        demangled: Demangled symbol name

    Returns:
        Demangled name with prefix restored/annotated
    """
    if not prefix:
        return demangled

    # Successfully demangled - add descriptive prefix
    if demangled != stripped and (annotation := GCC_PREFIX_ANNOTATIONS.get(prefix)):
        return f"[{annotation}: {demangled}]"

    # Failed to demangle - restore original prefix
    return prefix + demangled


def _restore_symbol_suffix(original: str, demangled: str) -> str:
    """Restore GCC optimization suffix that was removed before demangling.

    Args:
        original: Original symbol name with suffix
        demangled: Demangled symbol name without suffix

    Returns:
        Demangled name with suffix annotation
    """
    if suffix_match := GCC_OPTIMIZATION_SUFFIX_PATTERN.search(original):
        return f"{demangled} [{suffix_match.group(1)}]"
    return demangled


def batch_demangle(
    symbols: list[str],
    cppfilt_path: str | None = None,
    objdump_path: str | None = None,
) -> dict[str, str]:
    """Batch demangle C++ symbol names.

    Args:
        symbols: List of symbol names to demangle
        cppfilt_path: Path to c++filt binary (auto-detected if not provided)
        objdump_path: Path to objdump binary to derive c++filt path from

    Returns:
        Dictionary mapping original symbol names to demangled names
    """
    cache: dict[str, str] = {}

    if not symbols:
        return cache

    # Find c++filt tool
    cppfilt_cmd = cppfilt_path or find_tool("c++filt", objdump_path)
    if not cppfilt_cmd:
        _LOGGER.warning("Could not find c++filt, symbols will not be demangled")
        return {s: s for s in symbols}

    _LOGGER.debug("Demangling %d symbols using %s", len(symbols), cppfilt_cmd)

    # Strip GCC optimization suffixes and prefixes before demangling
    symbols_stripped: list[str] = []
    symbols_prefixes: list[str] = []
    for symbol in symbols:
        stripped, prefix = _strip_gcc_annotations(symbol)
        symbols_stripped.append(stripped)
        symbols_prefixes.append(prefix)

    try:
        result = subprocess.run(
            [cppfilt_cmd],
            input="\n".join(symbols_stripped),
            capture_output=True,
            text=True,
            check=False,
        )
    except (subprocess.SubprocessError, OSError, UnicodeDecodeError) as e:
        _LOGGER.warning("Failed to batch demangle symbols: %s", e)
        return {s: s for s in symbols}

    if result.returncode != 0:
        _LOGGER.warning(
            "c++filt exited with code %d: %s",
            result.returncode,
            result.stderr[:200] if result.stderr else "(no error output)",
        )
        return {s: s for s in symbols}

    # Process demangled output
    demangled_lines = result.stdout.strip().split("\n")

    # Check for output length mismatch
    if len(demangled_lines) != len(symbols):
        _LOGGER.warning(
            "c++filt output mismatch: expected %d lines, got %d",
            len(symbols),
            len(demangled_lines),
        )
        return {s: s for s in symbols}

    failed_count = 0

    for original, stripped, prefix, demangled in zip(
        symbols, symbols_stripped, symbols_prefixes, demangled_lines
    ):
        # Add back any prefix that was removed
        demangled = _restore_symbol_prefix(prefix, stripped, demangled)

        # If we stripped a suffix, add it back to the demangled name for clarity
        if original != stripped and not prefix:
            demangled = _restore_symbol_suffix(original, demangled)

        cache[original] = demangled

        # Count symbols that failed to demangle
        if stripped == demangled and stripped.startswith("_Z"):
            failed_count += 1
            if failed_count <= 5:
                _LOGGER.debug("Failed to demangle: %s", original)

    if failed_count > 0:
        _LOGGER.debug(
            "Failed to demangle %d/%d symbols using %s",
            failed_count,
            len(symbols),
            cppfilt_cmd,
        )

    return cache
