import esphome.codegen as cg
from esphome.components import switch
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, PduBuffer
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ASSUMED_STATE, CONF_ID
from esphome.types import ConfigType

from .. import (
    ModbusItemBaseSchema,
    SensorItem,
    add_modbus_base_properties,
    modbus_calc_properties,
    modbus_controller_ns,
    reject_odd_holding_write_offset,
    validate_custom_pdu_item,
    validate_modbus_register,
)
from ..const import (
    CONF_BITMASK,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_USE_WRITE_MULTIPLE,
    CONF_WRITE_LAMBDA,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSwitch = modbus_controller_ns.class_(
    "ModbusSwitch", cg.Component, switch.Switch, SensorItem
)


def _validate_holding_offset(config: ConfigType) -> ConfigType:
    # Only a holding-register switch folds the byte offset into a 16-bit register write.
    if config.get(CONF_REGISTER_TYPE) == "holding":
        reject_odd_holding_write_offset(config)
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(ModbusSwitch, default_restore_mode="DISABLED")
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ModbusItemBaseSchema)
    .extend(
        {
            cv.Optional(CONF_ASSUMED_STATE, default=False): cv.boolean,
            cv.Optional(CONF_REGISTER_TYPE): cv.enum(MODBUS_REGISTER_TYPE),
            cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
            cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
        }
    ),
    validate_modbus_register,
    _validate_holding_offset,
)

FINAL_VALIDATE_SCHEMA = validate_custom_pdu_item


async def to_code(config: ConfigType) -> None:
    byte_offset, _ = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_BITMASK],
        config[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    paren = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_parent(paren))
    cg.add(var.set_use_write_mutiple(config[CONF_USE_WRITE_MULTIPLE]))
    assumed_state = config[CONF_ASSUMED_STATE]
    cg.add(var.set_assumed_state(assumed_state))
    if not assumed_state:
        cg.add(paren.add_sensor_item(var))
    if CONF_WRITE_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_WRITE_LAMBDA],
            [
                (ModbusSwitch.operator("ptr"), "item"),
                (cg.bool_, "x"),
                (PduBuffer.operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(bool),
        )
        cg.add(var.set_write_template(template_))
    await add_modbus_base_properties(var, config, ModbusSwitch, bool, bool)
