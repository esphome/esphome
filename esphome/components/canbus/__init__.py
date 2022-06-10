import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.core import CORE
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_DATA

CODEOWNERS = ["@mvturnho", "@danielschramm"]
IS_PLATFORM_COMPONENT = True

CONF_CAN_ID = "can_id"
CONF_CAN_ID_MASK = "can_id_mask"
CONF_USE_EXTENDED_ID = "use_extended_id"
CONF_REMOTE_TRANSMISSION_REQUEST = "remote_transmission_request"
CONF_CANBUS_ID = "canbus_id"
CONF_BIT_RATE = "bit_rate"
CONF_ON_FRAME = "on_frame"


def validate_id(config):
    if CONF_CAN_ID in config:
        id_value = config[CONF_CAN_ID]
        id_ext = config[CONF_USE_EXTENDED_ID]
        if not id_ext:
            if id_value > 0x7FF:
                raise cv.Invalid("Standard IDs must be 11 Bit (0x000-0x7ff / 0-2047)")
    return config


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


canbus_ns = cg.esphome_ns.namespace("canbus")
CanbusComponent = canbus_ns.class_("CanbusComponent", cg.Component)
CanbusTrigger = canbus_ns.class_(
    "CanbusTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8), cg.uint32),
    cg.Component,
)
CanSpeed = canbus_ns.enum("CAN_SPEED")

CAN_SPEEDS = {
    "5KBPS": CanSpeed.CAN_5KBPS,
    "10KBPS": CanSpeed.CAN_10KBPS,
    "20KBPS": CanSpeed.CAN_20KBPS,
    "31K25BPS": CanSpeed.CAN_31K25BPS,
    "33KBPS": CanSpeed.CAN_33KBPS,
    "40KBPS": CanSpeed.CAN_40KBPS,
    "50KBPS": CanSpeed.CAN_50KBPS,
    "80KBPS": CanSpeed.CAN_80KBPS,
    "83K3BPS": CanSpeed.CAN_83K3BPS,
    "95KBPS": CanSpeed.CAN_95KBPS,
    "100KBPS": CanSpeed.CAN_100KBPS,
    "125KBPS": CanSpeed.CAN_125KBPS,
    "200KBPS": CanSpeed.CAN_200KBPS,
    "250KBPS": CanSpeed.CAN_250KBPS,
    "500KBPS": CanSpeed.CAN_500KBPS,
    "1000KBPS": CanSpeed.CAN_1000KBPS,
}

CANBUS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CanbusComponent),
        cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
        cv.Optional(CONF_BIT_RATE, default="125KBPS"): cv.enum(CAN_SPEEDS, upper=True),
        cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
        cv.Optional(CONF_ON_FRAME): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CanbusTrigger),
                cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
                cv.Optional(CONF_CAN_ID_MASK, default=0x1FFFFFFF): cv.int_range(
                    min=0, max=0x1FFFFFFF
                ),
                cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
                cv.Optional(CONF_REMOTE_TRANSMISSION_REQUEST): cv.boolean,
            },
            validate_id,
        ),
    },
).extend(cv.COMPONENT_SCHEMA)

CANBUS_SCHEMA.add_extra(validate_id)


async def setup_canbus_core_(var, config):
    await cg.register_component(var, config)
    cg.add(var.set_can_id([config[CONF_CAN_ID]]))
    cg.add(var.set_use_extended_id([config[CONF_USE_EXTENDED_ID]]))
    cg.add(var.set_bitrate(CAN_SPEEDS[config[CONF_BIT_RATE]]))

    for conf in config.get(CONF_ON_FRAME, []):
        can_id = conf[CONF_CAN_ID]
        can_id_mask = conf[CONF_CAN_ID_MASK]
        ext_id = conf[CONF_USE_EXTENDED_ID]
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], var, can_id, can_id_mask, ext_id
        )
        if CONF_REMOTE_TRANSMISSION_REQUEST in conf:
            cg.add(
                trigger.set_remote_transmission_request(
                    conf[CONF_REMOTE_TRANSMISSION_REQUEST]
                )
            )
        await cg.register_component(trigger, conf)
        await automation.build_automation(
            trigger,
            [
                (cg.std_vector.template(cg.uint8), "x"),
                (cg.uint32, "can_id"),
                (cg.bool_, "remote_transmission_request"),
            ],
            conf,
        )


async def register_canbus(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.new_Pvariable(config[CONF_ID], var)
    await setup_canbus_core_(var, config)


# Actions
@automation.register_action(
    "canbus.send",
    canbus_ns.class_("CanbusSendAction", automation.Action),
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
            cv.Optional(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
            cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
            cv.Optional(CONF_REMOTE_TRANSMISSION_REQUEST, default=False): cv.boolean,
            cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
        },
        validate_id,
        key=CONF_DATA,
    ),
)
async def canbus_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_CANBUS_ID])

    if CONF_CAN_ID in config:
        can_id = await cg.templatable(config[CONF_CAN_ID], args, cg.uint32)
        cg.add(var.set_can_id(can_id))
    use_extended_id = await cg.templatable(
        config[CONF_USE_EXTENDED_ID], args, cg.uint32
    )
    cg.add(var.set_use_extended_id(use_extended_id))

    remote_transmission_request = await cg.templatable(
        config[CONF_REMOTE_TRANSMISSION_REQUEST], args, bool
    )
    cg.add(var.set_remote_transmission_request(remote_transmission_request))

    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = [int(x) for x in data]
    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var
