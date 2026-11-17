import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_RX_ONLY,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

sds011_ns = cg.esphome_ns.namespace("sds011")
SDS011Component = sds011_ns.class_("SDS011Component", uart.UARTDevice, cg.Component)


def validate_update_interval(value: ConfigType) -> ConfigType:
    if (
        update_interval := value.get(CONF_UPDATE_INTERVAL)
    ) is not None and update_interval.total_minutes > 30:
        raise cv.Invalid("Maximum update interval is 30min")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SDS011Component),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RX_ONLY, default=False): cv.boolean,
            cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_minutes,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    cv.only_when(CONF_UPDATE_INTERVAL, CONF_RX_ONLY, False),
    validate_update_interval,
)


def _final_validate(config: ConfigType) -> None:
    # In the default mode setup() writes config commands, so tx is required;
    # rx_only mode never writes, so tx is optional.
    uart.final_validate_device_schema(
        "sds011",
        baud_rate=9600,
        require_rx=True,
        require_tx=not config.get(CONF_RX_ONLY, False),
        data_bits=8,
        parity="NONE",
        stop_bits=1,
    )(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    # Pop update_interval before register_component so it doesn't generate
    # a set_update_interval call — sds011 handles this via set_update_interval_min
    update_interval = config.pop(CONF_UPDATE_INTERVAL, None)
    var = await uart.new_uart_component(config)

    if update_interval is not None:
        cg.add(var.set_update_interval_min(update_interval))
    cg.add(var.set_rx_mode_only(config[CONF_RX_ONLY]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PM_2_5, var.set_pm_2_5_sensor)
    await sensors(CONF_PM_10_0, var.set_pm_10_0_sensor)
