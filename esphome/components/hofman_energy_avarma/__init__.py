import esphome.codegen as cg
from esphome.components import binary_sensor, modbus_controller, sensor
from esphome.components.modbus_controller import (
    ModbusController,
    SensorValueType,
    modbus_calc_properties,
)
from esphome.components.modbus_controller.binary_sensor import ModbusBinarySensor
from esphome.components.modbus_controller.sensor import ModbusSensor
import esphome.config_validation as cv
from esphome.const import CONF_FORCE_UPDATE, CONF_ID, CONF_NAME, CONF_PLATFORM

from .avarma_registers import AVARMA_BINARY_REGISTERS, AVARMA_SENSOR_REGISTERS

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
                unit_of_measurement=register.parameter_id,
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
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    paren = await cg.get_variable(config[modbus_controller.CONF_MODBUS_CONTROLLER_ID])
    # config["test"]["id"] = "test1234"
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

        byte_offset, reg_count = modbus_calc_properties(conf)
        value_type = SensorValueType.U_WORD
        var = cg.new_Pvariable(
            conf[CONF_ID],
            conf[modbus_controller.CONF_REGISTER_TYPE],
            conf[modbus_controller.CONF_ADDRESS],
            byte_offset,
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
                # await cg.register_component(var, conf)
                await binary_sensor.register_binary_sensor(var, conf)
                cg.add(paren.add_sensor_item(var))

                # await binary_sensor.new_binary_sensor(conf)
    await cg.register_component(var, config)
