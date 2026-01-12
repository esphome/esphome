#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap

import yaml

from esphome.components.nrf52.const import (
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    SUPPORTED_MCUS,
)
from esphome.helpers import write_file_if_changed


class HexInt(int):
    """Integer that displays as hex in repr()"""

    def __repr__(self):
        return f"0x{self:x}"


root = Path(__file__).parent.parent
boards_file_path = root / "esphome" / "components" / "nrf52" / "boards_generated.py"


def get_zephyr_revision_from_ncs(ncs_path: Path) -> str:
    west_yml = ncs_path / "west.yml"
    if not west_yml.is_file():
        raise FileNotFoundError(f"west.yml not found in {ncs_path}")

    with open(west_yml) as f:
        manifest = yaml.safe_load(f)

    for project in manifest.get("manifest", {}).get("projects", []):
        if project.get("name") == "zephyr":
            return project.get("revision")

    raise ValueError("Zephyr revision not found in west.yml")


def checkout_zephyr(ncs_path: Path):
    zephyr_dir = ncs_path / "zephyr"

    # Remove empty directory if it exists
    if zephyr_dir.exists():
        shutil.rmtree(zephyr_dir)

    zephyr_revision = get_zephyr_revision_from_ncs(ncs_path)

    print(f"Cloning Zephyr repository at revision {zephyr_revision}...")
    subprocess.run(
        [
            "git",
            "clone",
            "-q",
            "-c",
            "advice.detachedHead=false",
            "--depth",
            "1",
            "--branch",
            zephyr_revision,
            "https://github.com/nrfconnect/sdk-zephyr",
            str(zephyr_dir),
        ],
        check=True,
    )

    return zephyr_dir


def checkout_nrf_sdk(version: str) -> Path:
    print("NRF SDK not found. Cloning nRF SDK repository...")
    tempdir = Path(tempfile.mkdtemp())
    subprocess.run(
        [
            "git",
            "clone",
            "-q",
            "-c",
            "advice.detachedHead=false",
            "--depth",
            "1",
            "--branch",
            version,
            "https://github.com/nrfconnect/sdk-nrf",
            str(tempdir),
        ],
        check=True,
    )
    checkout_zephyr(tempdir)
    return tempdir


def get_nrf_sdk(ncs_path: Path, version: str) -> Path:
    if ncs_path is None:
        ncs_path = (
            Path.home()
            / ".platformio"
            / "packages"
            / "framework-zephyr"
            / "nrfutil_sdk"
            / version
        )
    if (ncs_path / "zephyr").is_dir():
        return ncs_path
    return checkout_nrf_sdk(version)


def get_edtlib(nrf_sdk_path: Path):
    sys.path.insert(
        0,
        str(nrf_sdk_path / "zephyr" / "scripts" / "dts" / "python-devicetree" / "src"),
    )

    from devicetree import edtlib

    return edtlib


def get_list_boards(nrf_sdk_path: Path):
    sys.path.insert(0, str(nrf_sdk_path / "zephyr" / "scripts"))
    import list_boards

    return list_boards


def preprocess_dts(dts_file, include_paths):
    """Preprocess a DTS file using gcc -E"""
    fd, preprocessed_file = tempfile.mkstemp(suffix=".dts.pre", text=True)
    os.close(fd)

    include_opts = [f"-I{str(path)}" for path in include_paths]

    cmd = (
        [
            "gcc",
            "-E",
            "-nostdinc",
            "-undef",
            "-x",
            "assembler-with-cpp",
            "-D__DTS__",
            "-P",
        ]
        + include_opts
        + ["-o", preprocessed_file, dts_file]
    )

    try:
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        return preprocessed_file
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"DTS preprocessing failed: {e.stderr}")


def parse_dts(edtlib, board_dts_file, include_paths, bindings_dirs):
    include_paths.extend([board_dts_file.parent, board_dts_file.parent / "dts"])
    preprocessed_file = preprocess_dts(board_dts_file, include_paths)
    dt = edtlib.EDT(preprocessed_file, bindings_dirs=bindings_dirs)
    os.remove(preprocessed_file)
    return dt


