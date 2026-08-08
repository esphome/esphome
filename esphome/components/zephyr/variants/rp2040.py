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

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_RP2040
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
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=30,
    pwm_node_labels=["pwm"],
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "rp2040", config, "RP2040 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        BOOTLOADER_MCUBOOT,
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
        zephyr_add_sysbuild_conf,
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

    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")

    # RP2040 boards ship with a flat code_partition layout in their DTS, not
    # MCUboot's boot / slot0 / slot1 / storage structure.  Provide a default
    # 2 MB overlay that both the app and MCUboot can see.  Boards with 4 MB or
    # 8 MB flash must override the entire partition table via zephyr: overlays:
    # in their ESPHome YAML.
    _partition_overlay = """
        &code_partition {
            reg = <0x0 0x0>;
        };

        &flash0 {
            partitions {
                boot_partition: partition@100 {
                    label = "mcuboot";
                    reg = <0x100 0xFF00>;
                    read-only;
                };
                slot0_partition: partition@10000 {
                    label = "image-0";
                    reg = <0x10000 0xF0000>;
                };
                slot1_partition: partition@100000 {
                    label = "image-1";
                    reg = <0x100000 0xF0000>;
                };
                storage_partition: partition@1F0000 {
                    compatible = "zephyr,mapped-partition";
                    label = "storage";
                    reg = <0x1F0000 0x10000>;
                };
            };
        };

        / {
            chosen {
                zephyr,code-partition = &slot0_partition;
            };
        };
    """
    zephyr_add_overlay(_partition_overlay)

    _mcuboot_partition_overlay = """
        &code_partition {
            reg = <0x0 0x0>;
        };

        &flash0 {
            partitions {
                boot_partition: partition@100 {
                    label = "mcuboot";
                    reg = <0x100 0xFF00>;
                    read-only;
                };
                slot0_partition: partition@10000 {
                    label = "image-0";
                    reg = <0x10000 0xF0000>;
                };
                slot1_partition: partition@100000 {
                    label = "image-1";
                    reg = <0x100000 0xF0000>;
                };
                storage_partition: partition@1F0000 {
                    compatible = "zephyr,mapped-partition";
                    label = "storage";
                    reg = <0x1F0000 0x10000>;
                };
            };
        };
    """
    zephyr_add_overlay(_mcuboot_partition_overlay, image="mcuboot")
