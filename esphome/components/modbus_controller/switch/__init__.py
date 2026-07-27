import esphome.codegen as cg
from esphome.components import switch
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ASSUMED_STATE, CONF_ID

from .. import (
    RANGE_REUSE,
    ModbusItemBaseSchema,
    ModbusWriteBytes,
    SensorItem,
    add_modbus_base_properties,
    modbus_calc_properties,
    modbus_controller_ns,
    validate_modbus_register,
    validate_range_reuse_migration,
)
from ..const import (
    CONF_BITMASK,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_REUSE_PREVIOUS_RANGE,
    CONF_SKIP_UPDATES,
    CONF_USE_WRITE_MULTIPLE,
    CONF_WRITE_LAMBDA,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSwitch = modbus_controller_ns.class_(
    "ModbusSwitch", cg.Component, switch.Switch, SensorItem
)

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
    validate_range_reuse_migration,
)


async def to_code(config):
    byte_offset = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_BITMASK],
        config[CONF_SKIP_UPDATES],
        RANGE_REUSE[config[CONF_REUSE_PREVIOUS_RANGE]],
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
                (ModbusWriteBytes.operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(bool),
        )
        cg.add(var.set_write_template(template_))
    await add_modbus_base_properties(var, config, ModbusSwitch, bool, bool)