def qualifiers_to_dts_filename(board_name, qualifier, board_dir):
    segments = qualifier.replace("/", "_")

    # Check if this is a simple SoC name (no variants, just base)
    # This happens when qualifier matches board_name pattern but has no structure
    if "/" not in qualifier and "_" not in segments:
        if (board_dir / f"{board_name}.dts").is_file():
            return board_name
        return f"{board_name}_{segments}"
    # Has variants or cpucluster structure
    return f"{board_name}_{segments}"


def mcu_name_from_qualifier(qualifier):
    for mcu in SUPPORTED_MCUS:
        if mcu in qualifier:
            return mcu
    return None


def get_external_flash_info(dt):
    flash_nodes = dt.compat2nodes.get("jedec,spi-nor", []) + dt.compat2nodes.get(
        "nordic,qspi-nor", []
    )
    if not flash_nodes:
        return None
    flash_node = flash_nodes[0]
    # print(flash_node.labels)
    if "size" not in flash_node.props:
        return None
    if len(flash_node.labels) > 0:
        erase_block_size = (
            flash_node.props["erase-block-size"].val
            if "erase-block-size" in flash_node.props
            else 4096
        )
        return {
            "label": flash_node.labels[0],
            "size": HexInt(flash_node.props["size"].val),
            "erase_block_size": HexInt(erase_block_size),
        }
    return None


def get_bootloader_info(dt):
    partitions_sets = dt.compat2nodes["fixed-partitions"]
    for partitions in partitions_sets:
        if "soc-nv-flash" in partitions.parent.compats:
            partitions = list(partitions.children.values())
            if partitions[0].label.lower() == "softdevice":
                offset_sd = partitions[0].props.get("reg").val
                offset_uf2 = partitions[-1].props.get("reg").val
                return {
                    "type": (
                        BOOTLOADER_ADAFRUIT_NRF52_SD140_V6
                        if offset_sd[1] == 0x26000
                        else BOOTLOADER_ADAFRUIT_NRF52_SD140_V7
                    ),
                    "partitions": [
                        {
                            "name": "softdevice",
                            "address": HexInt(offset_sd[0]),
                            "size": HexInt(offset_sd[1]),
                        },
                        {
                            "name": "adafruit_bootloader",
                            "address": HexInt(offset_uf2[0]),
                            "size": HexInt(offset_uf2[1]),
                        },
                    ],
                }
            if partitions[0].label.lower() == "sam-ba":
                offset = partitions[0].props.get("reg").val
                return {
                    "type": "sam-ba",
                    "partitions": [
                        {
                            "name": "sam-ba",
                            "address": HexInt(offset[0]),
                            "size": HexInt(offset[1]),
                        }
                    ],
                }
    return None


def process_board(board, list_boards_module, include_paths, bindings_dirs, edtlib):
    qualifiers = list_boards_module.board_v2_qualifiers(board)
    for q in qualifiers:
        mcu = mcu_name_from_qualifier(q)
        if not mcu:
            continue
        filename = qualifiers_to_dts_filename(board.name, q, board.dir)
        dts_file = board.dir / f"{filename}.dts"
        # yaml_file = board.dir / f"{filename}.yaml"
        if not dts_file.is_file():  # or not yaml_file.is_file():
            print(f"  DTS file {dts_file} does not exist, skipping...")
            continue

        edt = parse_dts(edtlib, dts_file, include_paths, bindings_dirs)
        return {
            "name": board.full_name if board.full_name else board.name,
            "mcu": mcu,
            "vendor": board.vendor if board.vendor else "Unknown",
            "external_flash": get_external_flash_info(edt),
            "bootloader": get_bootloader_info(edt),
        }
    return None


