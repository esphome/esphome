"""Tests for esp32_ulp configuration validation and code generation."""

from contextlib import nullcontext
from unittest import mock

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_SOURCES
from esphome.components.esp32 import (
    KEY_IDF_EXTRA_TARGETS,
    KEY_VARIANT,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
from esphome.components.esp32_ulp import (
    CONF_RESERVE_MEMORY,
    CONF_WAKEUP_PERIOD,
    CONF_WAKEUP_SOURCES,
    CONFIG_SCHEMA,
    _final_validate,
    to_code,
)
from esphome.const import KEY_FRAMEWORK_VERSION, PlatformFramework, Toolchain
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

MAIN_C = "ulp/main.c"
COUNTER_H = "ulp/counter.h"
TIMER = {CONF_SOURCES: [MAIN_C], CONF_WAKEUP_PERIOD: "100ms"}


def _set_variant(set_core_config: SetCoreConfigCallable, variant: str) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 6)},
        platform_data={KEY_VARIANT: variant, KEY_IDF_EXTRA_TARGETS: set()},
    )


@pytest.mark.parametrize(
    ("variant", "config", "expectation"),
    [
        pytest.param(VARIANT_ESP32S3, TIMER, nullcontext(), id="s3_timer"),
        pytest.param(VARIANT_ESP32S2, TIMER, nullcontext(), id="s2_timer"),
        pytest.param(VARIANT_ESP32C6, TIMER, nullcontext(), id="c6_timer"),
        pytest.param(VARIANT_ESP32P4, TIMER, nullcontext(), id="p4_timer"),
        pytest.param(
            VARIANT_ESP32C3,
            TIMER,
            pytest.raises(cv.Invalid, match="only available on"),
            id="no_coprocessor",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_RESERVE_MEMORY: 8176},
            nullcontext(),
            id="s3_reserve_max",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_RESERVE_MEMORY: 8177},
            pytest.raises(cv.Invalid, match="at most 8176 bytes on ESP32S3"),
            id="s3_reserve_too_large",
        ),
        pytest.param(
            VARIANT_ESP32C6,
            TIMER | {CONF_RESERVE_MEMORY: 16352},
            nullcontext(),
            id="c6_reserve_max",
        ),
        pytest.param(
            VARIANT_ESP32C6,
            TIMER | {CONF_RESERVE_MEMORY: 16353},
            pytest.raises(cv.Invalid, match="at most 16352 bytes on ESP32C6"),
            id="c6_reserve_too_large",
        ),
        pytest.param(
            VARIANT_ESP32P4,
            TIMER | {CONF_RESERVE_MEMORY: 31088},
            nullcontext(),
            id="p4_reserve_max",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_WAKEUP_SOURCES: ["lp_io"]},
            pytest.raises(cv.Invalid, match="only available on the LP core"),
            id="s3_rejects_wakeup_sources",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            {CONF_SOURCES: [MAIN_C]},
            pytest.raises(cv.Invalid, match="wakeup_period is required"),
            id="s3_requires_wakeup_period",
        ),
        pytest.param(
            VARIANT_ESP32C6,
            {CONF_SOURCES: [MAIN_C]},
            pytest.raises(cv.Invalid, match="needs wakeup_period or wakeup_sources"),
            id="c6_requires_a_trigger",
        ),
        pytest.param(
            VARIANT_ESP32C6,
            {CONF_SOURCES: [MAIN_C], CONF_WAKEUP_SOURCES: ["lp_io", "hp_cpu"]},
            nullcontext(),
            id="c6_event_only",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_WAKEUP_PERIOD: "0s"},
            pytest.raises(cv.Invalid, match="at least 1us"),
            id="period_zero",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_WAKEUP_PERIOD: "72min"},
            pytest.raises(cv.Invalid, match="at most 4294967295us"),
            id="period_over_uint32",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_SOURCES: [MAIN_C, COUNTER_H]},
            nullcontext(),
            id="header_alongside_source",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_SOURCES: [COUNTER_H]},
            pytest.raises(cv.Invalid, match="At least one ULP source"),
            id="headers_only",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_SOURCES: [MAIN_C, MAIN_C]},
            pytest.raises(cv.Invalid, match="must be unique"),
            id="duplicate_names",
        ),
        pytest.param(
            VARIANT_ESP32S3,
            TIMER | {CONF_SOURCES: ["__init__.py"]},
            pytest.raises(cv.Invalid, match="must be C, assembly or header"),
            id="bad_suffix",
        ),
    ],
)
def test_schema_validation(
    set_core_config: SetCoreConfigCallable,
    variant: str,
    config: ConfigType,
    expectation,
) -> None:
    _set_variant(set_core_config, variant)
    with expectation:
        CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    ("variant", "toolchain", "expectation", "extra_targets"),
    [
        pytest.param(
            VARIANT_ESP32S3,
            Toolchain.ESP_IDF,
            nullcontext(),
            {"esp32c3"},
            id="s3_native",
        ),
        pytest.param(
            VARIANT_ESP32C6, Toolchain.ESP_IDF, nullcontext(), set(), id="c6_native"
        ),
        pytest.param(
            VARIANT_ESP32S3,
            Toolchain.PLATFORMIO,
            pytest.raises(cv.Invalid, match="requires the native ESP-IDF build"),
            set(),
            id="platformio_rejected",
        ),
    ],
)
def test_final_validate(
    set_core_config: SetCoreConfigCallable,
    monkeypatch: pytest.MonkeyPatch,
    variant: str,
    toolchain: Toolchain,
    expectation,
    extra_targets: set[str],
) -> None:
    _set_variant(set_core_config, variant)
    monkeypatch.setattr(CORE, "toolchain", toolchain)
    with expectation:
        _final_validate(CONFIG_SCHEMA(TIMER))
    assert CORE.data["esp32"][KEY_IDF_EXTRA_TARGETS] == extra_targets


