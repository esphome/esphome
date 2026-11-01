"""Tests for the Nordic board data generator."""

from pathlib import Path
import shutil

import pytest

from esphome.components.nrf52 import boards
from esphome.components.nrf52.generate_boards import (
    END_MARKER,
    GENERATED_MARKER,
    BoardInfo,
    BoardTarget,
    NordicSoc,
    find_targets,
    load_boards,
    load_socs,
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
\t\t\told_spelling: old_spelling {
\t\t\t\tcompatible = "zephyr,cdc-acm-uart";
\t\t\t\tstatus = "ok";
\t\t\t};
\t\t};
\t\t/* Enabled itself, but under a disabled USB controller */
\t\tusbd2: usbd@50000000 {
\t\t\tstatus = "disabled";
\t\t\tunder_disabled: under_disabled {
\t\t\t\tcompatible = "zephyr,cdc-acm-uart";
\t\t\t\tstatus = "okay";
\t\t\t};
\t\t};
\t};
};
"""


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


SOC_YML = """\
family:
  - name: nordic_nrf
    series:
      - name: nrf52
        socs:
          - name: nrf52840
      - name: nrf53
        socs:
          - name: nrf5340
            cpuclusters:
              - name: cpuapp
              - name: cpunet
      - name: nrf54l
        socs:
          - name: nrf54l15
            cpuclusters:
              - name: cpuapp
              - name: cpuflpr
      - name: nrf91
        socs:
          - name: nrf9160
  - name: other
    series:
      - name: stm32
        socs:
          - name: stm32f401