def process_boards(boards, nrf_sdk_path, list_boards_module):
    zephyr_base = nrf_sdk_path / "zephyr"
    include_paths = [
        zephyr_base / "include",
        zephyr_base / "include" / "zephyr",
        zephyr_base / "dts" / "common",
        zephyr_base / "dts" / "vendor",
        zephyr_base / "dts",
        zephyr_base / "dts" / "arm",
        zephyr_base / "dts" / "riscv",  # For nrf54
        nrf_sdk_path / "nrf" / "dts",
        nrf_sdk_path / "nrf" / "dts" / "arm",
        nrf_sdk_path / "nrf" / "dts" / "riscv",
        nrf_sdk_path / "nrf" / "dts" / "common",
    ]
    bindings_dirs = [
        str(zephyr_base / "dts" / "bindings"),
        str(nrf_sdk_path / "nrf" / "dts" / "bindings"),
    ]
    edtlib = get_edtlib(nrf_sdk_path)
    result = {}
    for board in boards.values():
        if "native" in board.dir.parts:
            # skip native simulated boards
            continue
        if {soc.name for soc in board.socs}.isdisjoint(SUPPORTED_MCUS):
            continue
        info = process_board(
            board, list_boards_module, include_paths, bindings_dirs, edtlib
        )
        if info:
            result[board.name] = info
    return result


def list_boards(nrf_sdk_path: Path, list_boards_module):
    list_args = argparse.Namespace(
        arch_roots=[],
        soc_roots=[nrf_sdk_path / "zephyr", nrf_sdk_path / "nrf"],
        board_roots=[nrf_sdk_path / "zephyr", nrf_sdk_path / "nrf"],
        board=None,
        board_dir=[],
    )
    return list_boards_module.find_v2_boards(list_args)


def generate_python(boards):
    sorted_boards = dict(sorted(boards.items()))
    template = textwrap.dedent(
        f"""
            # Auto-generated file. Do not edit.

            BOARDS = {sorted_boards!r}
        """
    )
    result = subprocess.run(
        [sys.executable, "-m", "ruff", "format", "-"],
        input=template,
        text=True,
        capture_output=True,
        check=True,
    )

    if result.returncode != 0:
        print("Error formatting generated code with ruff: ", result.stderr)
        sys.exit(1)

    return result.stdout


def main(check: bool, ncs_path: Path, version: str | None = None):
    if not version:
        version = "v2.9.2"
    nrf_sdk_path = get_nrf_sdk(ncs_path, version)
    is_temp = str(nrf_sdk_path).startswith(tempfile.gettempdir())

    try:
        list_boards_module = get_list_boards(nrf_sdk_path)
        all_boards = list_boards(nrf_sdk_path, list_boards_module)
        boards = process_boards(all_boards, nrf_sdk_path, list_boards_module)
        content = generate_python(boards)
        existing_content = (
            boards_file_path.read_text() if boards_file_path.is_file() else ""
        )
        if check:
            if existing_content != content:
                print("boards_generated.py file is not up to date.")
                print("Please run `script/generate-nrf52-boards.py`")
                sys.exit(1)
            print("boards_generated.py file is up to date")
        elif write_file_if_changed(boards_file_path, content):
            print("nRF52 boards updated successfully.")
    finally:
        if is_temp and nrf_sdk_path.exists():
            print(f"Cleaning up temporary directory {nrf_sdk_path}...")
            shutil.rmtree(nrf_sdk_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        help="Check if the boards_generated.py file is up to date.",
        action="store_true",
    )
    parser.add_argument(
        "--ncs-path",
        help="Path to a local checkout of the ncs repository. If not provided, the ~/.platformio checkout will be used or the repository will be cloned.",
        default=None,
        type=Path,
    )
    parser.add_argument(
        "--ncs-version",
        help="Version of the ncs SDK to use (e.g., 'v2.9.2'). If not provided, the recommended version will be used.",
        default=None,
        type=str,
    )
    args = parser.parse_args()
    main(args.check, args.ncs_path, args.ncs_version)
