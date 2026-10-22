import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    CONF_WIFI,
    ThreadModel,
    Toolchain,
)
from esphome.core import CORE
from esphome.types import ConfigType

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    ZEPHYR_VARIANT_NRF5340,
)
from ..partitions import BootLayout
from . import (
    MAINLINE,
    NCS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

_DEFAULT_BOARD = "nrf7002dk/nrf5340/cpuapp"

# Only the application core is built. BLE, 802.15.4, OpenThread and Zigbee need the
# network core image, which isn't built yet; Wi-Fi (nRF7002 over QSPI) doesn't.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        # The board DTS always links the app into slot0, so only MCUboot is offered.
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

# SAADC inputs: P0.04-P0.07 = AIN0-AIN3, P0.25-P0.28 = AIN4-AIN7 (not nRF52's layout).
_ADC_AIN_MAP = {
    4: "AIN0",
    5: "AIN1",
    6: "AIN2",
    7: "AIN3",
    25: "AIN4",
    26: "AIN5",
    27: "AIN6",
    28: "AIN7",
}

# gpio0 (0-31) + gpio1 (32-47), any pin for any UART/SPI signal (NRF_PSEL).
_GPIO_MATRIX_PINS = frozenset(range(48))

VARIANT_NAME = ZEPHYR_VARIANT_NRF5340
VARIANT = ZephyrVariant(
    boot=BootLayout(0x0),
    sdk=NCS,
    sdk_name="ncs",
    alt_sdks={"zephyr": MAINLINE},
    family="nordic",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset({"wifi"}),
    # wlan0 is the nordic,wlan interface node, not the nRF7002 QSPI device.
    transport_drivers={"wifi": ("WIFI_NRF70", "wlan0")},
    # Firmware patch for the driver's normal (STA/AP) mode.
    transport_blobs={
        "wifi": ("nrf_wifi", ".*default/nrf70\\.bin", ".blobs_nrf_wifi_ready")
    },
    soc="nrf5340",
    qualifier="cpuapp",
    # Mainline's rng_hci needs the network core; NCS uses the CryptoCell-312.
    rng_needs_second_core=True,
    # No scratch partition in the board's flash layout.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    uart_node_labels={},
    pwm_node_labels=["pwm0", "pwm1", "pwm2", "pwm3"],
    uart_valid_pins={"tx": _GPIO_MATRIX_PINS, "rx": _GPIO_MATRIX_PINS},
    spi_valid_pins={
        "clk": _GPIO_MATRIX_PINS,
        "mosi": _GPIO_MATRIX_PINS,
        "miso": _GPIO_MATRIX_PINS,
    },
)


def config_schema(config: ConfigType) -> ConfigType:
    _LOGGER.warning(
        "nRF5340 is dual-core, but only the application core is built here -- "
        "BLE, 802.15.4, OpenThread and Zigbee (which need the network core) are not "
        "supported yet. Only Wi-Fi (via the onboard nRF7002) and the application "
        "core's own peripherals (GPIO, UART, I2C, SPI, PWM, ADC) are available."
    )
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "nrf5340", config, "nRF5340 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        config[CONF_ADVANCED][CONF_BOOTLOADER],
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_framework_type,
        zephyr_setup_preferences,
    )

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NRF5340")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF5340")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
    if zephyr_framework_type() == "ncs" and CORE.config.get(CONF_WIFI) is not None:
        # NCS sysbuild forces the image's CONFIG_WIFI_NRF70 off unless this is set.
        zephyr_add_sysbuild_conf("WIFI_NRF70", True)
