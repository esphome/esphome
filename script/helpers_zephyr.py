"""Load clang-tidy idedata for the nrf52/Zephyr environment.

The compile commands come from a configure-only build of a minimal Zephyr
project using the native sdk-nrf toolchain (see
``esphome.components.nrf52.clang_tidy``); this module extracts the include
paths, defines and compiler flags clang-tidy needs from them.
"""

import json
import os
from pathlib import Path
import re
import shlex
import subprocess


def load_idedata(environment, temp_folder, platformio_ini):
    if explicit := os.environ.get("ESPHOME_ZEPHYR_COMPILE_COMMANDS"):
        compile_commands_path = Path(explicit)
    else:
        from esphome.components.nrf52.clang_tidy import generate_compile_commands

        work_dir = (Path(temp_folder) / f"zephyr-{environment}").resolve()
        compile_commands_path = generate_compile_commands(
            work_dir, Path(platformio_ini)
        )

    if not compile_commands_path.is_file():
        raise RuntimeError(f"compile_commands.json not found: {compile_commands_path}")

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
        ignore_prefixes = ("_ASMLANGUAGE", "NRF_802154_ECB_PRIORITY=")
        return [
            match.replace("\\", "")
            for match in define_pattern.findall(command)
            if not any(match.startswith(prefix) for prefix in ignore_prefixes)
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
        # Extracts CXXFLAGS from the command string, excluding includes and
        # defines. Anchored per token: a substring match would extract a bogus
        # "-format-zero-length" from -Wno-format-zero-length.
        flag_pattern = re.compile(
            r"^(-O[0-3s]|-g|-std=.+|-Wall|-Wextra|-Werror|--.+|-f.+|-m.+|-imacros.+)$"
        )
        flags = []
        tokens = shlex.split(command)
        for i, token in enumerate(tokens):
            if token == "-imacros" and i + 1 < len(tokens):
                flags.append(f"-imacros{tokens[i + 1]}")
            elif flag_pattern.match(token):
                flags.append(token)
        return flags

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

    compile_commands = json.loads(compile_commands_path.read_text(encoding="utf-8"))
    return transform_to_idedata_format(compile_commands)
