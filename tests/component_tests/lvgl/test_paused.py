"""Tests for the LVGL ``paused`` option code generation."""

from __future__ import annotations

import re

_SET_PAUSED_RE = re.compile(r"->set_paused\((.+?)\);")


def _extract_set_paused(main_cpp: str) -> list[str]:
    """Return the argument text of every set_paused() call found."""
    return [m.group(1) for m in _SET_PAUSED_RE.finditer(main_cpp)]


class TestPausedCodeGeneration:
    """Verify that the ``paused`` option drives the set_paused() call."""

    def test_paused_true_generates_set_paused(
        self, generate_main, component_config_path
    ):
        """``paused: true`` emits a set_paused(true, false) call."""
        main_cpp = generate_main(component_config_path("paused.yaml"))
        calls = _extract_set_paused(main_cpp)
        assert calls == ["true, false"]

    def test_paused_default_omits_set_paused(
        self, generate_main, component_config_path
    ):
        """Without ``paused`` (default false) no set_paused call is generated."""
        main_cpp = generate_main(component_config_path("not_paused.yaml"))
        assert _extract_set_paused(main_cpp) == []
