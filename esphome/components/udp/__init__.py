from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
from esphome.components.const import CONF_ON_RECEIVE
from esphome.components.packet_transport import (
    CONF_BINARY_SENSORS,
    CONF_ENCRYPTION,
    CONF_PING_PONG_ENABLE,
    CONF_PROVIDERS,
    CONF_ROLLING_CODE_ENABLE,
    CONF_SENSORS,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATA,
    CONF_ENABLE_IPV6,
    CONF_ID,
    CONF_NETWORK,
    CONF_PORT,
    CONF_TRIGGER_ID,
)
from esphome.core import ID, Lambda
from esphome.cpp_generator import ExpressionStatement, MockObj
import esphome.final_validate as fv

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]

MULTI_CONF = True
udp_ns = cg.esphome_ns.namespace("udp")
UDPComponent = udp_ns.class_("UDPComponent", cg.Component)
UDPWriteAction = udp_ns.class_("UDPWriteAction", automation.Action)
trigger_args = cg.std_vector.template(cg.uint8)

CONF_ADDRESSES = "addresses"
CONF_LISTEN_ADDRESS = "listen_address"
CONF_UDP_ID = "udp_id"
CONF_LISTEN_PORT = "listen_port"
CONF_BROADCAST_PORT = "broadcast_port"

UDP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.valid_name,
    }
).extend(ENCRYPTION_SCHEMA)


def validate_(config):
    if CONF_ENCRYPTION in config:
        if CONF_SENSORS not in config and CONF_BINARY_SENSORS not in config:
            raise cv.Invalid("No sensors or binary sensors to encrypt")
    elif config[CONF_ROLLING_CODE_ENABLE]:
        raise cv.Invalid("Rolling code requires an encryption key")
    if config[CONF_PING_PONG_ENABLE]:
        if not any(CONF_ENCRYPTION in p for p in config.get(CONF_PROVIDERS) or ()):
            raise cv.Invalid("Ping-pong requires at least one encrypted provider")
    return config


CONFIG_SCHEMA = cv.All(
    cv.polling_component_schema("15s")
    .extend(
        {
            cv.GenerateID(): cv.declare_id(UDPComponent),
            cv.Optional(CONF_PORT, default=18511): cv.port,
            cv.Optional(
                CONF_LISTEN_ADDRESS, default="255.255.255.255"
            ): cv.ipaddress_multi_broadcast,
            cv.Optional(CONF_ADDRESSES, default=["255.255.255.255"]): cv.ensure_list(
                cv.ipaddress,
            ),
            cv.Optional(CONF_ROLLING_CODE_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_PING_PONG_ENABLE, default=False): cv.boolean,
            cv.Optional(
                CONF_PING_PONG_RECYCLE_TIME, default="600s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_SENSORS): cv.ensure_list(sensor_validation(Sensor)),
            cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                sensor_validation(BinarySensor)
            ),
            cv.Optional(CONF_PROVIDERS): cv.ensure_list(PROVIDER_SCHEMA),
        },
    )
    .extend(ENCRYPTION_SCHEMA),
    validate_,
)

SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REMOTE_ID): cv.string_strict,
        cv.Required(CONF_PROVIDER): cv.valid_name,
        cv.GenerateID(CONF_UDP_ID): cv.use_id(UDPComponent),
    }
)


def is_relocated(option):
    def validator(value):
        raise cv.Invalid(
            f"The '{option}' option should now be configured in the 'packet_transport' component"
        )

    return validator
def _final_validate(config):
    enable_ipv6 = fv.full_config.get().get(CONF_NETWORK).get(CONF_ENABLE_IPV6)
    if not enable_ipv6:
        for address in config[CONF_ADDRESSES]:
            cv.ipv4address(address)
        cv.ipv4address_multi_broadcast(config[CONF_LISTEN_ADDRESS])
    return config


def require_internal_with_name(config):
    if CONF_NAME in config and CONF_INTERNAL not in config:
        raise cv.Invalid("Must provide internal: config when using name:")
    return config

def is_relocated(option):
    def validator(value):
        raise cv.Invalid(
            f"The '{option}' option should now be configured in the 'packet_transport' component"
        )

    return validator


RELOCATED = {
    cv.Optional(x): is_relocated(x)
    for x in (
        CONF_PROVIDERS,
        CONF_ENCRYPTION,
        CONF_PING_PONG_ENABLE,
        CONF_ROLLING_CODE_ENABLE,
        CONF_SENSORS,
        CONF_BINARY_SENSORS,
    )
}

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(UDPComponent),
        cv.Optional(CONF_PORT, default=18511): cv.Any(
            cv.port,
            cv.Schema(
                {
                    cv.Required(CONF_LISTEN_PORT): cv.port,
                    cv.Required(CONF_BROADCAST_PORT): cv.port,
                }
            ),
        ),
        cv.Optional(
            CONF_LISTEN_ADDRESS, default="255.255.255.255"
        ): cv.ipaddress_multi_broadcast,
        cv.Optional(CONF_ADDRESSES, default=["255.255.255.255"]): cv.ensure_list(
            cv.ipaddress,
        ),
        cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    Trigger.template(trigger_args)
                ),
            }
        ),
    }
).extend(RELOCATED)


async def register_udp_client(var, config):
    udp_var = await cg.get_variable(config[CONF_UDP_ID])
    cg.add(var.set_parent(udp_var))
    return udp_var


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    cg.add_define("USE_UDP")
    cg.add_global(udp_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    var = await cg.register_component(var, config)
    conf_port = config[CONF_PORT]
    if isinstance(conf_port, int):
        cg.add(var.set_listen_port(conf_port))
        cg.add(var.set_broadcast_port(conf_port))
    else:
        cg.add(var.set_listen_port(conf_port[CONF_LISTEN_PORT]))
        cg.add(var.set_broadcast_port(conf_port[CONF_BROADCAST_PORT]))
    if (listen_address := str(config[CONF_LISTEN_ADDRESS])) != "255.255.255.255":
        cg.add(var.set_listen_address(listen_address))
    for address in config[CONF_ADDRESSES]:
        cg.add(var.add_address_parsed(cg.IPAddress(address)))
    if on_receive := config.get(CONF_ON_RECEIVE):
        on_receive = on_receive[0]
        trigger = cg.new_Pvariable(on_receive[CONF_TRIGGER_ID])
        trigger = await automation.build_automation(
            trigger, [(trigger_args, "data")], on_receive
        )
        trigger = Lambda(str(ExpressionStatement(trigger.trigger(MockObj("data")))))
        trigger = await cg.process_lambda(trigger, [(trigger_args, "data")])
        cg.add(var.add_listener(trigger))
        cg.add(var.set_should_listen())


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


@automation.register_action(
    "udp.write",
    UDPWriteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(UDPComponent),
            cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
        },
        key=CONF_DATA,
    ),
)
async def udp_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    udp_var = await cg.get_variable(config[CONF_ID])
    await cg.register_parented(var, udp_var)
    cg.add(udp_var.set_should_broadcast())
    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        # Generate static array in flash to avoid RAM copy
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
        cg.add(var.set_data_static(arr, len(data)))
    return var
