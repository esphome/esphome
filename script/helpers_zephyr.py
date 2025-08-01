import json
from pathlib import Path
import re
import subprocess


def load_idedata(environment, temp_folder, platformio_ini):
    build_environment = environment.replace("-tidy", "")
    build_dir = Path(temp_folder) / f"build-{build_environment}"
    Path(build_dir).mkdir(exist_ok=True)
    Path(build_dir / "platformio.ini").write_text(
        Path(platformio_ini).read_text(encoding="utf-8"), encoding="utf-8"
    )
    esphome_dir = Path(build_dir / "esphome")
    esphome_dir.mkdir(exist_ok=True)
    Path(esphome_dir / "main.cpp").write_text(
        """
#include <zephyr/kernel.h>
int main() { return 0;}
""",
        encoding="utf-8",
    )
    zephyr_dir = Path(build_dir / "zephyr")
    zephyr_dir.mkdir(exist_ok=True)
    Path(zephyr_dir / "prj.conf").write_text(
        """
CONFIG_NEWLIB_LIBC=y
CONFIG_ADC=y
""",
        encoding="utf-8",
    )
    subprocess.run(["pio", "run", "-e", build_environment, "-d", build_dir], check=True)

    def extract_include_paths(command):
        include_paths = []
        include_pattern = re.compile(r'("-I\s*[^"]+)|(-isystem\s*[^\s]+)|(-I\s*[^\s]+)')
        for match in include_pattern.findall(command):
            split_strings = re.split(
                r"\s*-\s*(?:I|isystem)", list(filter(lambda x: x, match))[0]
            )
            include_paths.append(split_strings[1])
        return include_paths

    def extract_defines(command):
        define_pattern = re.compile(r"-D\s*([^\s]+)")
        return [
            match
            for match in define_pattern.findall(command)
            if match not in ("_ASMLANGUAGE")
        ]

    def find_cxx_path(commands):
        for entry in commands:
            command = entry["command"]
            cxx_path = command.split()[0]
            if not cxx_path.endswith("++"):
                continue
            return cxx_path
        return None

    def get_builtin_include_paths(compiler):
        result = subprocess.run(
            [compiler, "-E", "-x", "c++", "-", "-v"],
            input="",
            text=True,
            stderr=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            check=True,
        )
        include_paths = []
        start_collecting = False
        for line in result.stderr.splitlines():
            if start_collecting:
                if line.startswith(" "):
                    include_paths.append(line.strip())
                else:
                    break
            if "#include <...> search starts here:" in line:
                start_collecting = True
        return include_paths

    def extract_cxx_flags(command):
        # Extracts CXXFLAGS from the command string, excluding includes and defines.
        flag_pattern = re.compile(
            r"(-O[0-3s]|-g|-std=[^\s]+|-Wall|-Wextra|-Werror|--[^\s]+|-f[^\s]+|-m[^\s]+|-imacros\s*[^\s]+)"
        )
        return [
            match.replace("-imacros ", "-imacros")
            for match in flag_pattern.findall(command)
        ]

    def transform_to_idedata_format(compile_commands):
        cxx_path = find_cxx_path(compile_commands)
        idedata = {
            "includes": {
                "toolchain": get_builtin_include_paths(cxx_path),
                "build": set(),
            },
            "defines": set(),
            "cxx_path": cxx_path,
            "cxx_flags": set(),
        }

        for entry in compile_commands:
            command = entry["command"]
            exec = command.split()[0]
            if exec != cxx_path:
                continue

            idedata["includes"]["build"].update(extract_include_paths(command))
            idedata["defines"].update(extract_defines(command))
            idedata["cxx_flags"].update(extract_cxx_flags(command))

        # Convert sets to lists for JSON serialization
        idedata["includes"]["build"] = list(idedata["includes"]["build"])
        idedata["defines"] = list(idedata["defines"])
        idedata["cxx_flags"] = list(idedata["cxx_flags"])

        return idedata

    compile_commands = json.loads(
        Path(
            build_dir / ".pio" / "build" / build_environment / "compile_commands.json"
        ).read_text(encoding="utf-8")
    )
    return transform_to_idedata_format(compile_commands)
