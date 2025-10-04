import platform

import esphome.codegen as cg

DEPENDENCIES = ["rp2040"]


PIOASM_REPO_VERSION = "1.5.0-b"
PIOASM_REPO_BASE = f"https://github.com/earlephilhower/pico-quick-toolchain/releases/download/{PIOASM_REPO_VERSION}"
PIOASM_VERSION = "pioasm-2e6142b.230216"
PIOASM_DOWNLOADS = {
    "linux": {
        "aarch64": f"aarch64-linux-gnu.{PIOASM_VERSION}.tar.gz",
        "armv7l": f"arm-linux-gnueabihf.{PIOASM_VERSION}.tar.gz",
        "x86_64": f"x86_64-linux-gnu.{PIOASM_VERSION}.tar.gz",
    },
    "windows": {
        "amd64": f"x86_64-w64-mingw32.{PIOASM_VERSION}.zip",
    },
    "darwin": {
        "x86_64": f"x86_64-apple-darwin14.{PIOASM_VERSION}.tar.gz",
        "arm64": f"x86_64-apple-darwin14.{PIOASM_VERSION}.tar.gz",
    },
}


async def to_code(config):
    # cg.add_platformio_option(
    #     "platform_packages",
    #     [
    #         "earlephilhower/tool-pioasm-rp2040-earlephilhower",
    #     ],
    # )
    file = PIOASM_DOWNLOADS[platform.system().lower()][platform.machine().lower()]
    cg.add_platformio_option(
        "platform_packages",
        [f"earlephilhower/tool-pioasm-rp2040-earlephilhower@{PIOASM_REPO_BASE}/{file}"],
    )
    cg.add_platformio_option("extra_scripts", ["pre:build_pio.py"])
