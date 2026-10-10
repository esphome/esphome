"""Run a user program on the ESP32-S2/S3 ULP RISC-V coprocessor."""

from pathlib import Path

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_extra_build_file,
    add_idf_sdkconfig_option,
    add_src_cmake,
    include_builtin_idf_component,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["esp32"]
DOMAIN = "esp32_ulp"

CONF_SOURCES = "sources"
CONF_WAKEUP_PERIOD = "wakeup_period"
CONF_RESERVE_MEMORY = "reserve_memory"
CONF_RUN_ON_BOOT = "run_on_boot"
CONF_WAKEUP_FROM_SLEEP = "wakeup_from_sleep"

ULP_SOURCE_SUFFIXES = (".c", ".S", ".s")
ULP_BUILD_SUBDIR = "ulp"
ULP_APP_NAME = "ulp_main"

esp32_ulp_ns = cg.esphome_ns.namespace("esp32_ulp")
ESP32ULP = esp32_ulp_ns.class_("ESP32ULP", cg.Component)


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
            cv.Optional(CONF_RESERVE_MEMORY, default=4096): cv.int_range(
                min=32, max=8176
            ),
            cv.Optional(CONF_RUN_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_WAKEUP_FROM_SLEEP, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=[VARIANT_ESP32S2, VARIANT_ESP32S3],
        msg_prefix="The ULP RISC-V coprocessor",
    ),
)


def _final_validate(config: ConfigType) -> ConfigType:
    if not CORE.using_toolchain_esp_idf:
        raise cv.Invalid("esp32_ulp requires the native ESP-IDF build")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    names: list[str] = []
    for path in config[CONF_SOURCES]:
        add_extra_build_file(f"{ULP_BUILD_SUBDIR}/{path.name}", path)
        names.append(path.name)

    add_idf_sdkconfig_option("CONFIG_ULP_COPROC_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_ULP_COPROC_TYPE_RISCV", True)
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
# ULP RISC-V program (esp32_ulp)
ulp_embed_binary({ULP_APP_NAME} "{sources}" "{dep_srcs}")"""
    )
    # Expose the program's global variables (ulp_<name>) to lambdas.
    cg.add_global(cg.RawStatement(f'#include "{ULP_APP_NAME}.h"'))

    if (period := config.get(CONF_WAKEUP_PERIOD)) is not None:
        cg.add(var.set_wakeup_period(period))
    cg.add(var.set_run_on_boot(config[CONF_RUN_ON_BOOT]))
    cg.add(var.set_wakeup_from_sleep(config[CONF_WAKEUP_FROM_SLEEP]))
