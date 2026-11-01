"""Generate the board data in boards.py from the Zephyr board device trees.

Each board's device tree is run through the C preprocessor and parsed with Zephyr's devicetree
library, the same steps a firmware build takes.

Usage: python esphome/components/nrf52/generate_boards.py <sdk-version>=<zephyr-path>[,<nrf-path>] ...
"""

from __future__ import annotations

from collections.abc import Iterable, Iterator
from dataclasses import dataclass
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from devicetree import dtlib
import yaml

CDC_ACM_COMPATIBLE = "zephyr,cdc-acm-uart"
NORDIC_FAMILY = "nordic_nrf"
# SoC series to list boards for: nrf51, nrf52 and every nrf54 series (nrf54l, nrf54h)
SERIES_PREFIXES = ("nrf51", "nrf52", "nrf54")
# Searched in each devicetree root, in this order, followed by dts/<arch> for every
# architecture and then dts (cmake/modules/pre_dt.cmake). Missing directories are skipped
# as the build does.
INCLUDE_SUBDIRS = ("include", "include/zephyr", "dts/common", "dts/vendor")
NOT_ARCH_DTS_DIRS = ("bindings", "common", "vendor")

# Same layout as esp32/boards.py: the generated BOARDS starts at the marker and ends the file
GENERATED_MARKER = '"""\nBOARDS generated with script/generate-nrf52-boards.py\n"""\n'
END_MARKER = "# DO NOT ADD ANYTHING BELOW THIS LINE"


@dataclass(frozen=True)
class BoardTarget:
    """One board target (what `board:` takes) and the device tree file it builds with."""

    name: str
    # full_name from board.yml
    full_name: str
    # The SoC in upper case, such as NRF52840, like the variant in esp32/boards.py
    variant: str
    dts: Path
    # Overlays the build adds on top, for the board's default revision
    overlays: tuple[Path, ...] = ()


@dataclass(frozen=True)
class BoardInfo:
    name: str
    variant: str
    # Paths of the CDC ACM nodes the board enables on its own
    cdc_acm: tuple[str, ...]


@dataclass(frozen=True)
class NordicSoc:
    series: str
    cpuclusters: tuple[str, ...]


def load_socs(roots: Iterable[Path]) -> dict[str, NordicSoc]:
    """Read the Nordic SoCs of SERIES_PREFIXES and their CPU clusters from soc.yml files."""
    socs: dict[str, NordicSoc] = {}
    for root in roots:
        for soc_yml in sorted((root / "soc").glob("**/soc.yml")):
            data = yaml.safe_load(soc_yml.read_text(encoding="utf-8")) or {}
            for family in data.get("family", []):
                if family["name"] != NORDIC_FAMILY:
                    continue
                for series in family.get("series", []):
                    if not series["name"].startswith(SERIES_PREFIXES):
                        continue
                    for soc in series.get("socs", []):
                        clusters = tuple(c["name"] for c in soc.get("cpuclusters", []))
                        socs[soc["name"]] = NordicSoc(series["name"], clusters)
    return socs


def _variant_qualifiers(prefix: str, variants: list[dict]) -> Iterator[str]:
    """Yield prefix/variant for each variant and, below it, its own nested variants."""
    for variant in variants:
        qualifier = f"{prefix}/{variant['name']}"
        yield qualifier
        yield from _variant_qualifiers(qualifier, variant.get("variants", []))


def _soc_qualifiers(soc: dict, cpuclusters: tuple[str, ...]) -> Iterator[str]:
    """Yield the qualifiers of one board SoC, as Zephyr's list_boards.py builds them."""
    name = soc["name"]
    variants = soc.get("variants", [])
    if not cpuclusters:
        yield name
        yield from _variant_qualifiers(name, variants)
        return
    for cluster in cpuclusters:
        yield f"{name}/{cluster}"
        yield from _variant_qualifiers(
            f"{name}/{cluster}",
            [v for v in variants if v.get("cpucluster") == cluster],
        )


def _board_yml_targets(
    board_yml: Path, nordic_socs: dict[str, NordicSoc]
) -> Iterator[BoardTarget]:
    """Yield the targets of a board.yml that build for a Nordic SoC."""
    data = yaml.safe_load(board_yml.read_text(encoding="utf-8"))
    boards = data.get("boards") or [data["board"]]
    for board in boards:
        name = board["name"]
        full_name = board.get("full_name", name)
        socs = board.get("socs", [])
        revision = (board.get("revision") or {}).get("default")
        for soc in socs:
            if (nordic := nordic_socs.get(soc["name"])) is None:
                continue
            for qualifier in _soc_qualifiers(soc, nordic.cpuclusters):
                normalized = qualifier.replace("/", "_")
                dts = board_yml.parent / f"{name}_{normalized}.dts"
                # A board with one SoC may leave the SoC out of its file name
                if len(socs) == 1 and not dts.is_file():
                    without_soc = normalized.removeprefix(soc["name"])
                    dts = board_yml.parent / f"{name}{without_soc}.dts"
                if not dts.is_file():
                    continue
                # ... and out of the target name, when that leaves only the board name
                short = len(socs) == 1 and qualifier == soc["name"]
                target = name if short else f"{name}/{qualifier}"
                overlays: tuple[Path, ...] = ()
                if revision is not None:
                    overlay = dts.with_name(
                        f"{dts.stem}_{str(revision).replace('.', '_')}.overlay"
                    )
                    if overlay.is_file():
                        overlays = (overlay,)
                yield BoardTarget(
                    target,
                    full_name,
                    soc["name"].upper(),
                    dts,
                    overlays,
                )


