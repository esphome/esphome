"""Tests for esphome.build_helpers.ninja_gen."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from esphome.build_helpers import ninja_gen
from esphome.build_helpers.ninja_gen import Flag


@pytest.mark.parametrize(
    ("flag", "anchored"),
    [
        (("-Iinc",), ("-I{base}/inc",)),
        (("-Llib",), ("-L{base}/lib",)),
        (("-include", "pre.h"), ("-include", "{base}/pre.h")),
        (("-isystem", "sys"), ("-isystem", "{base}/sys")),
        # The glued spelling of a flag that takes a path
        (("-isystemsys",), ("-isystem{base}/sys",)),
        (("-includepre.h",), ("-include{base}/pre.h",)),
        # An absolute operand is never changed
        (("-I{base}/abs",), ("-I{base}/abs",)),
        (("-include", "{base}/abs.h"), ("-include", "{base}/abs.h")),
        # Not a path
        (("-DUSE_HOST",), ("-DUSE_HOST",)),
        (("-lssl",), ("-lssl",)),
        (("-framework", "Cocoa"), ("-framework", "Cocoa")),
        (("-I",), ("-I",)),
    ],
)
def test_anchor_path_flag(tmp_path: Path, flag: Flag, anchored: Flag) -> None:
    """Relative operands resolve from the build path, as under PlatformIO."""

    def fill(tokens: Flag) -> Flag:
        return tuple(
            str(Path(tok.replace("{base}", str(tmp_path))))
            if "{base}" in tok and not tok.startswith("-")
            else tok.replace("{base}/", f"{tmp_path}{os.sep}")
            for tok in tokens
        )

    assert ninja_gen.anchor_path_flag(fill(flag), tmp_path) == fill(anchored)


def test_collect_sources_skips_excluded_and_other_files(tmp_path: Path) -> None:
    for name in ("b.cpp", "a.c", "skip.cpp", "notes.txt", "sub/c.S"):
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("")
    assert ninja_gen.collect_sources(tmp_path, {"skip.cpp"}) == [
        tmp_path / "a.c",
        tmp_path / "b.cpp",
        tmp_path / "sub" / "c.S",
    ]


@pytest.mark.parametrize("ccache", ["/usr/bin/ccache", None])
def test_compile_rule_lines_splice_the_launcher(ccache: str | None) -> None:
    """Without ccache no command keeps a leading space, which CreateProcess
    on Windows rejects; with it every compile rule starts with the launcher."""
    commands = [
        line.removeprefix("  command = ")
        for line in ninja_gen.compile_rule_lines(ccache)
        if line.startswith("  command = ")
    ]
    assert len(commands) == 4
    for command in commands:
        if ccache:
            assert command.startswith("$ccache $c")
        else:
            assert command.startswith("$c")
            assert "$ccache" not in command
