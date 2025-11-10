"""Helper functions for memory analysis."""

from functools import cache
from pathlib import Path

from .const import SECTION_MAPPING

# Import namespace constant from parent module
# Note: This would create a circular import if done at module level,
# so we'll define it locally here as well
_NAMESPACE_ESPHOME = "esphome::"


# Get the list of actual ESPHome components by scanning the components directory
@cache
def get_esphome_components():
    """Get set of actual ESPHome components from the components directory."""
    # Find the components directory relative to this file
    # Go up two levels from analyze_memory/helpers.py to esphome/
    current_dir = Path(__file__).parent.parent
    components_dir = current_dir / "components"

    if not components_dir.exists() or not components_dir.is_dir():
        return frozenset()

    return frozenset(
        item.name
        for item in components_dir.iterdir()
        if item.is_dir()
        and not item.name.startswith(".")
        and not item.name.startswith("__")
    )


@cache
def get_component_class_patterns(component_name: str) -> list[str]:
    """Generate component class name patterns for symbol matching.

    Args:
        component_name: The component name (e.g., "ota", "wifi", "api")

    Returns:
        List of pattern strings to match against demangled symbols
    """
    component_upper = component_name.upper()
    component_camel = component_name.replace("_", "").title()
    return [
        f"{_NAMESPACE_ESPHOME}{component_upper}Component",  # e.g., esphome::OTAComponent
        f"{_NAMESPACE_ESPHOME}ESPHome{component_upper}Component",  # e.g., esphome::ESPHomeOTAComponent
        f"{_NAMESPACE_ESPHOME}{component_camel}Component",  # e.g., esphome::OtaComponent
        f"{_NAMESPACE_ESPHOME}ESPHome{component_camel}Component",  # e.g., esphome::ESPHomeOtaComponent
    ]


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