"""


def _socs(tmp_path: Path) -> dict[str, NordicSoc]:
    _write(tmp_path / "soc" / "nordic" / "soc.yml", SOC_YML)
    return load_socs([tmp_path])


def test_load_socs(tmp_path: Path) -> None:
    assert _socs(tmp_path) == {
        "nrf52840": NordicSoc("nrf52", ()),
        "nrf54l15": NordicSoc("nrf54l", ("cpuapp", "cpuflpr")),
    }


def test_board_yml_targets(tmp_path: Path) -> None:
    socs = _socs(tmp_path)
    board = tmp_path / "boards" / "seeed" / "xiao_ble"
    _write(
        board / "board.yml",
        "board:\n  name: xiao_ble\n  socs:\n  - name: nrf52840\n"
        "    variants:\n    - name: sense\n",
    )
    _write(board / "xiao_ble.dts", "")
    _write(board / "xiao_ble_nrf52840_sense.dts", "")
    multi = tmp_path / "boards" / "nordic" / "nrf9160dk"
    _write(
        multi / "board.yml",
        "board:\n  name: nrf9160dk\n  socs:\n  - name: nrf9160\n"
        "    variants:\n    - name: ns\n  - name: nrf52840\n"
        "  revision:\n    default: 0.14.0\n",
    )
    for name in ("nrf52840", "nrf9160", "nrf9160_ns"):
        _write(multi / f"nrf9160dk_{name}.dts", "")
    _write(multi / "nrf9160dk_nrf52840_0_14_0.overlay", "")
    _write(multi / "nrf9160dk_nrf52840_0_7_0.overlay", "")
    clusters = tmp_path / "boards" / "nordic" / "nrf54l15dk"
    _write(
        clusters / "board.yml",
        "board:\n  name: nrf54l15dk\n  socs:\n  - name: nrf54l15\n"
        "    variants:\n    - name: ns\n      cpucluster: cpuapp\n",
    )
    for name in ("cpuapp", "cpuapp_ns", "cpuflpr"):
        _write(clusters / f"nrf54l15dk_nrf54l15_{name}.dts", "")
    # Series that are not listed
    excluded = tmp_path / "boards" / "nordic" / "nrf5340dk"
    _write(
        excluded / "board.yml",
        "board:\n  name: nrf5340dk\n  socs:\n  - name: nrf5340\n",
    )
    _write(excluded / "nrf5340dk_nrf5340_cpuapp.dts", "")
    other = tmp_path / "boards" / "st" / "nucleo"
    _write(
        other / "board.yml", "board:\n  name: nucleo\n  socs:\n  - name: stm32f401\n"
    )
    _write(other / "nucleo.dts", "")

    def target(name: str, soc: str, dts: Path, *overlays: Path) -> BoardTarget:
        # Without a full_name in board.yml the board name is used
        full_name = name.split("/", maxsplit=1)[0]
        return BoardTarget(name, full_name, soc.upper(), dts, overlays)

    assert find_targets(tmp_path / "boards", socs) == [
        target(
            "nrf54l15dk/nrf54l15/cpuapp",
            "nrf54l15",
            clusters / "nrf54l15dk_nrf54l15_cpuapp.dts",
        ),
        target(
            "nrf54l15dk/nrf54l15/cpuapp/ns",
            "nrf54l15",
            clusters / "nrf54l15dk_nrf54l15_cpuapp_ns.dts",
        ),
        target(
            "nrf54l15dk/nrf54l15/cpuflpr",
            "nrf54l15",
            clusters / "nrf54l15dk_nrf54l15_cpuflpr.dts",
        ),
        target(
            "nrf9160dk/nrf52840",
            "nrf52840",
            multi / "nrf9160dk_nrf52840.dts",
            multi / "nrf9160dk_nrf52840_0_14_0.overlay",
        ),
        target("xiao_ble", "nrf52840", board / "xiao_ble.dts"),
        target(
            "xiao_ble/nrf52840/sense",
            "nrf52840",
            board / "xiao_ble_nrf52840_sense.dts",
        ),
    ]


@pytest.mark.skipif(shutil.which("cpp") is None, reason="needs the C preprocessor")
def test_load_boards_reads_enabled_cdc_acm_nodes(tmp_path: Path) -> None:
    zephyr = tmp_path / "zephyr"
    board = zephyr / "boards" / "seeed" / "xiao_ble"
    _write(
        board / "board.yml",
        "board:\n  name: xiao_ble\n  full_name: XIAO BLE\n  socs:\n  - name: nrf52840\n",
    )
    _write(board / "xiao_ble.dts", DTS)
    # Found through the dts/<arch> include directory, as in a build
    _write(zephyr / "dts" / "arm" / "soc.dtsi", "/dts-v1/;\n/ { soc { }; };\n")
    _socs(zephyr)
    broken = zephyr / "boards" / "nordic" / "broken"
    _write(
        broken / "board.yml", "board:\n  name: broken\n  socs:\n  - name: nrf52840\n"
    )
    _write(broken / "broken.dts", "/dts-v1/;\n/ { x = &missing; };\n")

    assert load_boards([zephyr]) == {
        "xiao_ble": BoardInfo(
            "XIAO BLE",
            "NRF52840",
            (
                "/soc/usbd@40027000/board_cdc_acm_uart",
                "/soc/usbd@40027000/old_spelling",
            ),
        )
    }


def test_render_and_replace() -> None:
    generated = render(
        {
            "3.2.0": {"b": BoardInfo('Say "B"', "NRF52840", ("/soc/cdc",))},
            "2.9.2": {"a": BoardInfo("A", "NRF54L15", ())},
        }
    )
    namespace: dict = {}
    exec(generated, namespace)  # noqa: S102
    assert namespace["BOARDS"] == {
        "2.9.2": {"a": {"name": "A", "variant": "NRF54L15"}},
        "3.2.0": {
            "b": {
                "name": 'Say "B"',
                "variant": "NRF52840",
                "cdc_acm": ["/soc/cdc"],
            }
        },
    }
    assert list(namespace["BOARDS"]) == ["2.9.2", "3.2.0"]

    assert generated.startswith(GENERATED_MARKER)
    assert generated.endswith(f"}}\n{END_MARKER}\n")
    content = f"KEEP = 1\n\n{GENERATED_MARKER}OLD = 1\n{END_MARKER}\n"
    assert replace_generated(content, generated) == f"KEEP = 1\n\n{generated}"
    with pytest.raises(ValueError, match="BOARDS generated"):
        replace_generated("KEEP = 1\n", generated)


def test_boards_py_is_formatted_as_generated() -> None:
    content = Path(boards.__file__).read_text(encoding="utf-8")
    generated = content[content.index(GENERATED_MARKER) :]
    versions = {
        version: {
            name: BoardInfo(
                info["name"],
                info["variant"],
                tuple(info.get("cdc_acm", ())),
            )
            for name, info in board_infos.items()
        }
        for version, board_infos in boards.BOARDS.items()
    }
    assert render(versions) == generated


@pytest.mark.parametrize(
    ("framework", "expected"),
    [("2.6.1", "2.9.2"), ("2.9.2", "2.9.2"), ("3.1.0", "2.9.2"), ("9.0.0", "3.4.0")],
)
def test_get_board_info_picks_sdk_version(
    monkeypatch: pytest.MonkeyPatch, framework: str, expected: str
) -> None:
    monkeypatch.setattr(
        boards,
        "BOARDS",
        {v: {"b": {"variant": v}} for v in ("3.4.0", "2.9.2", "3.2.0")},
    )
    assert boards.get_board_info(cv.Version.parse(framework), "b") == {
        "variant": expected
    }
    assert boards.get_board_info(cv.Version.parse(framework), "unknown") is None


@pytest.mark.parametrize(
    ("board", "expected"),
    [
        ("xiao_ble/nrf52840", "xiao_ble"),
        ("xiao_ble/NRF52840", "xiao_ble"),
        ("xiao_ble", "xiao_ble"),
        ("xiao_ble/nrf52840/sense", "xiao_ble/nrf52840/sense"),
        # Wrong SoC for the board
        ("xiao_ble/nrf52832", "xiao_ble/nrf52832"),
        # Listed with its SoC, as it has more than one
        ("nrf9160dk/nrf52840", "nrf9160dk/nrf52840"),
        ("unknown/nrf52840", "unknown/nrf52840"),
    ],
)
def test_short_board_name(
    monkeypatch: pytest.MonkeyPatch, board: str, expected: str
) -> None:
    monkeypatch.setattr(
        boards,
        "BOARDS",
        {
            "2.9.2": {
                "xiao_ble": {"variant": "NRF52840"},
                "xiao_ble/nrf52840/sense": {"variant": "NRF52840"},
                "nrf9160dk/nrf52840": {"variant": "NRF52840"},
            }
        },
    )
    assert boards.short_board_name(board) == expected
