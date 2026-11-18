"""Tests for the memory analyzer."""

import subprocess
from unittest.mock import patch

from esphome.analyze_memory import MemoryAnalyzer


def make_analyzer[T: MemoryAnalyzer](
    external_components: set[str] | None = None, cls: type[T] = MemoryAnalyzer
) -> T:
    """Create an analyzer through its real constructor, without an ELF or a toolchain."""
    with (
        patch("esphome.analyze_memory.resolve_tool_path", return_value=None),
        patch("esphome.analyze_memory.find_tool", return_value="nm"),
    ):
        # Any existing file passes the ELF check; nothing reads it
        return cls(__file__, external_components=external_components)


def completed(stdout: str) -> subprocess.CompletedProcess[str]:
    """A successful tool run printing ``stdout``."""
    return subprocess.CompletedProcess([], 0, stdout=stdout)


def hex_dump(section: str, data: bytes, address: int = 0) -> str:
    """Format bytes the way ``readelf -x`` does."""
    lines = [f"Hex dump of section '{section}':"]
    for start in range(0, len(data), 16):
        chunk = data[start : start + 16]
        words = " ".join(chunk[i : i + 4].hex() for i in range(0, len(chunk), 4))
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"  0x{address + start:08x} {words:<35} {text}")
    return "\n".join(lines) + "\n\n"
