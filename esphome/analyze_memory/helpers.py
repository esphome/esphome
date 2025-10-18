"""Helper functions for memory analysis."""

from .const import SECTION_MAPPING


def map_section_name(raw_section: str) -> str | None:
    """Map raw section name to standard section.

    Args:
        raw_section: Raw section name from ELF file (e.g., ".iram0.text", ".rodata.str1.1")

    Returns:
        Standard section name (".text", ".rodata", ".data", ".bss") or None
    """
    for standard_section, patterns in SECTION_MAPPING.items():
        if any(pattern in raw_section for pattern in patterns):
            return standard_section
    return None


def parse_symbol_line(line: str) -> tuple[str, str, int, str] | None:
    """Parse a single symbol line from objdump output.

    Args:
        line: Line from objdump -t output

    Returns:
        Tuple of (section, name, size, address) or None if not a valid symbol.
        Format: address l/g w/d F/O section size name
        Example: 40084870 l     F .iram0.text    00000000 _xt_user_exc
    """
    parts = line.split()
    if len(parts) < 5:
        return None

    try:
        # Validate and extract address
        address = parts[0]
        int(address, 16)
    except ValueError:
        return None

    # Look for F (function) or O (object) flag
    if "F" not in parts and "O" not in parts:
        return None

    # Find section, size, and name
    for i, part in enumerate(parts):
        if not part.startswith("."):
            continue

        section = map_section_name(part)
        if not section:
            break

        # Need at least size field after section
        if i + 1 >= len(parts):
            break

        try:
            size = int(parts[i + 1], 16)
        except ValueError:
            break

        # Need symbol name and non-zero size
        if i + 2 >= len(parts) or size == 0:
            break

        name = " ".join(parts[i + 2 :])
        return (section, name, size, address)

    return None
