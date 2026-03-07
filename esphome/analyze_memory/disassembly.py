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

# Pattern to strip "+0xNN" offset suffix from a symbol reference
_OFFSET_SUFFIX_RE = re.compile(r"\+0x[0-9a-fA-F]+$")

# Linker/section symbols that indicate literal pool addresses (not real function refs)
_LITERAL_POOL_SYMBOLS = frozenset(
    ["_lit4_end", "_iram_end", "_data_end", "_bss_end", "_heap_end"]
)

# Xtensa literal pool load instruction (loads 32-bit value from nearby memory)
# The <symbol+offset> in l32r refers to where the literal is stored, not a call target
_L32R_RE = re.compile(r"^l32r\s+")

# Instructions that indicate data misinterpreted as code by objdump's linear sweep.
# These appear when switch tables, constants, or padding are embedded in code sections.
# We skip these lines entirely to avoid noisy diffs with spurious symbol references.
_DATA_AS_CODE_RE = re.compile(
    r"^(?:"
    r"excw"  # exception wait — never used in normal code
    r"|ill"  # illegal instruction — padding/alignment
    r"|\.byte\s"  # raw data bytes objdump couldn't decode
    r"|\.short\s"  # raw data shorts
    r"|\.word\s"  # raw data words
    r"|\{[^}]*excw"  # FLIX bundles containing excw
    r"|orb\s+b"  # coprocessor bool ops — data artifact on non-FPU chips
    r"|orbc\s+b"  # coprocessor bool ops — data artifact
    r")"
)

# Default limits
DEFAULT_MAX_LINES_PER_SYMBOL = 150
DEFAULT_MIN_SYMBOL_SIZE = 16
DEFAULT_MAX_SYMBOLS = 500


def _find_balanced_ref(text: str, start: int) -> tuple[str, int] | None:
    """Find a balanced <...> symbolic reference starting at position start.

    Handles nested angle brackets from C++ template symbols like
    <std::vector<unsigned char, std::allocator<unsigned char> >::resize(...)>.

    Returns (content_between_brackets, end_position) or None if not balanced.
    """
    if start >= len(text) or text[start] != "<":
        return None
    depth = 1
    pos = start + 1
    while pos < len(text) and depth > 0:
        if text[pos] == "<":
            depth += 1
        elif text[pos] == ">":
            depth -= 1
        pos += 1
    if depth != 0:
        return None
    # content is between the outermost < and >
    return text[start + 1 : pos - 1], pos


def _normalize_ref(content: str, func_name: str, is_literal_load: bool) -> str:
    """Normalize a symbolic reference for stable diffs.

    Args:
        content: Text between outermost < and > (e.g. "symbol+0x20")
        func_name: Current function name (for detecting self-references)
        is_literal_load: True if this is a literal pool load (l32r)

    Returns:
        Normalized reference string including angle brackets
    """
    # Strip offset suffix (e.g. "symbol+0x20" -> "symbol")
    symbol = _OFFSET_SUFFIX_RE.sub("", content)

    # Literal pool loads (l32r) always reference data, not code
    if is_literal_load:
        return "<.literal>"

    # Linker section symbols are literal pool addresses
    if symbol in _LITERAL_POOL_SYMBOLS:
        return "<.literal>"

    # Self-references (branches within the same function) - strip offset
    # since it shifts whenever code is added/removed in the function
    if symbol == func_name:
        return "<self>"

    # Cross-function references - keep the symbol name but strip the offset
    return f"<{symbol}>"


def _normalize_all_refs(insn: str, func_name: str, is_literal_load: bool) -> str:
    """Replace all <symbol> references in an instruction with normalized forms.

    Scans left-to-right for balanced <...> pairs to correctly handle
    C++ template symbols with nested angle brackets.
    """
    result: list[str] = []
    pos = 0
    while pos < len(insn):
        idx = insn.find("<", pos)
        if idx == -1:
            result.append(insn[pos:])
            break
        # Copy text before the <
        result.append(insn[pos:idx])
        ref = _find_balanced_ref(insn, idx)
        if ref is None:
            # Unbalanced - copy the < literally and move on
            result.append("<")
            pos = idx + 1
            continue
        content, end_pos = ref
        result.append(_normalize_ref(content, func_name, is_literal_load))
        pos = end_pos
    return "".join(result)


def _normalize_function_asm(
    lines: list[str], base_addr: int, func_name: str, func_size: int = 0
) -> str:
    """Normalize instruction lines for stable cross-build diffs.

    Normalizations applied:
    1. Absolute instruction addresses → relative offsets (+0x0000)
    2. Absolute addresses before symbolic refs → stripped
    3. Self-branch targets (<func+0xNN>) → <self>
    4. Literal pool loads (l32r) → <.literal>
    5. Linker section symbols → <.literal>
    6. Cross-function ref offsets → stripped (keep symbol name only)
    7. Instructions past the known function size → excluded
       (objdump disassembles padding/data between functions as code,
       producing spurious references to unrelated symbols)

    Args:
        lines: Raw instruction lines from objdump
        base_addr: Base address of the function
        func_name: Demangled name of the current function
        func_size: Known size of the function in bytes (0 = no limit)

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
        # Stop at function boundary to avoid disassembling padding/data
        if func_size and offset >= func_size:
            break
        insn = m.group(2)
        # Skip data misinterpreted as code (switch tables, padding, etc.)
        if _DATA_AS_CODE_RE.match(insn):
            continue
        # Strip absolute addresses before symbolic refs
        insn = _ABS_ADDR_IN_INSN_RE.sub("", insn)
        # Detect literal pool loads (Xtensa l32r instruction)
        is_literal = bool(_L32R_RE.match(insn))
        # Normalize symbolic references using balanced bracket matching
        # (regex can't handle nested <> in C++ template symbols)
        insn = _normalize_all_refs(insn, func_name, is_literal)
        # l32r lines have a second parenthesized ref: "l32r a5, <addr> (<data>)"
        # After normalization this becomes "<.literal> (<.literal>)" — trim it
        if is_literal:
            idx = insn.find("<.literal>")
            if idx != -1:
                insn = insn[: idx + len("<.literal>")]
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
    sizes = symbol_sizes or {}

    def _save_function() -> None:
        """Normalize and save the current function's disassembly."""
        if current_func is not None and current_lines:
            func_size = sizes.get(current_func, 0)
            asm = _normalize_function_asm(
                current_lines[:max_lines_per_symbol],
                base_addr,
                current_func,
                func_size,
            )
            if asm:
                functions[current_func] = asm

    for line in result.stdout.splitlines():
        # Check for function header
        m = _FUNC_HEADER_RE.match(line)
        if m:
            _save_function()
            current_func = m.group(2)  # demangled name from objdump -C
            current_lines = []
            base_addr = int(m.group(1), 16)
            continue

        # Collect instruction lines
        if current_func is not None:
            current_lines.append(line)

    _save_function()

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
