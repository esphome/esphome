"""Run a user program on the ESP32 ULP coprocessor.

The coprocessor is picked from the variant: the ULP RISC-V core on the
ESP32-S2/S3 and the LP core on the ESP32-C5/C6/P4.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.const import CONF_SOURCES
from esphome.components.esp32 import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_extra_build_file,
    add_idf_install_target,
    add_idf_sdkconfig_option,
    add_src_cmake,
    get_esp32_variant,
    include_builtin_idf_component,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["esp32"]
DOMAIN = "esp32_ulp"

CONF_WAKEUP_PERIOD = "wakeup_period"
CONF_WAKEUP_SOURCES = "wakeup_sources"
CONF_RESERVE_MEMORY = "reserve_memory"
CONF_RUN_ON_BOOT = "run_on_boot"
CONF_WAKEUP_FROM_SLEEP = "wakeup_from_sleep"

BACKEND_RISCV = "riscv"
BACKEND_LP_CORE = "lp_core"

# Which coprocessor each variant has (sdkconfig ULP_COPROC_TYPE choice)
VARIANT_BACKEND = {
    VARIANT_ESP32S2: BACKEND_RISCV,
    VARIANT_ESP32S3: BACKEND_RISCV,
    VARIANT_ESP32C5: BACKEND_LP_CORE,
    VARIANT_ESP32C6: BACKEND_LP_CORE,
    VARIANT_ESP32P4: BACKEND_LP_CORE,
}
# Upper bound of ULP_COPROC_RESERVE_MEM per variant (components/ulp/Kconfig)
RESERVE_MEMORY_MAX = {
    VARIANT_ESP32S2: 8176,
    VARIANT_ESP32S3: 8176,
    VARIANT_ESP32C5: 16352,
    VARIANT_ESP32C6: 16352,
    VARIANT_ESP32P4: 31088,
}
# LP core wake sources (ULP_LP_CORE_WAKEUP_SOURCE_*); the LP timer is
# implied by wakeup_period.
WAKEUP_SOURCES = {
    "hp_cpu": "HP_CPU",
    "lp_io": "LP_IO",
    "lp_uart": "LP_UART",
    "etm": "ETM",
}
# The S2/S3 ULP RISC-V build needs the RISC-V compiler, which the native
# toolchain install skips for Xtensa variants; any RISC-V chip pulls it in.
RISCV_TOOLCHAIN_TARGET = "esp32c3"

ULP_SOURCE_SUFFIXES = (".c", ".S", ".s")
ULP_BUILD_SUBDIR = "ulp"
ULP_APP_NAME = "ulp_main"

esp32_ulp_ns = cg.esphome_ns.namespace("esp32_ulp")
ESP32ULP = esp32_ulp_ns.class_("ESP32ULP", cg.Component)


def get_backend() -> str:
    return VARIANT_BACKEND[get_esp32_variant()]


def _validate_source(value: object) -> Path:
    path = CORE.relative_config_path(cv.file_(value))
    if path.suffix not in ULP_SOURCE_SUFFIXES:
        raise cv.Invalid(
            f"ULP sources must be C or assembly files ({', '.join(ULP_SOURCE_SUFFIXES)})"
        )
    return path


def _validate_sources(value: list[Path]) -> list[Path]:
    names = [path.name for path in value]
    duplicates = {name for name in names if names.count(name) > 1}
    if duplicates:
        raise cv.Invalid(
            f"ULP source file names must be unique, duplicated: {', '.join(sorted(duplicates))}"
        )
    return value


def _validate_variant(config: ConfigType) -> ConfigType:
    variant = get_esp32_variant()
    backend = VARIANT_BACKEND[variant]
    reserve = config[CONF_RESERVE_MEMORY]
    if reserve > RESERVE_MEMORY_MAX[variant]:
        raise cv.Invalid(
            f"{CONF_RESERVE_MEMORY} may be at most {RESERVE_MEMORY_MAX[variant]} bytes on {variant}",
            path=[CONF_RESERVE_MEMORY],
        )
    if backend == BACKEND_RISCV:
        if CONF_WAKEUP_SOURCES in config:
            raise cv.Invalid(
                f"{CONF_WAKEUP_SOURCES} is only available on the LP core ({', '.join(v for v, b in VARIANT_BACKEND.items() if b == BACKEND_LP_CORE)})",
                path=[CONF_WAKEUP_SOURCES],
            )
    elif CONF_WAKEUP_PERIOD not in config and not config.get(CONF_WAKEUP_SOURCES):
        raise cv.Invalid(
            f"The LP core needs {CONF_WAKEUP_PERIOD} or {CONF_WAKEUP_SOURCES} to know when to run"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32ULP),
            cv.Required(CONF_SOURCES): cv.All(
                cv.ensure_list(_validate_source),
                cv.Length(min=1),
                _validate_sources,
            ),
            cv.Optional(CONF_WAKEUP_PERIOD): cv.positive_time_period_microseconds,
            cv.Optional(CONF_WAKEUP_SOURCES): cv.ensure_list(
                cv.one_of(*WAKEUP_SOURCES, lower=True)
            ),
            cv.Optional(CONF_RESERVE_MEMORY, default=4096): cv.int_range(
                min=32, max=max(RESERVE_MEMORY_MAX.values())
            ),
            cv.Optional(CONF_RUN_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_WAKEUP_FROM_SLEEP, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=list(VARIANT_BACKEND),
        msg_prefix="The ULP coprocessor",
    ),
    _validate_variant,
)


def _final_validate(config: ConfigType) -> ConfigType:
    if not CORE.using_toolchain_esp_idf:
        raise cv.Invalid("esp32_ulp requires the native ESP-IDF build")
    if get_backend() == BACKEND_RISCV:
        add_idf_install_target(RISCV_TOOLCHAIN_TARGET)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    backend = get_backend()

    names: list[str] = []
    for path in config[CONF_SOURCES]:
        add_extra_build_file(f"{ULP_BUILD_SUBDIR}/{path.name}", path)
        names.append(path.name)

    add_idf_sdkconfig_option("CONFIG_ULP_COPROC_ENABLED", True)
    if backend == BACKEND_RISCV:
        add_idf_sdkconfig_option("CONFIG_ULP_COPROC_TYPE_RISCV", True)
    else:
        add_idf_sdkconfig_option("CONFIG_ULP_COPROC_TYPE_LP_CORE", True)
    add_idf_sdkconfig_option(
        "CONFIG_ULP_COPROC_RESERVE_MEM", config[CONF_RESERVE_MEMORY]
    )
    include_builtin_idf_component("ulp")

    sources = ";".join(
        f"${{CMAKE_CURRENT_SOURCE_DIR}}/../{ULP_BUILD_SUBDIR}/{name}" for name in names
    )
    # Sources that include the generated ulp_main.h must be compiled after
    # the ULP program is built: main.cpp for lambdas and this component.
    dep_srcs = ";".join(
        f"${{CMAKE_CURRENT_SOURCE_DIR}}/{src}"
        for src in ("main.cpp", "esphome/components/esp32_ulp/esp32_ulp.cpp")
    )
    add_src_cmake(
        f"""
# ULP program (esp32_ulp)
ulp_embed_binary({ULP_APP_NAME} "{sources}" "{dep_srcs}")"""
    )
    # Expose the program's global variables (ulp_<name>) to lambdas.
    cg.add_global(cg.RawStatement(f'#include "{ULP_APP_NAME}.h"'))

    if (period := config.get(CONF_WAKEUP_PERIOD)) is not None:
        cg.add(var.set_wakeup_period(period))
    if backend == BACKEND_LP_CORE:
        sources_expr = [
            f"ULP_LP_CORE_WAKEUP_SOURCE_{WAKEUP_SOURCES[s]}"
            for s in config.get(CONF_WAKEUP_SOURCES, [])
        ]
        if period is not None:
            sources_expr.append("ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER")
        cg.add(var.set_wakeup_sources(cg.RawExpression(" | ".join(sources_expr))))
    cg.add(var.set_run_on_boot(config[CONF_RUN_ON_BOOT]))
    cg.add(var.set_wakeup_from_sleep(config[CONF_WAKEUP_FROM_SLEEP]))
