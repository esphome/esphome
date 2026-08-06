import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import ZEPHYR_VARIANT_RP2040
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_DEFAULT_BOARD = "waveshare_rp2040_zero"

_ADVANCED_SCHEMA = cv.Schema({})

VARIANT_NAME = ZEPHYR_VARIANT_RP2040
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="rpi_pico",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    soc="rp2040",
    swap_methods=frozenset(),
    gpio_port_width=30,
    pwm_node_labels=["pwm"],
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    framework = config[CONF_FRAMEWORK]
    version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "rp2040", config, "RP2040 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        "",
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_overlay,
        zephyr_add_prj_conf,
        zephyr_set_prj_conf_override,
        zephyr_setup_preferences,
        zephyr_to_code,
    )

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_RP2040")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RP2040")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)
    zephyr_add_prj_conf("TEST_RANDOM_GENERATOR", True)

    zephyr_set_prj_conf_override("ENTROPY_GENERATOR", False)

    # Reserve space for NVS storage partition at end of flash.
    # Layout targets 2 MB flash (standard on most RP2040 boards including
    # rpi_pico, rp2040_zero, and waveshare variants). Boards with 4 MB or
    # 8 MB flash must provide their own partition table via zephyr: overlays:
    # in the ESPHome YAML config.
    zephyr_add_overlay(
        """
        /delete-node/ &code_partition;

        &flash0 {
            partitions {
                code_partition: partition@100 {
                    reg = <0x100 0x1f0000>;
                    read-only;
                };
                storage_partition: partition@1f0100 {
                    compatible = "zephyr,mapped-partition";
                    reg = <0x1f0100 0xff00>;
                };
            };
        };

        / {
            chosen {
                zephyr,code-partition = &code_partition;
            };
        };
        """
    )