@pytest.fixture
def codegen() -> dict[str, object]:
    """Run to_code() without codegen and collect what it hands to the esp32 platform."""
    var = mock.MagicMock()
    sdkconfig: dict[str, object] = {}
    build_files: list[str] = []
    cmake: list[str] = []
    with (
        mock.patch("esphome.components.esp32_ulp.cg.new_Pvariable", return_value=var),
        mock.patch(
            "esphome.components.esp32_ulp.cg.register_component",
            new_callable=mock.AsyncMock,
        ),
        mock.patch("esphome.components.esp32_ulp.cg.add_global"),
        mock.patch("esphome.components.esp32_ulp.cg.add"),
        mock.patch(
            "esphome.components.esp32_ulp.add_idf_sdkconfig_option",
            side_effect=sdkconfig.__setitem__,
        ),
        mock.patch(
            "esphome.components.esp32_ulp.add_extra_build_file",
            side_effect=lambda name, path: build_files.append(name),
        ),
        mock.patch("esphome.components.esp32_ulp.include_builtin_idf_component"),
        mock.patch(
            "esphome.components.esp32_ulp.add_src_cmake", side_effect=cmake.append
        ),
    ):
        yield {
            "var": var,
            "sdkconfig": sdkconfig,
            "build_files": build_files,
            "cmake": cmake,
        }


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("variant", "config", "sdkconfig_type", "wakeup_sources"),
    [
        pytest.param(
            VARIANT_ESP32S3, TIMER, "CONFIG_ULP_COPROC_TYPE_RISCV", None, id="s3_riscv"
        ),
        pytest.param(
            VARIANT_ESP32C6,
            TIMER,
            "CONFIG_ULP_COPROC_TYPE_LP_CORE",
            {"ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER"},
            id="c6_timer",
        ),
        pytest.param(
            VARIANT_ESP32P4,
            {CONF_SOURCES: [MAIN_C], CONF_WAKEUP_SOURCES: ["hp_cpu", "lp_io"]},
            "CONFIG_ULP_COPROC_TYPE_LP_CORE",
            {"ULP_LP_CORE_WAKEUP_SOURCE_HP_CPU", "ULP_LP_CORE_WAKEUP_SOURCE_LP_IO"},
            id="p4_event_only",
        ),
    ],
)
async def test_to_code(
    set_core_config: SetCoreConfigCallable,
    codegen: dict[str, object],
    variant: str,
    config: ConfigType,
    sdkconfig_type: str,
    wakeup_sources: set[str] | None,
) -> None:
    _set_variant(set_core_config, variant)
    await to_code(CONFIG_SCHEMA(config))

    sdkconfig = codegen["sdkconfig"]
    assert sdkconfig["CONFIG_ULP_COPROC_ENABLED"] is True
    assert sdkconfig[sdkconfig_type] is True
    assert sdkconfig["CONFIG_ULP_COPROC_RESERVE_MEM"] == 4096
    assert (
        "CONFIG_ULP_COPROC_TYPE_RISCV" in sdkconfig
        or "CONFIG_ULP_COPROC_TYPE_LP_CORE" in sdkconfig
    )
    assert len([k for k in sdkconfig if k.startswith("CONFIG_ULP_COPROC_TYPE_")]) == 1

    var = codegen["var"]
    if wakeup_sources is None:
        var.set_wakeup_sources.assert_not_called()
    else:
        expr = str(var.set_wakeup_sources.call_args[0][0])
        assert set(expr.split(" | ")) == wakeup_sources
    if CONF_WAKEUP_PERIOD in config:
        var.set_wakeup_period.assert_called_once()
    else:
        var.set_wakeup_period.assert_not_called()


@pytest.mark.asyncio
async def test_to_code_copies_headers_but_does_not_compile_them(
    set_core_config: SetCoreConfigCallable, codegen: dict[str, object]
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    await to_code(CONFIG_SCHEMA(TIMER | {CONF_SOURCES: [MAIN_C, COUNTER_H]}))

    assert codegen["build_files"] == ["ulp/main.c", "ulp/counter.h"]
    (cmake,) = codegen["cmake"]
    assert "ulp_embed_binary(ulp_main" in cmake
    assert "ulp/main.c" in cmake
    assert "counter.h" not in cmake
