import esphome.codegen as cg
from esphome.core import CORE, EsphomeError
from esphome.helpers import IS_MACOS

CODEOWNERS = ["@esphome/core"]


async def to_code(config):
    if CORE.is_nrf52:
        raise EsphomeError(
            "md5 is not implemented for nrf52 (pulled in by hmac_md5 or ota)"
        )

    cg.add_define("USE_MD5")

    # Add OpenSSL library for host platform
    if CORE.is_host:
        if IS_MACOS:
            # macOS needs special handling for Homebrew OpenSSL
            cg.add_build_flag("-I/opt/homebrew/opt/openssl/include")
            cg.add_build_flag("-L/opt/homebrew/opt/openssl/lib")
        cg.add_build_flag("-lcrypto")
    if CORE.is_zephyr:
        from esphome.components.zephyr import zephyr_add_prj_conf

        zephyr_add_prj_conf("MBEDTLS", True)
        zephyr_add_prj_conf("MBEDTLS_BUILTIN", True)
        # All platform:zephyr variants (native_sim/esp32_h2 >= 4.4.0) are above
        # the PSA crypto threshold.
        zephyr_add_prj_conf("PSA_CRYPTO", True)
        zephyr_add_prj_conf("MBEDTLS_MD_C", True)
        # PSA_WANT_ALG_MD5's prompt is hidden by MBEDTLS_PROMPTLESS (set elsewhere by
        # OpenThread), so a direct CONFIG_PSA_WANT_ALG_MD5=y is silently dropped.
        # MBEDTLS_HASH_ALL_ENABLED is the only unguarded Kconfig that selects it.
        zephyr_add_prj_conf("MBEDTLS_HASH_ALL_ENABLED", True)
