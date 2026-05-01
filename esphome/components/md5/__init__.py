import esphome.codegen as cg
from esphome.core import CORE
from esphome.helpers import IS_MACOS

CODEOWNERS = ["@esphome/core"]


async def to_code(config):
    cg.add_define("USE_MD5")

    # Add OpenSSL library for host platform
    if CORE.is_host:
        if IS_MACOS:
            # macOS needs special handling for Homebrew OpenSSL
            cg.add_build_flag("-I/opt/homebrew/opt/openssl/include")
            cg.add_build_flag("-L/opt/homebrew/opt/openssl/lib")
        cg.add_build_flag("-lcrypto")
