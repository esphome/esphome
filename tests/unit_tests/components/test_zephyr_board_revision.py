"""Unit tests for esphome.components.zephyr.board_revision."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.components.zephyr.board_revision import (
    BoardParts,
    declared_revisions,
    parse_board_string,
    resolve_revision,
)

# ---------------------------------------------------------------------------
# parse_board_string
# ---------------------------------------------------------------------------


def test_parse_board_string_bare_name() -> None:
    assert parse_board_string("nrf52840dk") == BoardParts("nrf52840dk", None, None)


def test_parse_board_string_with_qualifiers_only() -> None:
    assert parse_board_string("esp32h2_devkitm/esp32h2") == BoardParts(
        "esp32h2_devkitm", None, "/esp32h2"
    )


def test_parse_board_string_letter_revision() -> None:
    assert parse_board_string("some_board@A").revision == "A"


# Real-world qualifier depths seen across supported variants' board strings, 1
# through 5 total segments (0 through 4 "/"-qualifiers after the bare name):
#   1: native_sim's "native_sim/native/64"-shaped bare boards (0 qualifiers here)
#   2: esp32h2_devkitm/esp32h2 (1 qualifier)
#   3: esp32c6_devkitc/esp32c6/hpcore (2 qualifiers)
#   4: adafruit_feather_nrf52840/nrf52840/sense/uf2 (3 qualifiers)
#   5: rpi_pico2/rp2350a/m33/w/mcuboot (4 qualifiers, the deepest real case)
# Revision always sits directly after the bare name, before any qualifiers --
# matches Zephyr's own <board>@<revision>/<qualifiers> ordering, and must hold at
# every depth, not just the shallow ones.
@pytest.mark.parametrize(
    ("qualifiers", "expected_qualifiers"),
    [
        pytest.param([], None, id="1_segment_no_qualifiers"),
        pytest.param(["soc"], "/soc", id="2_segments_1_qualifier"),
        pytest.param(["soc", "cpu"], "/soc/cpu", id="3_segments_2_qualifiers"),
        pytest.param(
            ["soc", "sense", "uf2"], "/soc/sense/uf2", id="4_segments_3_qualifiers"
        ),
        pytest.param(
            ["soc", "m33", "w", "mcuboot"],
            "/soc/m33/w/mcuboot",
            id="5_segments_4_qualifiers",
        ),
    ],
)
def test_parse_board_string_with_revision_at_each_depth(
    qualifiers: list[str], expected_qualifiers: str | None
) -> None:
    board = "scobc_a1@1.0.0" + "".join(f"/{q}" for q in qualifiers)
    assert parse_board_string(board) == BoardParts(
        "scobc_a1", "1.0.0", expected_qualifiers
    )


# ---------------------------------------------------------------------------
# resolve_revision / declared_revisions -- number format
# ---------------------------------------------------------------------------


def _write_board_yml(board_dir: Path, revision_yaml: str) -> None:
    board_dir.mkdir(parents=True, exist_ok=True)
    (board_dir / "board.yml").write_text(
        "board:\n  name: my_board\n  vendor: acme\n" + revision_yaml
    )


def test_resolve_revision_no_revision_block(tmp_path: Path) -> None:
    _write_board_yml(tmp_path, "")
    resolved, declares = resolve_revision(tmp_path, "1.0.0")
    assert declares is False
    assert resolved is None
    assert declared_revisions(tmp_path) == []


def test_resolve_revision_number_format_exact_match(tmp_path: Path) -> None:
    _write_board_yml(
        tmp_path,
        "  revision:\n    format: number\n    revisions:\n"
        "      - name: '1'\n      - name: '2'\n",
    )
    resolved, declares = resolve_revision(tmp_path, "2")
    assert declares is True
    assert resolved == "2"
    assert declared_revisions(tmp_path) == ["1", "2"]


def test_resolve_revision_number_format_miss_without_exact(tmp_path: Path) -> None:
    _write_board_yml(
        tmp_path,
        "  revision:\n    format: number\n    revisions:\n"
        "      - name: '1'\n      - name: '2'\n",
    )
    # No revision 0 declared, so nothing lower than "1" resolves.
    resolved, declares = resolve_revision(tmp_path, "0")
    assert declares is True
    assert resolved is None


# ---------------------------------------------------------------------------
# major.minor.patch format -- closest-lower-revision resolution, mirroring
# Zephyr's own board_check_revision() (cmake/modules/extensions.cmake)
# ---------------------------------------------------------------------------


def _mmp_board_yml(tmp_path: Path, *, exact: bool = False) -> Path:
    _write_board_yml(
        tmp_path,
        "  revision:\n    format: major.minor.patch\n"
        f"    exact: {'true' if exact else 'false'}\n"
        "    revisions:\n      - name: '1.4.0'\n      - name: '2.0.0'\n",
    )
    return tmp_path


def test_resolve_revision_mmp_exact_match(tmp_path: Path) -> None:
    board_dir = _mmp_board_yml(tmp_path)
    resolved, declares = resolve_revision(board_dir, "2.0.0")
    assert declares is True
    assert resolved == "2.0.0"


def test_resolve_revision_mmp_closest_lower_match(tmp_path: Path) -> None:
    """Real-world case: actinius_icarus declares 1.4.0/2.0.0 -- a request for 1.9.0
    (between them) resolves to the closest lower revision, 1.4.0, exactly like
    Zephyr's own board_check_revision() without EXACT."""
    board_dir = _mmp_board_yml(tmp_path)
    resolved, declares = resolve_revision(board_dir, "1.9.0")
    assert declares is True
    assert resolved == "1.4.0"


