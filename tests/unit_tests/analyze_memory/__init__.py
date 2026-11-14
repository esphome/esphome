"""Tests for the memory analyzer."""

from unittest.mock import patch

from esphome.analyze_memory import MemoryAnalyzer


def make_analyzer(external_components: set[str] | None = None) -> MemoryAnalyzer:
    """Create a MemoryAnalyzer through its real constructor, without an ELF or a toolchain."""
    with (
        patch("esphome.analyze_memory.resolve_tool_path", return_value=None),
        patch("esphome.analyze_memory.find_tool", return_value="nm"),
    ):
        # Any existing file passes the ELF check; nothing reads it
        return MemoryAnalyzer(__file__, external_components=external_components)
