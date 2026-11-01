import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIAMP,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import CONF_SY6970_ID, REGISTERS, SY6970Component, publish_children, sy6970_ns

DEPENDENCIES = ["sy6970"]

CONF_VBUS_VOLTAGE = "vbus_voltage"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_CHARGE_CURRENT = "charge_current"
CONF_PRECHARGE_CURRENT = "precharge_current"


def _decode(
    reg: cg.MockObj,
    step: cg.MockObj,
    base: cg.MockObj | None = None,
    *,
    shift: int = 0,
    mask: int = 0x7F,
    scale: float | None = None,
) -> cg.MockObj:
    """``(base + ((registers[reg] >> shift) & mask) * step) * scale``."""
    raw = REGISTERS[reg]
    if shift:
        raw = raw >> shift
    value = (raw & mask) * step
    if base is not None:
        value = base + value
    return value if scale is None else value * scale


# Voltages in V from mV, currents in mA
STATES = {
    CONF_VBUS_VOLTAGE: _decode(
        sy6970_ns.SY6970_REG_VBUS_VOLTAGE,
        sy6970_ns.VBUS_STEP_MV,
        sy6970_ns.VBUS_BASE_MV,
        scale=0.001,
    ),
    CONF_BATTERY_VOLTAGE: _decode(
        sy6970_ns.SY6970_REG_BATV,
        sy6970_ns.VBAT_STEP_MV,
        sy6970_ns.VBAT_BASE_MV,
        scale=0.001,
    ),
    CONF_SYSTEM_VOLTAGE: _decode(
        sy6970_ns.SY6970_REG_VINDPM_STATUS,
        sy6970_ns.VSYS_STEP_MV,
        sy6970_ns.VSYS_BASE_MV,
        scale=0.001,
    ),
    CONF_CHARGE_CURRENT: _decode(
        sy6970_ns.SY6970_REG_CHARGE_CURRENT_MONITOR, sy6970_ns.CHG_CURRENT_STEP_MA
    ),
    CONF_PRECHARGE_CURRENT: _decode(
        sy6970_ns.SY6970_REG_PRECHARGE_CURRENT,
        sy6970_ns.PRE_CHG_STEP_MA,
        sy6970_ns.PRE_CHG_BASE_MA,
        shift=4,
        mask=0x0F,
    ),
}


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_VBUS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SYSTEM_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRECHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SY6970_ID])

    await publish_children(hub, sensor.sub_sensors(config), STATES)
