"""Tests for esphome.build_helpers.ninja_gen."""

from __future__ import annotations

from pathlib import Path

from esphome.build_helpers import ninja_gen


def test_anchor_path_flags_anchors_relative_operands(tmp_path: Path) -> None:
    """Relative operands resolve from the build path, as under PlatformIO."""
    absolute = str(tmp_path / "abs")
    assert ninja_gen.anchor_path_flags(
        [
            "-Iinc",
            f"-I{absolute}",
            "-Llib",
            "-include",
            "pre.h",
            "-I",
            "split",
            "-DUSE_HOST",
            "-lssl",
            "-I",
        ],
        tmp_path,
    ) == [
        f"-I{tmp_path / 'inc'}",
        f"-I{absolute}",
        f"-L{tmp_path / 'lib'}",
        "-include",
        str(tmp_path / "pre.h"),
        "-I",
        str(tmp_path / "split"),
        "-DUSE_HOST",
        "-lssl",
        "-I",
    ]


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
