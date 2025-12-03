import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.helpers import IS_MACOS
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

sha256_ns = cg.esphome_ns.namespace("sha256")

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    # Add OpenSSL library for host platform
    if not CORE.is_host:
        return
    if IS_MACOS:
        # macOS needs special handling for Homebrew OpenSSL
        cg.add_build_flag("-I/opt/homebrew/opt/openssl/include")
        cg.add_build_flag("-L/opt/homebrew/opt/openssl/lib")
    cg.add_build_flag("-lcrypto")
