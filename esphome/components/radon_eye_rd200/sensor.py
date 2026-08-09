import functools

import esphome.codegen as cg
from esphome.components import bluetooth_connection, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_RADON,
    CONF_RADON_LONG_TERM,
    ICON_RADIOACTIVE,
    PLATFORM_ESP32,
    PLATFORM_RP2,
    STATE_CLASS_MEASUREMENT,
    UNIT_BECQUEREL_PER_CUBIC_METER,
)
from esphome.core import CORE
from esphome.types import ConfigType


def AUTO_LOAD(config: ConfigType | None = None) -> list[str]:
    """The GATT backend plus the platform BLE stack it registers with; the
    union arm serves tooling that resolves the manifest without a target
    platform (the bluetooth_proxy pattern)."""
    if CORE.is_esp32:
        return ["bluetooth_connection", "esp32_ble_tracker"]
    if CORE.target_platform == PLATFORM_RP2:
        return ["bluetooth_connection", "rp2040_ble"]
    return ["bluetooth_connection", "esp32_ble_tracker", "rp2040_ble"]


radon_eye_rd200_ns = cg.esphome_ns.namespace("radon_eye_rd200")
RadonEyeRD200 = radon_eye_rd200_ns.class_("RadonEyeRD200", cg.PollingComponent)

_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
    icon=ICON_RADIOACTIVE,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)


@functools.lru_cache(maxsize=None)
def _schema_for_platform(platform: str) -> cv.Schema | cv.All:
    """Built per platform (cached): the backend id's class and the esp32
    controller-slot consumption depend on the target."""
    schema = (
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(RadonEyeRD200),
                cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Optional(CONF_RADON): _SENSOR_SCHEMA,
                cv.Optional(CONF_RADON_LONG_TERM): _SENSOR_SCHEMA,
            }
        )
        .extend(cv.polling_component_schema("5min"))
        .extend(bluetooth_connection.gatt_client_schema())
    )
    if platform == PLATFORM_ESP32:
        from esphome.components import esp32_ble

        return cv.All(
            schema, esp32_ble.consume_connection_slots(1, "radon_eye_rd200")
        )
    return schema


def _platform_schema(config: ConfigType) -> ConfigType:
    return _schema_for_platform(CORE.target_platform)(config)


CONFIG_SCHEMA = cv.All(
    cv.only_on([PLATFORM_ESP32, PLATFORM_RP2]),
    _platform_schema,
)


async def to_code(config: ConfigType) -> None:
    backend = await bluetooth_connection.new_gatt_backend(config)
    var = cg.new_Pvariable(
        config[CONF_ID], backend, config[CONF_MAC_ADDRESS].as_hex
    )
    await cg.register_component(var, config)

    if (radon := config.get(CONF_RADON)) is not None:
        sens = await sensor.new_sensor(radon)
        cg.add(var.set_radon(sens))
    if (radon_long_term := config.get(CONF_RADON_LONG_TERM)) is not None:
        sens = await sensor.new_sensor(radon_long_term)
        cg.add(var.set_radon_long_term(sens))
