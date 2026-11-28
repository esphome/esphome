import esphome.codegen as cg
from esphome.components import ble_client, ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_EXTERNAL_TEMPERATURE,
    CONF_HUMIDITY,
    CONF_MAC_ADDRESS,
    CONF_SIGNAL_STRENGTH,
    CONF_TEMPERATURE,
    CONF_TIMEOUT,
    CONF_TYPE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from . import ThermoProBLEClient

CODEOWNERS = ["@sittner", "@kj4dxk"]


def AUTO_LOAD(config: ConfigType) -> list[str]:
    return ["ble_client"] if config.get(CONF_TYPE) == "gatt" else ["ble_device_base"]


CONF_NOTIFICATION_COUNT = "notification_count"

thermopro_ble_ns = cg.esphome_ns.namespace("thermopro_ble")
ThermoProBLE = thermopro_ble_ns.class_(
    "ThermoProBLE", ble_device_base.ESPBTDeviceListener
)


ADVERTISEMENT_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("thermopro_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ThermoProBLE),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                kind=sensor.HUMIDITY, accuracy_decimals=0
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                kind=sensor.BATTERY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
                kind=sensor.SIGNAL_STRENGTH, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


GATT_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ThermoProBLEClient),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                kind=sensor.BATTERY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_NOTIFICATION_COUNT): sensor.sensor_schema(
                accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(
                CONF_TIMEOUT, default="15s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "advertisement": ADVERTISEMENT_SCHEMA,
        "gatt": cv.All(cv.only_on_esp32, GATT_SCHEMA),
    },
    default_type="advertisement",
)


async def to_code(config: ConfigType) -> None:
    if config[CONF_TYPE] == "gatt":
        cg.add_define("USE_THERMOPRO_BLE_GATT")
        var = await ble_client.new_ble_node_component(config)
        cg.add(var.set_timeout_ms(config[CONF_TIMEOUT].total_milliseconds))
    else:
        var = await ble_device_base.new_ble_device(
            config, config[CONF_MAC_ADDRESS].as_hex
        )

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_EXTERNAL_TEMPERATURE, var.set_external_temperature_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
    if config[CONF_TYPE] == "gatt":
        await sensors(CONF_NOTIFICATION_COUNT, var.set_notification_count_sensor)
    else:
        await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
        await sensors(CONF_SIGNAL_STRENGTH, var.set_signal_strength_sensor)
