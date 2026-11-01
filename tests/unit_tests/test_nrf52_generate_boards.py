"""Tests for the nRF52 board data generator."""

from pathlib import Path
import shutil

import pytest

from esphome.components.nrf52 import boards
from esphome.components.nrf52.generate_boards import (
    GENERATED_MARKER,
    BoardInfo,
    BoardTarget,
    find_targets,
    load_boards,
    render,
    replace_generated,
)
import esphome.config_validation as cv

DTS = """\
#include <soc.dtsi>

/ {
\tsoc {
\t\tusbd: usbd@40027000 {
\t\t\tboard_cdc_acm_uart: board_cdc_acm_uart {
\t\t\t\tcompatible = "zephyr,cdc-acm-uart";
\t\t\t};
\t\t\toff: off {
\t\t\t\tcompatible = "zephyr,cdc-acm-uart";
\t\t\t\tstatus = "disabled";
\t\t\t};
\t\t};
\t};
};
"""


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def test_hwmv2_targets(tmp_path: Path) -> None:
    board = tmp_path / "boards" / "seeed" / "xiao_ble"
    _write(
        board / "board.yml",
        "board:\n  name: xiao_ble\n  socs:\n  - name: nrf52840\n"
        "    variants:\n    - name: sense\n",
    )
    _write(board / "xiao_ble.dts", "")
    _write(board / "xiao_ble_nrf52840_sense.dts", "")
    multi = tmp_path / "boards" / "nordic" / "thingy91"
    _write(
        multi / "board.yml",
        "board:\n  name: thingy91\n  socs:\n  - name: nrf9160\n  - name: nrf52840\n",
    )
    _write(multi / "thingy91_nrf52840.dts", "")
    _write(multi / "thingy91_nrf9160.dts", "")

    assert find_targets(tmp_path / "boards") == [
        BoardTarget("thingy91/nrf52840", "nrf52840", multi / "thingy91_nrf52840.dts"),
        BoardTarget("xiao_ble", "nrf52840", board / "xiao_ble.dts"),
        BoardTarget(
            "xiao_ble/nrf52840/sense",
            "nrf52840",
            board / "xiao_ble_nrf52840_sense.dts",
        ),
    ]


def test_hwmv1_targets(tmp_path: Path) -> None:
    board = tmp_path / "boards" / "arm" / "xiao_ble"
    _write(board / "xiao_ble.dts", "")
    _write(board / "xiao_ble_defconfig", "CONFIG_SOC_NRF52840_QIAA=y\n")
    other = tmp_path / "boards" / "arm" / "nucleo"
    _write(other / "nucleo.dts", "")
    _write(other / "nucleo_defconfig", "CONFIG_SOC_STM32F401XE=y\n")

    assert find_targets(tmp_path / "boards") == [
        BoardTarget("xiao_ble", "nrf52840", board / "xiao_ble.dts")
    ]


@pytest.mark.skipif(shutil.which("cpp") is None, reason="needs the C preprocessor")
def test_load_boards_reads_enabled_cdc_acm_nodes(tmp_path: Path) -> None:
    zephyr = tmp_path / "zephyr"
    board = zephyr / "boards" / "seeed" / "xiao_ble"
    _write(
        board / "board.yml", "board:\n  name: xiao_ble\n  socs:\n  - name: nrf52840\n"
    )
    _write(board / "xiao_ble.dts", DTS)
    # Found through the dts include directory, as in a build
    _write(zephyr / "dts" / "soc.dtsi", "/dts-v1/;\n/ { soc { }; };\n")

    assert load_boards([zephyr]) == {
        "xiao_ble": BoardInfo("nrf52840", ("/soc/usbd@40027000/board_cdc_acm_uart",))
    }


def test_render_and_replace() -> None:
    generated = render(
        {
            "3.2.0": {"b": BoardInfo("nrf52840", ("/soc/usbd/cdc",))},
            "2.9.2": {"a": BoardInfo("nrf52832", ())},
        }
    )
    namespace: dict = {}
    exec(generated, namespace)  # noqa: S102
    assert namespace["BOARDS"] == {
        "2.9.2": {"a": {"soc": "nrf52832"}},
        "3.2.0": {"b": {"soc": "nrf52840", "cdc_acm": ["/soc/usbd/cdc"]}},
    }
    assert list(namespace["BOARDS"]) == ["2.9.2", "3.2.0"]

    content = f"KEEP = 1\n\n{GENERATED_MARKER}\nOLD = 1\n"
    assert replace_generated(content, generated) == f"KEEP = 1\n\n{generated}"
    with pytest.raises(ValueError, match="DO NOT EDIT"):
        replace_generated("KEEP = 1\n", generated)


def test_boards_py_is_formatted_as_generated() -> None:
    content = Path(boards.__file__).read_text(encoding="utf-8")
    generated = content[content.index(GENERATED_MARKER) :]
    versions = {
        version: {
            name: BoardInfo(info["soc"], tuple(info.get("cdc_acm", ())))
            for name, info in board_infos.items()
        }
        for version, board_infos in boards.BOARDS.items()
    }
    assert render(versions) == generated


@pytest.mark.parametrize(
    ("framework", "expected"),
    [("2.0.0", "2.6.1"), ("2.9.2", "2.9.2"), ("3.1.0", "2.9.2"), ("9.0.0", "3.4.0")],
)
def test_get_board_picks_sdk_version(
    monkeypatch: pytest.MonkeyPatch, framework: str, expected: str
) -> None:
    monkeypatch.setattr(
        boards,
        "BOARDS",
        {v: {"b": {"soc": v}} for v in ("3.4.0", "2.6.1", "2.9.2", "3.2.0")},
    )
    assert boards.get_board(cv.Version.parse(framework), "b") == {"soc": expected}
    assert boards.get_board(cv.Version.parse(framework), "unknown") is None
