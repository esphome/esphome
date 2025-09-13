import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_LAMBDA, CONF_OPTIMISTIC

from .. import (
    SENSOR_VALUE_TYPE,
    TYPE_REGISTER_MAP,
    ModbusController,
    SensorItem,
    modbus_controller_ns,
)
from ..const import (
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_SKIP_UPDATES,
    CONF_USE_WRITE_MULTIPLE,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras", "@stegm"]
CONF_OPTIONSMAP = "optionsmap"

ModbusSelect = modbus_controller_ns.class_(
    "ModbusSelect", cg.Component, select.Select, SensorItem
)


def ensure_option_map():
    def validator(value):
        cv.check_not_templatable(value)
        option = cv.All(cv.string_strict)
        mapping = cv.All(cv.int_range(-(2**63), 2**63 - 1))
        options_map_schema = cv.Schema({option: mapping})
        value = options_map_schema(value)

        all_values = list(value.values())
        unique_values = set(value.values())
        if len(all_values) != len(unique_values):
            raise cv.Invalid("Mapping values must be unique.")

        return value

    return validator


def register_count_value_type_min(value):
    reg_count = value.get(CONF_REGISTER_COUNT)
    if reg_count is not None:
        value_type = value[CONF_VALUE_TYPE]
        min_register_count = TYPE_REGISTER_MAP[value_type]
        if min_register_count > reg_count:
            raise cv.Invalid(
                f"Value type {value_type} needs at least {min_register_count} registers"
            )
    return value


INTEGER_SENSOR_VALUE_TYPE = {
    key: value for key, value in SENSOR_VALUE_TYPE.items() if not key.startswith("FP")
}

CONFIG_SCHEMA = cv.All(
    select.select_schema(ModbusSelect)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.Required(CONF_ADDRESS): cv.positive_int,
            cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(
                INTEGER_SENSOR_VALUE_TYPE
            ),
            cv.Optional(CONF_REGISTER_COUNT): cv.positive_int,
            cv.Optional(CONF_SKIP_UPDATES, default=0): cv.positive_int,
            cv.Optional(CONF_FORCE_NEW_RANGE, default=False): cv.boolean,
            cv.Required(CONF_OPTIONSMAP): ensure_option_map(),
            cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
        },
    ),
    register_count_value_type_min,
)


async def to_code(config):
    value_type = config[CONF_VALUE_TYPE]
    reg_count = config.get(CONF_REGISTER_COUNT)
    if reg_count is None:
        reg_count = TYPE_REGISTER_MAP[value_type]

    options_map = config[CONF_OPTIONSMAP]

    var = cg.new_Pvariable(
        config[CONF_ID],
        value_type,
        config[CONF_ADDRESS],
        reg_count,
        config[CONF_SKIP_UPDATES],
        config[CONF_FORCE_NEW_RANGE],
        list(options_map.values()),
    )

    await cg.register_component(var, config)
    await select.register_select(var, config, options=list(options_map.keys()))

    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(parent.add_sensor_item(var))
    cg.add(var.set_parent(parent))
    cg.add(var.set_use_write_mutiple(config[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [
                (ModbusSelect.operator("const_ptr"), "item"),
                (cg.int64, "x"),
                (
                    cg.std_vector.template(cg.uint8).operator("const").operator("ref"),
                    "data",
                ),
            ],
            return_type=cg.optional.template(cg.std_string),
        )
        cg.add(var.set_template(template_))

    if CONF_WRITE_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_WRITE_LAMBDA],
            [
                (ModbusSelect.operator("const_ptr"), "item"),
                (cg.std_string.operator("const").operator("ref"), "x"),
                (cg.int64, "value"),
                (cg.std_vector.template(cg.uint16).operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(cg.int64),
        )
        cg.add(var.set_write_template(template_))
