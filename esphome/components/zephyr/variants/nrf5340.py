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

# nRF5340 is dual-core (Cortex-M33 application core + a separate Cortex-M33 network
# core for BLE/802.15.4, connected over IPC). Only the application core is built and
# flashed here -- the network core image (and anything that needs it: BLE, 802.15.4,
# OpenThread, Zigbee) is not wired up yet, so those transports are deliberately absent
# below. Wi-Fi (via the onboard nRF7002, QSPI-attached) runs entirely on the
# application core and needs no network-core image.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        # The board's own devicetree partition map (nrf5340_cpuapp_partition.dtsi)
        # unconditionally links the app against slot0_partition at 0x10000, with
        # MCUboot occupying 0x0-0x10000 -- there's no "no bootloader" layout to fall
        # back to, unlike rp2040/rp2350/nrf54l15. So, unlike those variants, only
        # BOOTLOADER_MCUBOOT is offered here.
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

# GPIO -> nRF5340 SAADC analog-input name (fixed silicon fact, nRF5340 Product
# Specification). AIN0-AIN5 confirmed directly against this board's own arduino_adc
# node comments (nrf5340_cpuapp_common.dtsi: "A0 = P0.4 = AIN0" ... "A5 = P0.26 =
# AIN5"); AIN6/AIN7 follow the same contiguous P0.27/P0.28 block per the official
# pin-assignment table (P0.04..P0.07 = AIN0..AIN3, P0.25..P0.28 = AIN4..AIN7) -- a
# different GPIO layout from the nRF52840 (see variants/nrf52.py's own _ADC_AIN_MAP).
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

# nrf5340_cpuapp_peripherals.dtsi: gpio0 (32 pins, flat 0-31) + gpio1 (ngpios=<16>,
# flat 32-47). Free-mux via NRF_PSEL, same as nRF52 -- no fixed-function subset to
# exclude, shared by every free-mux GPIO signal (UART, SPI).
_GPIO_MATRIX_PINS = frozenset(range(48))

VARIANT_NAME = ZEPHYR_VARIANT_NRF5340
VARIANT = ZephyrVariant(
    # MCUboot is mandatory (see _ADVANCED_SCHEMA above) and always starts at 0x0.
    boot=BootLayout(0x0),
    # NCS (nRF Connect SDK) is the default, same reasoning as every other Nordic
    # variant here: it's where real hardware support/testing effort for this chip
    # concentrates. Mainline Zephyr stays available as an alternate.
    sdk=NCS,
    sdk_name="ncs",
    alt_sdks={"zephyr": MAINLINE},
    family="nordic",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # Wi-Fi only -- see the dual-core note on _ADVANCED_SCHEMA above for why BLE/
    # 802.15.4/OpenThread/Zigbee (all of which need the network-core image) aren't
    # offered yet.
    transports=frozenset({"wifi"}),
    # "WIFI_NRF70" is the nRF70-series Wi-Fi driver's top-level Kconfig symbol
    # (drivers/wifi/nrf_wifi/Kconfig.nrfwifi); it already defaults to y once the
    # board's nRF7002 QSPI devicetree node is enabled (nrf70_common.dtsi, status
    # "okay" on this board already), so enabling it here is just explicit intent,
    # matching every other transport_drivers entry. "wlan0" is the actual Wi-Fi
    # network-interface child node (compatible "nordic,wlan") the chosen
    # `zephyr,wifi` property points at -- not the nRF7002 QSPI device node itself.
    transport_drivers={"wifi": ("WIFI_NRF70", "wlan0")},
    # The nRF70 driver needs a firmware patch blob loaded over QSPI at runtime.
    # "nrf_wifi" is the west module (modules/lib/nrf_wifi); its module.yml lists one
    # blob per operating mode (default/scan_only/radio_test/system_with_raw/
    # offloaded_raw_tx) -- "default" is the one the driver's own CMakeLists.txt
    # selects for normal STA/AP system mode (the only mode this variant uses).
    transport_blobs={
        "wifi": ("nrf_wifi", ".*default/nrf70\\.bin", ".blobs_nrf_wifi_ready")
    },
    soc="nrf5340",
    qualifier="cpuapp",
    # No "scratch": nrf5340_cpuapp_partition.dtsi defines no scratch_partition either,
    # same as nRF52's stock layout.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    # Dynamic discovery (see nrf52.py/siwx917.py) -- boards built on this SoC vary
    # which UARTs they expose, so no static fallback is assumed.
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
    # west build always runs with --sysbuild (build_zephyr.py); mcuboot is the only
    # bootloader this variant offers (see _ADVANCED_SCHEMA above), so it's unconditional.
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
    if zephyr_framework_type() == "ncs" and CORE.config.get(CONF_WIFI) is not None:
        # NCS's own sysbuild layer (nrf/sysbuild/CMakeLists.txt) gates the app image's
        # CONFIG_WIFI_NRF70 on this sysbuild-level symbol: without it, sysbuild force-sets
        # CONFIG_WIFI_NRF70=n on the app image regardless of what wifi/__init__.py already
        # requested via the per-image prj.conf (transport_drivers above). Mainline Zephyr
        # has no such sysbuild gate, hence the framework-type check.
        zephyr_add_sysbuild_conf("WIFI_NRF70", True)
