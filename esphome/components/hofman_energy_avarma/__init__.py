import esphome.codegen as cg
from esphome.components import binary_sensor, modbus_controller, number, sensor, switch
from esphome.components.modbus_controller import ModbusController
from esphome.components.modbus_controller.binary_sensor import ModbusBinarySensor
from esphome.components.modbus_controller.const import CONF_REGISTER_TYPE
from esphome.components.modbus_controller.number import ModbusNumber
from esphome.components.modbus_controller.sensor import ModbusSensor
from esphome.components.modbus_controller.switch import ModbusSwitch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_FORCE_UPDATE,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MULTIPLY,
    CONF_NAME,
    CONF_PLATFORM,
)

from .avarma_registers import (
    AVARMA_BINARY_REGISTERS,
    AVARMA_NUMBER_REGISTERS,
    AVARMA_SENSOR_REGISTERS,
    AVARMA_SWITCH_REGISTERS,
)

DEPENDENCIES = ["modbus", "modbus_controller"]
AUTO_LOAD = [
    "sensor",
    "switch",
    "number",
    "binary_sensor",
    "modbus",
]
MULTI_CONF = False

avarma_component_ns = cg.esphome_ns.namespace("hofman_energy_avarma")
HofmanEnergyAvarmaComponent = avarma_component_ns.class_(
    "HofmanEnergyAvarmaComponent", cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HofmanEnergyAvarmaComponent),
            cv.Required(modbus_controller.CONF_MODBUS_CONTROLLER_ID): cv.use_id(
                ModbusController
            ),
        }
    )
    .extend(
        {
            cv.Optional(
                register.parameter_id,
                {CONF_NAME: register.name},
            ): sensor.sensor_schema(
                ModbusSensor,
                unit_of_measurement=register.unit_of_measurement,
                accuracy_decimals=register.accuracy_decimals,
                entity_category=register.entity_category,
                device_class=register.device_class,
                filters=[{"multiply": register.register_factor}],
            )
            for register in AVARMA_SENSOR_REGISTERS
        }
    )
    .extend(
        {
            cv.Optional(
                f"{register.parameter_id.replace('-', '_')}_{flag.bitmask}",
                {CONF_NAME: register.name + " " + flag.name},
            ): binary_sensor.binary_sensor_schema(
                ModbusBinarySensor,
                entity_category=register.entity_category,
            )
            for register in AVARMA_BINARY_REGISTERS
            for flag in register.flags  # type: ignore
        }
    )
    .extend(
        {
            cv.Optional(
                register.parameter_id,
                {CONF_NAME: register.name},
            ): switch.switch_schema(
                ModbusSwitch,
                entity_category=register.entity_category,
                device_class=register.device_class,
                default_restore_mode="DISABLED",
            )
            for register in AVARMA_SWITCH_REGISTERS
        }
    )
    .extend(
        {
            cv.Optional(
                register.parameter_id,
                {CONF_NAME: register.name},
            ): switch.switch_schema(
                ModbusSwitch,
                entity_category=register.entity_category,
                device_class=register.device_class,
                default_restore_mode="DISABLED",
            )
            for register in AVARMA_SWITCH_REGISTERS
        }
    )
    .extend(
        {
            cv.Optional(
                register.parameter_id, {CONF_NAME: register.name}
            ): number.number_schema(ModbusNumber)
            for register in AVARMA_NUMBER_REGISTERS
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    paren = await cg.get_variable(config[modbus_controller.CONF_MODBUS_CONTROLLER_ID])

    for register in AVARMA_SENSOR_REGISTERS:
        conf = config[register.parameter_id]

        conf[CONF_PLATFORM] = "modbus_controller"
        conf[modbus_controller.CONF_MODBUS_CONTROLLER_ID] = config[
            modbus_controller.CONF_MODBUS_CONTROLLER_ID
        ]
        conf[modbus_controller.CONF_ADDRESS] = register.address
        conf[modbus_controller.CONF_REGISTER_TYPE] = (
            modbus_controller.ModbusRegisterType.HOLDING
        )

        value_type = register.value_type
        var = cg.new_Pvariable(
            conf[CONF_ID],
            conf[modbus_controller.CONF_REGISTER_TYPE],
            conf[modbus_controller.CONF_ADDRESS],
            0,
            0xFFFFFFFF,
            value_type,
            1,
            0,
            False,
        )
        await sensor.register_sensor(var, conf)
        cg.add(paren.add_sensor_item(var))

    for register in AVARMA_BINARY_REGISTERS:
        if register.flags is not None:
            for flag in register.flags:
                binid = f"{register.parameter_id.replace('-', '_')}_{flag.bitmask}"
                conf = config[binid]
                conf[CONF_PLATFORM] = "modbus_controller"
                conf[modbus_controller.CONF_MODBUS_CONTROLLER_ID] = config[
                    modbus_controller.CONF_MODBUS_CONTROLLER_ID
                ]
                conf[modbus_controller.CONF_ADDRESS] = register.address
                conf[modbus_controller.CONF_REGISTER_TYPE] = "holding"
                conf[modbus_controller.CONF_BITMASK] = flag.bitmask
                conf[CONF_FORCE_UPDATE] = False

                var = cg.new_Pvariable(
                    conf[CONF_ID],
                    modbus_controller.ModbusRegisterType.HOLDING,
                    conf[modbus_controller.CONF_ADDRESS],
                    0,
                    conf[modbus_controller.CONF_BITMASK],
                    0,
                    False,
                )
                await binary_sensor.register_binary_sensor(var, conf)
                cg.add(paren.add_sensor_item(var))

    for register in AVARMA_SWITCH_REGISTERS:
        conf = config[register.parameter_id]
        conf[CONF_PLATFORM] = "modbus_controller"
        conf[modbus_controller.CONF_MODBUS_CONTROLLER_ID] = config[
            modbus_controller.CONF_MODBUS_CONTROLLER_ID
        ]
        conf[modbus_controller.CONF_ADDRESS] = register.address
        conf[modbus_controller.CONF_REGISTER_TYPE] = (
            modbus_controller.ModbusRegisterType.HOLDING
        )

        var = cg.new_Pvariable(
            conf[CONF_ID],
            conf[modbus_controller.CONF_REGISTER_TYPE],
            conf[modbus_controller.CONF_ADDRESS],
            0,
            0xFFFFFFFF,
            0,
            False,
        )
        await switch.register_switch(var, conf)
        cg.add(var.set_parent(paren))
        cg.add(paren.add_sensor_item(var))

    for register in AVARMA_NUMBER_REGISTERS:
        conf = config[register.parameter_id]
        conf[CONF_PLATFORM] = "modbus_controller"
        conf[modbus_controller.CONF_MODBUS_CONTROLLER_ID] = config[
            modbus_controller.CONF_MODBUS_CONTROLLER_ID
        ]
        conf[modbus_controller.CONF_ADDRESS] = register.address
        conf[modbus_controller.CONF_REGISTER_TYPE] = (
            modbus_controller.ModbusRegisterType.HOLDING
        )

        conf[CONF_MIN_VALUE] = register.min
        conf[CONF_MAX_VALUE] = register.max
        conf[CONF_MULTIPLY] = register.register_factor

        var = cg.new_Pvariable(
            conf[CONF_ID],
            conf[CONF_REGISTER_TYPE],
            conf[CONF_ADDRESS],
            0,
            0xFFFFFFFF,
            register.value_type,
            1.0,
            0,
            False,
        )

        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=1.0,
        )

        cg.add(var.set_parent(paren))
        cg.add(paren.add_sensor_item(var))
        cg.add(var.set_write_multiply(conf[CONF_MULTIPLY]))

    await cg.register_component(var, config)