def test_resolve_revision_mmp_loose_typing_matches_full_form(tmp_path: Path) -> None:
    """Zephyr allows trailing zeroes to be omitted on the command line: "2" == "2.0"
    == "2.0.0"."""
    board_dir = _mmp_board_yml(tmp_path)
    resolved, declares = resolve_revision(board_dir, "2")
    assert declares is True
    assert resolved == "2.0.0"


def test_resolve_revision_mmp_below_lowest_declared_fails(tmp_path: Path) -> None:
    board_dir = _mmp_board_yml(tmp_path)
    resolved, declares = resolve_revision(board_dir, "1.0.0")
    assert declares is True
    assert resolved is None


def test_resolve_revision_mmp_exact_true_rejects_non_listed_revision(
    tmp_path: Path,
) -> None:
    board_dir = _mmp_board_yml(tmp_path, exact=True)
    resolved, declares = resolve_revision(board_dir, "1.9.0")
    assert declares is True
    assert resolved is None


def test_resolve_revision_mmp_exact_true_accepts_listed_revision(
    tmp_path: Path,
) -> None:
    board_dir = _mmp_board_yml(tmp_path, exact=True)
    resolved, declares = resolve_revision(board_dir, "1.4.0")
    assert declares is True
    assert resolved == "1.4.0"


# ---------------------------------------------------------------------------
# letter format
# ---------------------------------------------------------------------------


def test_resolve_revision_letter_format_closest_lower_match(tmp_path: Path) -> None:
    _write_board_yml(
        tmp_path,
        "  revision:\n    format: letter\n    revisions:\n"
        "      - name: 'A'\n      - name: 'C'\n",
    )
    resolved, declares = resolve_revision(tmp_path, "B")
    assert declares is True
    assert resolved == "A"


# ---------------------------------------------------------------------------
# Malformed input
# ---------------------------------------------------------------------------


def test_resolve_revision_malformed_requested_value_does_not_raise(
    tmp_path: Path,
) -> None:
    board_dir = _mmp_board_yml(tmp_path)
    resolved, declares = resolve_revision(board_dir, "not-a-version")
    assert declares is True
    assert resolved is None


def test_resolve_revision_missing_board_yml(tmp_path: Path) -> None:
    resolved, declares = resolve_revision(tmp_path, "1.0.0")
    assert declares is False
    assert resolved is None
