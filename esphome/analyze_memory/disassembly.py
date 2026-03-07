"""Disassembly extraction and normalization for memory analysis."""

from __future__ import annotations

import logging
import re

from .toolchain import run_tool

_LOGGER = logging.getLogger(__name__)

# Pattern for objdump function headers: "40001234 <function_name>:"
_FUNC_HEADER_RE = re.compile(r"^([0-9a-fA-F]+)\s+<(.+)>:\s*$")

# Pattern for instruction lines: "  40001234:\tinstruction"
_INSN_LINE_RE = re.compile(r"^\s*([0-9a-fA-F]+):\s+(.*)")

# Pattern for absolute addresses before symbolic references in instructions
# Matches "40001234 <symbol>" and replaces the address portion
_ABS_ADDR_IN_INSN_RE = re.compile(r"\b[0-9a-fA-F]{5,}\b(?=\s+<)")

# Default limits
DEFAULT_MAX_LINES_PER_SYMBOL = 150
DEFAULT_MIN_SYMBOL_SIZE = 16
DEFAULT_MAX_SYMBOLS = 500


def _normalize_function_asm(lines: list[str], base_addr: int) -> str:
    """Normalize instruction lines by replacing absolute addresses with offsets.

    Args:
        lines: Raw instruction lines from objdump
        base_addr: Base address of the function

    Returns:
        Normalized disassembly text with relative offsets
    """
    result: list[str] = []
    for line in lines:
        m = _INSN_LINE_RE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        offset = addr - base_addr
        insn = m.group(2)
        # Strip absolute addresses before symbolic refs (e.g., "40001234 <func>")
        # Keep only the symbolic part since it's more meaningful for diffs
        insn = _ABS_ADDR_IN_INSN_RE.sub("", insn)
        result.append(f"+{offset:#06x}: {insn}")
    return "\n".join(result)


def extract_disassembly(
    objdump_path: str,
    elf_path: str,
    symbol_sizes: dict[str, int] | None = None,
    max_lines_per_symbol: int = DEFAULT_MAX_LINES_PER_SYMBOL,
    min_symbol_size: int = DEFAULT_MIN_SYMBOL_SIZE,
    max_symbols: int = DEFAULT_MAX_SYMBOLS,
) -> dict[str, str]:
    """Extract normalized disassembly from an ELF file.

    Runs objdump -d -C --no-show-raw-insn on the ELF, parses the output
    into per-function disassembly, normalizes addresses to relative offsets,
    and returns a dict mapping demangled symbol names to their disassembly.

    Args:
        objdump_path: Path to the objdump binary
        elf_path: Path to the ELF file
        symbol_sizes: Optional dict of symbol_name -> size to filter by.
            If provided, only symbols present in this dict and meeting
            the min_symbol_size threshold are included.
        max_lines_per_symbol: Maximum instruction lines per symbol
        min_symbol_size: Minimum symbol size in bytes to include
        max_symbols: Maximum number of symbols to include (largest first)

    Returns:
        Dict mapping demangled symbol name to normalized disassembly text
    """
    result = run_tool(
        [objdump_path, "-d", "-C", "--no-show-raw-insn", elf_path],
        timeout=120,
    )
    if not result or result.returncode != 0:
        _LOGGER.warning("objdump -d failed or timed out")
        return {}

    # Parse output into per-function disassembly
    functions: dict[str, str] = {}
    current_func: str | None = None
    current_lines: list[str] = []
    base_addr: int = 0

    for line in result.stdout.splitlines():
        # Check for function header
        m = _FUNC_HEADER_RE.match(line)
        if m:
            # Save previous function
            if current_func is not None and current_lines:
                asm = _normalize_function_asm(
                    current_lines[:max_lines_per_symbol], base_addr
                )
                if asm:
                    functions[current_func] = asm
            current_func = m.group(2)  # demangled name from objdump -C
            current_lines = []
            base_addr = int(m.group(1), 16)
            continue

        # Collect instruction lines
        if current_func is not None:
            current_lines.append(line)

    # Save last function
    if current_func is not None and current_lines:
        asm = _normalize_function_asm(current_lines[:max_lines_per_symbol], base_addr)
        if asm:
            functions[current_func] = asm

    # Filter by symbol sizes if provided
    if symbol_sizes:
        # Build set of symbols meeting size threshold
        eligible = {
            name for name, size in symbol_sizes.items() if size >= min_symbol_size
        }
        functions = {name: asm for name, asm in functions.items() if name in eligible}

    # Limit to top N by disassembly length (proxy for code size)
    if len(functions) > max_symbols:
        sorted_funcs = sorted(functions.items(), key=lambda x: len(x[1]), reverse=True)
        functions = dict(sorted_funcs[:max_symbols])

    _LOGGER.debug(
        "Extracted disassembly for %d symbols from %s",
        len(functions),
        elf_path,
    )
    return functions
