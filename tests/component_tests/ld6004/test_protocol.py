"""Compile and execute the production protocol without hardware dependencies."""

from pathlib import Path
import subprocess
import sys

import pytest


def test_protocol(tmp_path: Path) -> None:
    root: Path = Path(__file__).resolve().parents[3]
    binary: Path = tmp_path / (
        "protocol.exe" if sys.platform == "win32" else "protocol"
    )
    sanitizers: list[str] = (
        []
        if sys.platform == "win32"
        else ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    )
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            *sanitizers,
            "-I",
            str(root),
            str(Path(__file__).with_name("protocol_test.cpp")),
            str(root / "esphome/components/ld6004/protocol.cpp"),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)


@pytest.mark.skipif(
    sys.platform == "win32",
    reason="ESPHome USE_HOST requires POSIX sys/select.h, unavailable in Windows MinGW",
)
@pytest.mark.parametrize(
    "features,ld6004_entities",
    [
        ((), False),
        (("SENSOR",), False),
        (("SENSOR",), True),
        (("BINARY_SENSOR",), True),
        (
            ("SENSOR", "BINARY_SENSOR", "TEXT_SENSOR", "NUMBER", "SELECT", "SWITCH"),
            True,
        ),
    ],
)
def test_optional_platforms(
    tmp_path: Path, features: tuple[str, ...], ld6004_entities: bool
) -> None:
    root: Path = Path(__file__).resolve().parents[3]
    defines: Path = tmp_path / "esphome/core/defines.h"
    defines.parent.mkdir(parents=True)
    defines.write_text(
        "#pragma once\n#define USE_HOST\n#define ESPHOME_COMPONENT_COUNT 1\n"
        + "".join(
            f"#define USE_{feature}\n#define ESPHOME_ENTITY_{feature}_COUNT 1\n"
            + (f"#define USE_LD6004_{feature}\n" if ld6004_entities else "")
            for feature in features
        ),
        encoding="utf-8",
    )
    probe: Path = tmp_path / "entity_gating.cpp"
    probe.write_text(
        '#include "esphome/components/ld6004/ld6004.h"\n'
        "template<typename T> concept HasSensor = requires { &T::set_sensor; };\n"
        f"static_assert(HasSensor<esphome::ld6004::LD6004Component> == "
        f"{str(ld6004_entities and 'SENSOR' in features).lower()});\n",
        encoding="utf-8",
    )
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-fsyntax-only",
            "-I",
            str(tmp_path),
            "-I",
            str(root),
            str(root / "esphome/components/ld6004/ld6004.cpp"),
            str(probe),
        ],
        check=True,
    )


@pytest.mark.skipif(sys.platform == "win32", reason="USE_HOST requires POSIX headers")
def test_unrelated_sensor_storage(tmp_path: Path) -> None:
    root: Path = Path(__file__).resolve().parents[3]
    defines: Path = tmp_path / "esphome/core/defines.h"
    defines.parent.mkdir(parents=True)
    probe: Path = tmp_path / "storage.cpp"
    probe.write_text(
        '#include "esphome/components/ld6004/ld6004.h"\n'
        "#include <cstdio>\n"
        'int main() { printf("%zu", sizeof(esphome::ld6004::LD6004Component)); }\n',
        encoding="utf-8",
    )
    binary: Path = tmp_path / "storage"
    sizes: list[int] = []
    for flags in ("", "#define USE_SENSOR\n#define ESPHOME_ENTITY_SENSOR_COUNT 1\n"):
        defines.write_text(
            "#pragma once\n#define USE_HOST\n#define ESPHOME_COMPONENT_COUNT 1\n"
            + flags,
            encoding="utf-8",
        )
        subprocess.run(
            [
                "c++",
                "-std=c++20",
                "-I",
                str(tmp_path),
                "-I",
                str(root),
                str(probe),
                "-o",
                str(binary),
            ],
            check=True,
        )
        sizes.append(int(subprocess.check_output([str(binary)], text=True)))
    assert sizes[0] == sizes[1]
