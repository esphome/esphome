import esphome.codegen as cg
from esphome.core import CORE, IS_MACOS

CODEOWNERS = ["@esphome/core"]

sha256_ns = cg.esphome_ns.namespace("sha256")


async def to_code(config):
    # Add OpenSSL library for host platform
    if CORE.is_host:
        if IS_MACOS:
            # macOS needs special handling for Homebrew OpenSSL
            cg.add_build_flag("-I/opt/homebrew/opt/openssl/include")
            cg.add_build_flag("-L/opt/homebrew/opt/openssl/lib")
            cg.add_build_flag("-lcrypto")
        else:
            # Linux and other Unix systems usually have OpenSSL in standard paths
            cg.add_build_flag("-lcrypto")
