import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

noise_ns = cg.esphome_ns.namespace("noise")

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_NOISE")
    cg.add_library("esphome/noise-c", "0.1.21")
    # Enable optimized memzero/memcmp in libsodium instead of volatile byte loops
    cg.add_build_flag("-DHAVE_WEAK_SYMBOLS=1")
    cg.add_build_flag("-DHAVE_INLINE_ASM=1")
    # noise-c pulls noise_rand_bytes (our HWRNG binding, defined in noise.cpp)
    # from the static archive. A consumer such as api may reference no other
    # symbol from noise.cpp at normal log levels, so on the ESP-IDF link (which
    # resolves archives in a group) the member is dropped and noise-c fails to
    # link. Force the linker to keep it. The host toolchain links it without
    # help, and its ld syntax differs (leading underscore), so skip it there.
    if not CORE.is_host:
        cg.add_build_flag("-Wl,-u,noise_rand_bytes")