def find_targets(
    boards_dir: Path, nordic_socs: dict[str, NordicSoc]
) -> list[BoardTarget]:
    """Find the Nordic board targets under the boards directory of a board root."""
    targets: list[BoardTarget] = []
    for board_yml in sorted(boards_dir.glob("**/board.yml")):
        targets.extend(_board_yml_targets(board_yml, nordic_socs))
    return targets


def _include_dirs(roots: Iterable[Path]) -> list[Path]:
    """The include directories a Zephyr build passes to the preprocessor for each devicetree
    root (see cmake/modules/pre_dt.cmake)."""
    dirs: list[Path] = []
    for root in roots:
        dirs.extend(root / sub for sub in INCLUDE_SUBDIRS)
        if (dts := root / "dts").is_dir():
            dirs.extend(
                sorted(
                    arch
                    for arch in dts.iterdir()
                    if arch.is_dir() and arch.name not in NOT_ARCH_DTS_DIRS
                )
            )
            dirs.append(dts)
    return [d for d in dirs if d.is_dir()]


def _is_okay(node: dtlib.Node) -> bool:
    """A node is enabled without a status property or with "okay" (or the older "ok")."""
    status = node.props.get("status")
    return status is None or status.to_string() in ("okay", "ok")


def read_board(target: BoardTarget, roots: list[Path]) -> BoardInfo:
    """Preprocess and parse the board device tree and pick out the data ESPHome uses.

    Raises ValueError when the device tree does not preprocess or parse.
    """
    with tempfile.TemporaryDirectory() as tempdir:
        preprocessed = Path(tempdir) / "board.dts"
        command = ["cpp", "-x", "assembler-with-cpp", "-nostdinc", "-undef", "-P"]
        command.append("-D__DTS__")
        # The board directory is a devicetree root too, so <board>/dts is searched
        for include_dir in _include_dirs([target.dts.parent, *roots]):
            command += ["-isystem", str(include_dir)]
        # The build passes each source with -include, the board file first
        for source in (target.dts, *target.overlays):
            command += ["-include", str(source)]
        command += ["-o", str(preprocessed), "/dev/null"]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode != 0:
            raise ValueError(f"{target.dts}: {result.stderr.strip()}")
        try:
            tree = dtlib.DT(str(preprocessed))
        except dtlib.DTError as err:
            raise ValueError(f"{target.dts}: {err}") from err

    cdc_acm = tuple(
        node.path
        for node in tree.node_iter()
        if "compatible" in node.props
        and CDC_ACM_COMPATIBLE in node.props["compatible"].to_strings()
        and _is_okay(node)
        and all(_is_okay(parent) for parent in _parents(node))
    )
    return BoardInfo(target.full_name, target.variant, cdc_acm)


def _parents(node: dtlib.Node) -> Iterator[dtlib.Node]:
    while (node := node.parent) is not None:
        yield node


def load_boards(roots: list[Path]) -> dict[str, BoardInfo]:
    """Read every Nordic board of one SDK version. roots[0] is the Zephyr tree."""
    nordic_socs = load_socs(roots)
    boards: dict[str, BoardInfo] = {}
    for root in roots:
        for target in find_targets(root / "boards", nordic_socs):
            try:
                info = read_board(target, roots)
            except ValueError as err:
                # The build would fail on such a target too, so it cannot be used anyway
                print(f"Skipping {target.name}: {err}", file=sys.stderr)
                continue
            # A board in a later root replaces one with the same name, as in a build
            boards[target.name] = info
    return boards


def render(versions: dict[str, dict[str, BoardInfo]]) -> str:
    """Render the generated part of boards.py, from GENERATED_MARKER to END_MARKER."""
    lines = [
        GENERATED_MARKER,
        "# nRF Connect SDK version -> board -> data read from the board device tree",
        "BOARDS = {",
    ]
    for version, boards in sorted(versions.items(), key=lambda v: _version_key(v[0])):
        lines.append(f'    "{version}": {{')
        for name, info in sorted(boards.items()):
            lines.append(f'        "{name}": {{')
            lines.append(
                f'            "name": {json.dumps(info.name, ensure_ascii=False)},'
            )
            lines.append(f'            "variant": "{info.variant}",')
            if info.cdc_acm:
                lines.append('            "cdc_acm": [')
                lines.extend(f'                "{path}",' for path in info.cdc_acm)
                lines.append("            ],")
            lines.append("        },")
        lines.append("    },")
    lines.append("}")
    lines.append(END_MARKER)
    return "\n".join(lines) + "\n"


def _version_key(version: str) -> tuple[int, ...]:
    return tuple(int(part) for part in version.split("."))


def replace_generated(content: str, generated: str) -> str:
    """Replace everything from GENERATED_MARKER on with the newly generated part."""
    head, marker, _ = content.partition(GENERATED_MARKER)
    if not marker:
        raise ValueError(f"boards.py has no {GENERATED_MARKER!r}")
    return head + generated


def main() -> None:
    versions: dict[str, dict[str, BoardInfo]] = {}
    for argument in sys.argv[1:]:
        version, paths = argument.split("=", 1)
        versions[version] = load_boards([Path(p) for p in paths.split(",")])
    sys.stdout.write(render(versions))


if __name__ == "__main__":
    main()
