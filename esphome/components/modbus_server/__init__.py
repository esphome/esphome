import esphome.codegen as cg
from esphome.components import modbus
from esphome.components.const import CONF_ENABLED
from esphome.components.modbus.helpers import (
    CPP_TYPE_REGISTER_MAP,
    SENSOR_VALUE_TYPE,
    TYPE_REGISTER_MAP,
)
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID

from .const import (
    CONF_COURTESY_RESPONSE,
    CONF_READ_LAMBDA,
    CONF_REGISTER_LAST_ADDRESS,
    CONF_REGISTER_VALUE,
    CONF_REGISTERS,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

CODEOWNERS = ["@exciton"]

AUTO_LOAD = ["modbus"]

MULTI_CONF = True

modbus_server_ns = cg.esphome_ns.namespace("modbus_server")
ModbusServer = modbus_server_ns.class_(
    "ModbusServer", cg.Component, modbus.ModbusDevice
)

ServerCourtesyResponse = modbus_server_ns.struct("ServerCourtesyResponse")
ServerRegister = modbus_server_ns.struct("ServerRegister")

SERVER_COURTESY_RESPONSE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_REGISTER_LAST_ADDRESS, default=0xFFFF): cv.hex_uint16_t,
        cv.Optional(CONF_REGISTER_VALUE, default=0): cv.hex_uint16_t,
    }
)

ModbusServerRegisterSchema = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ServerRegister),
        cv.Required(CONF_ADDRESS): cv.positive_int,
        cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
        cv.Required(CONF_READ_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusServer),
            cv.Optional(CONF_COURTESY_RESPONSE): SERVER_COURTESY_RESPONSE_SCHEMA,
            cv.Optional(
                CONF_REGISTERS,
            ): cv.ensure_list(ModbusServerRegisterSchema),
        }
    ).extend(modbus.modbus_device_schema(0x01)),
)


def _final_validate(config):
    return modbus.final_validate_modbus_device("modbus_server", role="server")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if server_courtesy_response := config.get(CONF_COURTESY_RESPONSE):
        cg.add(
            var.set_server_courtesy_response(
                cg.StructInitializer(
                    ServerCourtesyResponse,
                    ("enabled", server_courtesy_response[CONF_ENABLED]),
                    (
                        "register_last_address",
                        server_courtesy_response[CONF_REGISTER_LAST_ADDRESS],
                    ),
                    ("register_value", server_courtesy_response[CONF_REGISTER_VALUE]),
                )
            )
        )
    if CONF_REGISTERS in config:
        for server_register in config[CONF_REGISTERS]:
            server_register_var = cg.new_Pvariable(
                server_register[CONF_ID],
                server_register[CONF_ADDRESS],
                server_register[CONF_VALUE_TYPE],
                TYPE_REGISTER_MAP[server_register[CONF_VALUE_TYPE]],
            )
            cpp_type = CPP_TYPE_REGISTER_MAP[server_register[CONF_VALUE_TYPE]]
            cg.add(
                server_register_var.set_read_lambda(
                    cg.TemplateArguments(cpp_type),
                    await cg.process_lambda(
                        server_register[CONF_READ_LAMBDA],
                        [(cg.uint16, "address")],
                        return_type=cpp_type,
                    ),
                )
            )
            if CONF_WRITE_LAMBDA in server_register:
                cg.add(
                    server_register_var.set_write_lambda(
                        cg.TemplateArguments(cpp_type),
                        await cg.process_lambda(
                            server_register[CONF_WRITE_LAMBDA],
                            parameters=[(cg.uint16, "address"), (cpp_type, "x")],
                            return_type=cg.bool_,
                        ),
                    )
                )
            cg.add(var.add_server_register(server_register_var))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await cg.register_component(var, config)
    return await modbus.register_modbus_device(var, config)
