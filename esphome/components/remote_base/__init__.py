import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor
from esphome.const import (
    CONF_COMMAND_REPEATS,
    CONF_DATA,
    CONF_TRIGGER_ID,
    CONF_NBITS,
    CONF_ADDRESS,
    CONF_COMMAND,
    CONF_CODE,
    CONF_PULSE_LENGTH,
    CONF_SYNC,
    CONF_ZERO,
    CONF_ONE,
    CONF_INVERTED,
    CONF_PROTOCOL,
    CONF_GROUP,
    CONF_DEVICE,
    CONF_SECOND,
    CONF_STATE,
    CONF_CHANNEL,
    CONF_FAMILY,
    CONF_REPEAT,
    CONF_WAIT_TIME,
    CONF_TIMES,
    CONF_TYPE_ID,
    CONF_CARRIER_FREQUENCY,
    CONF_RC_CODE_1,
    CONF_RC_CODE_2,
    CONF_MAGNITUDE,
    CONF_WAND_ID,
    CONF_LEVEL,
    CONF_DELTA,
    CONF_ID,
    CONF_BUTTON,
    CONF_CHECK,
)
from esphome.core import coroutine
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.util import Registry, SimpleRegistry

AUTO_LOAD = ["binary_sensor"]

CONF_RECEIVER_ID = "receiver_id"
CONF_TRANSMITTER_ID = "transmitter_id"
CONF_FIRST = "first"

ns = remote_base_ns = cg.esphome_ns.namespace("remote_base")
RemoteProtocol = ns.class_("RemoteProtocol")
RemoteReceiverListener = ns.class_("RemoteReceiverListener")
RemoteReceiverBinarySensorBase = ns.class_(
    "RemoteReceiverBinarySensorBase", binary_sensor.BinarySensor, cg.Component
)
RemoteReceiverTrigger = ns.class_(
    "RemoteReceiverTrigger", automation.Trigger, RemoteReceiverListener
)
RemoteTransmitterDumper = ns.class_("RemoteTransmitterDumper")
RemoteTransmittable = ns.class_("RemoteTransmittable")
RemoteTransmitterActionBase = ns.class_(
    "RemoteTransmitterActionBase", RemoteTransmittable, automation.Action
)
RemoteReceiverBase = ns.class_("RemoteReceiverBase")
RemoteTransmitterBase = ns.class_("RemoteTransmitterBase")


def templatize(value):
    if isinstance(value, cv.Schema):
        value = value.schema
    ret = {}
    for key, val in value.items():
        ret[key] = cv.templatable(val)
    return cv.Schema(ret)


REMOTE_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
    }
)


REMOTE_TRANSMITTABLE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(RemoteTransmitterBase),
    }
)


async def register_listener(var, config):
    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    cg.add(receiver.register_listener(var))


async def register_transmittable(var, config):
    transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter_))


def register_binary_sensor(name, type, schema):
    return BINARY_SENSOR_REGISTRY.register(name, type, schema)


def register_trigger(name, type, data_type):
    validator = automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(type),
            cv.Optional(CONF_RECEIVER_ID): cv.invalid(
                "This has been removed in ESPHome 2022.3.0 and the trigger attaches directly to the parent receiver."
            ),
        }
    )
    registerer = TRIGGER_REGISTRY.register(f"on_{name}", validator)

    def decorator(func):
        async def new_func(config):
            var = cg.new_Pvariable(config[CONF_TRIGGER_ID])
            await coroutine(func)(var, config)
            await automation.build_automation(var, [(data_type, "x")], config)
            return var

        return registerer(new_func)

    return decorator


def register_dumper(name, type):
    registerer = DUMPER_REGISTRY.register(name, type, {})

    def decorator(func):
        async def new_func(config, dumper_id):
            var = cg.new_Pvariable(dumper_id)
            await coroutine(func)(var, config)
            return var

        return registerer(new_func)

    return decorator


def validate_repeat(value):
    if isinstance(value, dict):
        return cv.Schema(
            {
                cv.Required(CONF_TIMES): cv.templatable(cv.positive_int),
                cv.Optional(CONF_WAIT_TIME, default="25ms"): cv.templatable(
                    cv.positive_time_period_microseconds
                ),
            }
        )(value)
    return validate_repeat({CONF_TIMES: value})


BASE_REMOTE_TRANSMITTER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REPEAT): validate_repeat,
    }
).extend(REMOTE_TRANSMITTABLE_SCHEMA)


def register_action(name, type_, schema):
    validator = templatize(schema).extend(BASE_REMOTE_TRANSMITTER_SCHEMA)
    registerer = automation.register_action(
        f"remote_transmitter.transmit_{name}", type_, validator
    )

    def decorator(func):
        async def new_func(config, action_id, template_arg, args):
            var = cg.new_Pvariable(action_id, template_arg)
            await register_transmittable(var, config)
            if CONF_REPEAT in config:
                conf = config[CONF_REPEAT]
                template_ = await cg.templatable(conf[CONF_TIMES], args, cg.uint32)
                cg.add(var.set_send_times(template_))
                template_ = await cg.templatable(conf[CONF_WAIT_TIME], args, cg.uint32)
                cg.add(var.set_send_wait(template_))
            await coroutine(func)(var, config, args)
            return var

        return registerer(new_func)

    return decorator


def declare_protocol(name):
    data = ns.struct(f"{name}Data")
    binary_sensor_ = ns.class_(f"{name}BinarySensor", RemoteReceiverBinarySensorBase)
    trigger = ns.class_(f"{name}Trigger", RemoteReceiverTrigger)
    action = ns.class_(f"{name}Action", RemoteTransmitterActionBase)
    dumper = ns.class_(f"{name}Dumper", RemoteTransmitterDumper)
    return data, binary_sensor_, trigger, action, dumper


BINARY_SENSOR_REGISTRY = Registry(
    binary_sensor.binary_sensor_schema().extend(
        {
            cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
        }
    )
)
validate_binary_sensor = cv.validate_registry_entry(
    "remote receiver", BINARY_SENSOR_REGISTRY
)
TRIGGER_REGISTRY = SimpleRegistry()
DUMPER_REGISTRY = Registry(
    {
        cv.Optional(CONF_RECEIVER_ID): cv.invalid(
            "This has been removed in ESPHome 1.20.0 and the dumper attaches directly to the parent receiver."
        ),
    }
)


def validate_dumpers(value):
    if isinstance(value, str) and value.lower() == "all":
        return validate_dumpers(list(DUMPER_REGISTRY.keys()))
    return cv.validate_registry("dumper", DUMPER_REGISTRY)(value)


def validate_triggers(base_schema):
    assert isinstance(base_schema, cv.Schema)

    @schema_extractor("triggers")
    def validator(config):
        added_keys = {}
        for key, (_, valid) in TRIGGER_REGISTRY.items():
            added_keys[cv.Optional(key)] = valid
        new_schema = base_schema.extend(added_keys)

        if config == SCHEMA_EXTRACT:
            return new_schema
        return new_schema(config)

    return validator


async def build_binary_sensor(full_config):
    registry_entry, config = cg.extract_registry_entry_config(
        BINARY_SENSOR_REGISTRY, full_config
    )
    type_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    var = cg.new_Pvariable(type_id)
    await cg.register_component(var, full_config)
    await register_listener(var, full_config)
    await builder(var, config)
    return var


async def build_triggers(full_config):
    triggers = []
    for key in TRIGGER_REGISTRY:
        for config in full_config.get(key, []):
            func = TRIGGER_REGISTRY[key][0]
            triggers.append(await func(config))
    return triggers


async def build_dumpers(config):
    dumpers = []
    for conf in config:
        dumper = await cg.build_registry_entry(DUMPER_REGISTRY, conf)
        dumpers.append(dumper)
    return dumpers


# ByronSX
(
    ByronSXData,
    ByronSXBinarySensor,
    ByronSXTrigger,
    ByronSXAction,
    ByronSXDumper,
) = declare_protocol("ByronSX")
BYRONSX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0xFF)),
        cv.Optional(CONF_COMMAND, default=0x10): cv.All(
            cv.hex_int, cv.one_of(1, 2, 3, 5, 6, 9, 0xD, 0xE, 0x10, int=True)
        ),
    }
)


@register_binary_sensor("byronsx", ByronSXBinarySensor, BYRONSX_SCHEMA)
def byronsx_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                ByronSXData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("byronsx", ByronSXTrigger, ByronSXData)
def byronsx_trigger(var, config):
    pass


@register_dumper("byronsx", ByronSXDumper)
def byronsx_dumper(var, config):
    pass


@register_action("byronsx", ByronSXAction, BYRONSX_SCHEMA)
async def byronsx_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# CanalSat
(
    CanalSatData,
    CanalSatBinarySensor,
    CanalSatTrigger,
    CanalSatAction,
    CanalSatDumper,
) = declare_protocol("CanalSat")
CANALSAT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEVICE): cv.hex_uint8_t,
        cv.Optional(CONF_ADDRESS, default=0): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("canalsat", CanalSatBinarySensor, CANALSAT_SCHEMA)
def canalsat_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                CanalSatData,
                ("device", config[CONF_DEVICE]),
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("canalsat", CanalSatTrigger, CanalSatData)
def canalsat_trigger(var, config):
    pass


@register_dumper("canalsat", CanalSatDumper)
def canalsat_dumper(var, config):
    pass


@register_action("canalsat", CanalSatAction, CANALSAT_SCHEMA)
async def canalsat_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DEVICE], args, cg.uint8)
    cg.add(var.set_device(template_))
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


(
    CanalSatLDData,
    CanalSatLDBinarySensor,
    CanalSatLDTrigger,
    CanalSatLDAction,
    CanalSatLDDumper,
) = declare_protocol("CanalSatLD")
CANALSATLD_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEVICE): cv.hex_uint8_t,
        cv.Optional(CONF_ADDRESS, default=0): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("canalsatld", CanalSatLDBinarySensor, CANALSAT_SCHEMA)
def canalsatld_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                CanalSatLDData,
                ("device", config[CONF_DEVICE]),
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("canalsatld", CanalSatLDTrigger, CanalSatLDData)
def canalsatld_trigger(var, config):
    pass


@register_dumper("canalsatld", CanalSatLDDumper)
def canalsatld_dumper(var, config):
    pass


@register_action("canalsatld", CanalSatLDAction, CANALSATLD_SCHEMA)
async def canalsatld_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DEVICE], args, cg.uint8)
    cg.add(var.set_device(template_))
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# Coolix
(
    CoolixData,
    CoolixBinarySensor,
    CoolixTrigger,
    CoolixAction,
    CoolixDumper,
) = declare_protocol("Coolix")


COOLIX_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FIRST): cv.hex_int_range(0, 16777215),
        cv.Optional(CONF_SECOND, default=0): cv.hex_int_range(0, 16777215),
        cv.Optional(CONF_DATA): cv.invalid(
            "'data' option has been removed in ESPHome 2023.8. "
            "Use the 'first' and 'second' options instead."
        ),
    }
)

COOLIX_SENSOR_SCHEMA = cv.Any(cv.hex_int_range(0, 16777215), COOLIX_BASE_SCHEMA)


@register_binary_sensor("coolix", CoolixBinarySensor, COOLIX_SENSOR_SCHEMA)
def coolix_binary_sensor(var, config):
    if isinstance(config, dict):
        cg.add(
            var.set_data(
                cg.ArrayInitializer(
                    config[CONF_FIRST],
                    config[CONF_SECOND],
                )
            )
        )
    else:
        cg.add(var.set_data(cg.ArrayInitializer(0, config)))


@register_action("coolix", CoolixAction, COOLIX_BASE_SCHEMA)
async def coolix_action(var, config, args):
    template_ = await cg.templatable(config[CONF_FIRST], args, cg.uint32)
    cg.add(var.set_first(template_))
    template_ = await cg.templatable(config[CONF_SECOND], args, cg.uint32)
    cg.add(var.set_second(template_))


@register_trigger("coolix", CoolixTrigger, CoolixData)
def coolix_trigger(var, config):
    pass


@register_dumper("coolix", CoolixDumper)
def coolix_dumper(var, config):
    pass


# Dish
DishData, DishBinarySensor, DishTrigger, DishAction, DishDumper = declare_protocol(
    "Dish"
)
DISH_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=16),
        cv.Required(CONF_COMMAND): cv.int_range(min=0, max=63),
    }
)


@register_binary_sensor("dish", DishBinarySensor, DISH_SCHEMA)
def dish_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DishData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("dish", DishTrigger, DishData)
def dish_trigger(var, config):
    pass


@register_dumper("dish", DishDumper)
def dish_dumper(var, config):
    pass


@register_action("dish", DishAction, DISH_SCHEMA)
async def dish_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# Dooya
DooyaData, DooyaBinarySensor, DooyaTrigger, DooyaAction, DooyaDumper = declare_protocol(
    "Dooya"
)
DOOYA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.hex_int_range(0, 16777215),
        cv.Required(CONF_CHANNEL): cv.hex_int_range(0, 255),
        cv.Required(CONF_BUTTON): cv.hex_int_range(0, 15),
        cv.Required(CONF_CHECK): cv.hex_int_range(0, 15),
    }
)


@register_binary_sensor("dooya", DooyaBinarySensor, DOOYA_SCHEMA)
def dooya_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DooyaData,
                ("id", config[CONF_ID]),
                ("channel", config[CONF_CHANNEL]),
                ("button", config[CONF_BUTTON]),
                ("check", config[CONF_CHECK]),
            )
        )
    )


@register_trigger("dooya", DooyaTrigger, DooyaData)
def dooya_trigger(var, config):
    pass


@register_dumper("dooya", DooyaDumper)
def dooya_dumper(var, config):
    pass


@register_action("dooya", DooyaAction, DOOYA_SCHEMA)
async def dooya_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ID], args, cg.uint32)
    cg.add(var.set_id(template_))
    template_ = await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)
    cg.add(var.set_channel(template_))
    template_ = await cg.templatable(config[CONF_BUTTON], args, cg.uint8)
    cg.add(var.set_button(template_))
    template_ = await cg.templatable(config[CONF_CHECK], args, cg.uint8)
    cg.add(var.set_check(template_))


# JVC
JVCData, JVCBinarySensor, JVCTrigger, JVCAction, JVCDumper = declare_protocol("JVC")
JVC_SCHEMA = cv.Schema({cv.Required(CONF_DATA): cv.hex_uint32_t})


@register_binary_sensor("jvc", JVCBinarySensor, JVC_SCHEMA)
def jvc_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                JVCData,
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("jvc", JVCTrigger, JVCData)
def jvc_trigger(var, config):
    pass


@register_dumper("jvc", JVCDumper)
def jvc_dumper(var, config):
    pass


@register_action("jvc", JVCAction, JVC_SCHEMA)
async def jvc_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DATA], args, cg.uint32)
    cg.add(var.set_data(template_))


# LG
LGData, LGBinarySensor, LGTrigger, LGAction, LGDumper = declare_protocol("LG")
LG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint32_t,
        cv.Optional(CONF_NBITS, default=28): cv.one_of(28, 32, int=True),
    }
)


@register_binary_sensor("lg", LGBinarySensor, LG_SCHEMA)
def lg_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                LGData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("lg", LGTrigger, LGData)
def lg_trigger(var, config):
    pass


@register_dumper("lg", LGDumper)
def lg_dumper(var, config):
    pass


@register_action("lg", LGAction, LG_SCHEMA)
async def lg_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DATA], args, cg.uint32)
    cg.add(var.set_data(template_))
    template_ = await cg.templatable(config[CONF_NBITS], args, cg.uint8)
    cg.add(var.set_nbits(template_))


# MagiQuest
(
    MagiQuestData,
    MagiQuestBinarySensor,
    MagiQuestTrigger,
    MagiQuestAction,
    MagiQuestDumper,
) = declare_protocol("MagiQuest")

MAGIQUEST_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_WAND_ID): cv.hex_uint32_t,
        cv.Optional(CONF_MAGNITUDE, default=0xFFFF): cv.hex_uint16_t,
    }
)


@register_binary_sensor("magiquest", MagiQuestBinarySensor, MAGIQUEST_SCHEMA)
def magiquest_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                MagiQuestData,
                ("magnitude", config[CONF_MAGNITUDE]),
                ("wand_id", config[CONF_WAND_ID]),
            )
        )
    )


@register_trigger("magiquest", MagiQuestTrigger, MagiQuestData)
def magiquest_trigger(var, config):
    pass


@register_dumper("magiquest", MagiQuestDumper)
def magiquest_dumper(var, config):
    pass


@register_action("magiquest", MagiQuestAction, MAGIQUEST_SCHEMA)
async def magiquest_action(var, config, args):
    template_ = await cg.templatable(config[CONF_WAND_ID], args, cg.uint32)
    cg.add(var.set_wand_id(template_))
    template_ = await cg.templatable(config[CONF_MAGNITUDE], args, cg.uint16)
    cg.add(var.set_magnitude(template_))


# Microchip HCS301 KeeLoq OOK
(
    KeeloqData,
    KeeloqBinarySensor,
    KeeloqTrigger,
    KeeloqAction,
    KeeloqDumper,
) = declare_protocol("Keeloq")
KEELOQ_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFFFFF)),
        cv.Required(CONF_CODE): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFFFFFF)),
        cv.Optional(CONF_COMMAND, default=0x10): cv.All(
            cv.hex_int,
            cv.Range(min=0, max=0x10),
        ),
        cv.Optional(CONF_LEVEL, default=False): cv.boolean,
    }
)


@register_binary_sensor("keeloq", KeeloqBinarySensor, KEELOQ_SCHEMA)
def Keeloq_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                KeeloqData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("keeloq", KeeloqTrigger, KeeloqData)
def keeloq_trigger(var, config):
    pass


@register_dumper("keeloq", KeeloqDumper)
def keeloq_dumper(var, config):
    pass


@register_action("keeloq", KeeloqAction, KEELOQ_SCHEMA)
async def keeloq_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint32)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_CODE], args, cg.uint32)
    cg.add(var.set_encrypted(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))
    template_ = await cg.templatable(config[CONF_LEVEL], args, bool)
    cg.add(var.set_vlow(template_))


# NEC
NECData, NECBinarySensor, NECTrigger, NECAction, NECDumper = declare_protocol("NEC")
NEC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_COMMAND): cv.hex_uint16_t,
        cv.Optional(CONF_COMMAND_REPEATS, default=1): cv.uint16_t,
    }
)


@register_binary_sensor("nec", NECBinarySensor, NEC_SCHEMA)
def nec_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                NECData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
                ("command_repeats", config[CONF_COMMAND_REPEATS]),
            )
        )
    )


@register_trigger("nec", NECTrigger, NECData)
def nec_trigger(var, config):
    pass


@register_dumper("nec", NECDumper)
def nec_dumper(var, config):
    pass


@register_action("nec", NECAction, NEC_SCHEMA)
async def nec_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint16)
    cg.add(var.set_command(template_))
    template_ = await cg.templatable(config[CONF_COMMAND_REPEATS], args, cg.uint16)
    cg.add(var.set_command_repeats(template_))


# Pioneer
(
    PioneerData,
    PioneerBinarySensor,
    PioneerTrigger,
    PioneerAction,
    PioneerDumper,
) = declare_protocol("Pioneer")
PIONEER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RC_CODE_1): cv.hex_uint16_t,
        cv.Optional(CONF_RC_CODE_2, default=0): cv.hex_uint16_t,
    }
)


@register_binary_sensor("pioneer", PioneerBinarySensor, PIONEER_SCHEMA)
def pioneer_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                PioneerData,
                ("rc_code_1", config[CONF_RC_CODE_1]),
                ("rc_code_2", config[CONF_RC_CODE_2]),
            )
        )
    )


@register_trigger("pioneer", PioneerTrigger, PioneerData)
def pioneer_trigger(var, config):
    pass


@register_dumper("pioneer", PioneerDumper)
def pioneer_dumper(var, config):
    pass


@register_action("pioneer", PioneerAction, PIONEER_SCHEMA)
async def pioneer_action(var, config, args):
    template_ = await cg.templatable(config[CONF_RC_CODE_1], args, cg.uint16)
    cg.add(var.set_rc_code_1(template_))
    template_ = await cg.templatable(config[CONF_RC_CODE_2], args, cg.uint16)
    cg.add(var.set_rc_code_2(template_))


# Pronto
(
    ProntoData,
    ProntoBinarySensor,
    ProntoTrigger,
    ProntoAction,
    ProntoDumper,
) = declare_protocol("Pronto")
PRONTO_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.string,
        cv.Optional(CONF_DELTA, default=-1): cv.int_,
    }
)


@register_binary_sensor("pronto", ProntoBinarySensor, PRONTO_SCHEMA)
def pronto_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                ProntoData,
                ("data", config[CONF_DATA]),
                ("delta", config[CONF_DELTA]),
            )
        )
    )


@register_trigger("pronto", ProntoTrigger, ProntoData)
def pronto_trigger(var, config):
    pass


@register_dumper("pronto", ProntoDumper)
def pronto_dumper(var, config):
    pass


@register_action("pronto", ProntoAction, PRONTO_SCHEMA)
async def pronto_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DATA], args, cg.std_string)
    cg.add(var.set_data(template_))


# Roomba
(
    RoombaData,
    RoombaBinarySensor,
    RoombaTrigger,
    RoombaAction,
    RoombaDumper,
) = declare_protocol("Roomba")
ROOMBA_SCHEMA = cv.Schema({cv.Required(CONF_DATA): cv.hex_uint8_t})


@register_binary_sensor("roomba", RoombaBinarySensor, ROOMBA_SCHEMA)
def roomba_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RoombaData,
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("roomba", RoombaTrigger, RoombaData)
def roomba_trigger(var, config):
    pass


@register_dumper("roomba", RoombaDumper)
def roomba_dumper(var, config):
    pass


@register_action("roomba", RoombaAction, ROOMBA_SCHEMA)
async def roomba_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DATA], args, cg.uint8)
    cg.add(var.set_data(template_))


# Sony
SonyData, SonyBinarySensor, SonyTrigger, SonyAction, SonyDumper = declare_protocol(
    "Sony"
)
SONY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint32_t,
        cv.Optional(CONF_NBITS, default=12): cv.one_of(12, 15, 20, int=True),
    }
)


@register_binary_sensor("sony", SonyBinarySensor, SONY_SCHEMA)
def sony_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                SonyData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("sony", SonyTrigger, SonyData)
def sony_trigger(var, config):
    pass


@register_dumper("sony", SonyDumper)
def sony_dumper(var, config):
    pass


@register_action("sony", SonyAction, SONY_SCHEMA)
async def sony_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DATA], args, cg.uint32)
    cg.add(var.set_data(template_))
    template_ = await cg.templatable(config[CONF_NBITS], args, cg.uint32)
    cg.add(var.set_nbits(template_))


# Raw
def validate_raw_alternating(value):
    assert isinstance(value, list)
    last_negative = None
    for i, val in enumerate(value):
        this_negative = val < 0
        if i != 0:
            if this_negative == last_negative:
                raise cv.Invalid(
                    f"Values must alternate between being positive and negative, please see index {i} and {i + 1}",
                    [i],
                )
        last_negative = this_negative
    return value


RawData, RawBinarySensor, RawTrigger, RawAction, RawDumper = declare_protocol("Raw")
CONF_CODE_STORAGE_ID = "code_storage_id"
RAW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All(
            [cv.Any(cv.int_, cv.time_period_microseconds)],
            cv.Length(min=1),
            validate_raw_alternating,
        ),
        cv.GenerateID(CONF_CODE_STORAGE_ID): cv.declare_id(cg.int32),
    }
)


@register_binary_sensor("raw", RawBinarySensor, RAW_SCHEMA)
def raw_binary_sensor(var, config):
    code_ = config[CONF_CODE]
    arr = cg.progmem_array(config[CONF_CODE_STORAGE_ID], code_)
    cg.add(var.set_data(arr))
    cg.add(var.set_len(len(code_)))


@register_trigger("raw", RawTrigger, cg.std_vector.template(cg.int32))
def raw_trigger(var, config):
    pass


@register_dumper("raw", RawDumper)
def raw_dumper(var, config):
    pass


@register_action(
    "raw",
    RawAction,
    RAW_SCHEMA.extend(
        {
            cv.Optional(CONF_CARRIER_FREQUENCY, default="0Hz"): cv.All(
                cv.frequency, cv.int_
            ),
        }
    ),
)
async def raw_action(var, config, args):
    code_ = config[CONF_CODE]
    if cg.is_template(code_):
        template_ = await cg.templatable(code_, args, cg.std_vector.template(cg.int32))
        cg.add(var.set_code_template(template_))
    else:
        code_ = config[CONF_CODE]
        arr = cg.progmem_array(config[CONF_CODE_STORAGE_ID], code_)
        cg.add(var.set_code_static(arr, len(code_)))
    templ = await cg.templatable(config[CONF_CARRIER_FREQUENCY], args, cg.uint32)
    cg.add(var.set_carrier_frequency(templ))


# Drayton
(
    DraytonData,
    DraytonBinarySensor,
    DraytonTrigger,
    DraytonAction,
    DraytonDumper,
) = declare_protocol("Drayton")
DRAYTON_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFF)),
        cv.Required(CONF_CHANNEL): cv.All(cv.hex_int, cv.Range(min=0, max=0x1F)),
        cv.Required(CONF_COMMAND): cv.All(cv.hex_int, cv.Range(min=0, max=0x7F)),
    }
)


@register_binary_sensor("drayton", DraytonBinarySensor, DRAYTON_SCHEMA)
def drayton_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DraytonData,
                ("address", config[CONF_ADDRESS]),
                ("channel", config[CONF_CHANNEL]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("drayton", DraytonTrigger, DraytonData)
def drayton_trigger(var, config):
    pass


@register_dumper("drayton", DraytonDumper)
def drayton_dumper(var, config):
    pass


@register_action("drayton", DraytonAction, DRAYTON_SCHEMA)
async def drayton_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)
    cg.add(var.set_channel(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# RC5
RC5Data, RC5BinarySensor, RC5Trigger, RC5Action, RC5Dumper = declare_protocol("RC5")
RC5_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0x1F)),
        cv.Required(CONF_COMMAND): cv.All(cv.hex_int, cv.Range(min=0, max=0x7F)),
    }
)


@register_binary_sensor("rc5", RC5BinarySensor, RC5_SCHEMA)
def rc5_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RC5Data,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("rc5", RC5Trigger, RC5Data)
def rc5_trigger(var, config):
    pass


@register_dumper("rc5", RC5Dumper)
def rc5_dumper(var, config):
    pass


@register_action("rc5", RC5Action, RC5_SCHEMA)
async def rc5_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# RC6
RC6Data, RC6BinarySensor, RC6Trigger, RC6Action, RC6Dumper = declare_protocol("RC6")
RC6_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("rc6", RC6BinarySensor, RC6_SCHEMA)
def rc6_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RC6Data,
                ("mode", 0),
                ("toggle", 0),
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("rc6", RC6Trigger, RC6Data)
def rc6_trigger(var, config):
    pass


@register_dumper("rc6", RC6Dumper)
def rc6_dumper(var, config):
    pass


@register_action("rc6", RC6Action, RC6_SCHEMA)
async def rc6_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# RC Switch Raw
RC_SWITCH_TIMING_SCHEMA = cv.All([cv.uint8_t], cv.Length(min=2, max=2))

RC_SWITCH_PROTOCOL_SCHEMA = cv.Any(
    cv.int_range(min=1, max=8),
    cv.Schema(
        {
            cv.Required(CONF_PULSE_LENGTH): cv.uint32_t,
            cv.Optional(CONF_SYNC, default=[1, 31]): RC_SWITCH_TIMING_SCHEMA,
            cv.Optional(CONF_ZERO, default=[1, 3]): RC_SWITCH_TIMING_SCHEMA,
            cv.Optional(CONF_ONE, default=[3, 1]): RC_SWITCH_TIMING_SCHEMA,
            cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        }
    ),
)


def validate_rc_switch_code(value):
    if not isinstance(value, str):
        raise cv.Invalid("All RCSwitch codes must be in quotes ('')")
    for c in value:
        if c not in ("0", "1"):
            raise cv.Invalid(
                f"Invalid RCSwitch code character '{c}'. Only '0' and '1' are allowed"
            )
    if len(value) > 64:
        raise cv.Invalid(
            f"Maximum length for RCSwitch codes is 64, code '{value}' has length {len(value)}"
        )
    if not value:
        raise cv.Invalid("RCSwitch code must not be empty")
    return value


def validate_rc_switch_raw_code(value):
    if not isinstance(value, str):
        raise cv.Invalid("All RCSwitch raw codes must be in quotes ('')")
    for c in value:
        if c not in ("0", "1", "x"):
            raise cv.Invalid(
                f"Invalid RCSwitch raw code character '{c}'.Only '0', '1' and 'x' are allowed"
            )
    if len(value) > 64:
        raise cv.Invalid(
            f"Maximum length for RCSwitch raw codes is 64, code '{value}' has length {len(value)}"
        )
    if not value:
        raise cv.Invalid("RCSwitch raw code must not be empty")
    return value


def build_rc_switch_protocol(config):
    if isinstance(config, int):
        return rc_switch_protocols[config]
    pl = config[CONF_PULSE_LENGTH]
    return RCSwitchBase(
        config[CONF_SYNC][0] * pl,
        config[CONF_SYNC][1] * pl,
        config[CONF_ZERO][0] * pl,
        config[CONF_ZERO][1] * pl,
        config[CONF_ONE][0] * pl,
        config[CONF_ONE][1] * pl,
        config[CONF_INVERTED],
    )


RC_SWITCH_RAW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): validate_rc_switch_raw_code,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_A_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GROUP): cv.All(
            validate_rc_switch_code, cv.Length(min=5, max=5)
        ),
        cv.Required(CONF_DEVICE): cv.All(
            validate_rc_switch_code, cv.Length(min=5, max=5)
        ),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_B_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=4),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_C_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FAMILY): cv.one_of(
            "a",
            "b",
            "c",
            "d",
            "e",
            "f",
            "g",
            "h",
            "i",
            "j",
            "k",
            "l",
            "m",
            "n",
            "o",
            "p",
            lower=True,
        ),
        cv.Required(CONF_GROUP): cv.int_range(min=1, max=4),
        cv.Required(CONF_DEVICE): cv.int_range(min=1, max=4),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_D_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GROUP): cv.one_of("a", "b", "c", "d", lower=True),
        cv.Required(CONF_DEVICE): cv.int_range(min=1, max=3),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TRANSMITTER = cv.Schema(
    {
        cv.Optional(CONF_REPEAT, default={CONF_TIMES: 5}): cv.Schema(
            {
                cv.Required(CONF_TIMES): cv.templatable(cv.positive_int),
                cv.Optional(CONF_WAIT_TIME, default="0us"): cv.templatable(
                    cv.positive_time_period_microseconds
                ),
            }
        ),
    }
)

rc_switch_protocols = ns.RC_SWITCH_PROTOCOLS
RCSwitchData = ns.struct("RCSwitchData")
RCSwitchBase = ns.class_("RCSwitchBase")
RCSwitchTrigger = ns.class_("RCSwitchTrigger", RemoteReceiverTrigger)
RCSwitchDumper = ns.class_("RCSwitchDumper", RemoteTransmitterDumper)
RCSwitchRawAction = ns.class_("RCSwitchRawAction", RemoteTransmitterActionBase)
RCSwitchTypeAAction = ns.class_("RCSwitchTypeAAction", RemoteTransmitterActionBase)
RCSwitchTypeBAction = ns.class_("RCSwitchTypeBAction", RemoteTransmitterActionBase)
RCSwitchTypeCAction = ns.class_("RCSwitchTypeCAction", RemoteTransmitterActionBase)
RCSwitchTypeDAction = ns.class_("RCSwitchTypeDAction", RemoteTransmitterActionBase)
RCSwitchRawReceiver = ns.class_("RCSwitchRawReceiver", RemoteReceiverBinarySensorBase)


@register_binary_sensor("rc_switch_raw", RCSwitchRawReceiver, RC_SWITCH_RAW_SCHEMA)
def rc_switch_raw_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_code(config[CONF_CODE]))


@register_action(
    "rc_switch_raw",
    RCSwitchRawAction,
    RC_SWITCH_RAW_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_raw_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=build_rc_switch_protocol
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_code(await cg.templatable(config[CONF_CODE], args, cg.std_string)))


@register_binary_sensor(
    "rc_switch_type_a", RCSwitchRawReceiver, RC_SWITCH_TYPE_A_SCHEMA
)
def rc_switch_type_a_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_type_a(config[CONF_GROUP], config[CONF_DEVICE], config[CONF_STATE]))


@register_action(
    "rc_switch_type_a",
    RCSwitchTypeAAction,
    RC_SWITCH_TYPE_A_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_a_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=build_rc_switch_protocol
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.std_string)))
    cg.add(
        var.set_device(await cg.templatable(config[CONF_DEVICE], args, cg.std_string))
    )
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, bool)))


@register_binary_sensor(
    "rc_switch_type_b", RCSwitchRawReceiver, RC_SWITCH_TYPE_B_SCHEMA
)
def rc_switch_type_b_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(
        var.set_type_b(config[CONF_ADDRESS], config[CONF_CHANNEL], config[CONF_STATE])
    )


@register_action(
    "rc_switch_type_b",
    RCSwitchTypeBAction,
    RC_SWITCH_TYPE_B_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_b_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=build_rc_switch_protocol
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_address(await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)))
    cg.add(var.set_channel(await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, bool)))


@register_binary_sensor(
    "rc_switch_type_c", RCSwitchRawReceiver, RC_SWITCH_TYPE_C_SCHEMA
)
def rc_switch_type_c_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(
        var.set_type_c(
            config[CONF_FAMILY],
            config[CONF_GROUP],
            config[CONF_DEVICE],
            config[CONF_STATE],
        )
    )


@register_action(
    "rc_switch_type_c",
    RCSwitchTypeCAction,
    RC_SWITCH_TYPE_C_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_c_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=build_rc_switch_protocol
    )
    cg.add(var.set_protocol(proto))
    cg.add(
        var.set_family(await cg.templatable(config[CONF_FAMILY], args, cg.std_string))
    )
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.uint8)))
    cg.add(var.set_device(await cg.templatable(config[CONF_DEVICE], args, cg.uint8)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, bool)))


@register_binary_sensor(
    "rc_switch_type_d",
    RCSwitchRawReceiver,
    RC_SWITCH_TYPE_D_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
def rc_switch_type_d_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_type_d(config[CONF_GROUP], config[CONF_DEVICE], config[CONF_STATE]))


@register_action(
    "rc_switch_type_d",
    RCSwitchTypeDAction,
    RC_SWITCH_TYPE_D_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_d_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=build_rc_switch_protocol
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.std_string)))
    cg.add(var.set_device(await cg.templatable(config[CONF_DEVICE], args, cg.uint8)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, bool)))


@register_trigger("rc_switch", RCSwitchTrigger, RCSwitchData)
def rc_switch_trigger(var, config):
    pass


@register_dumper("rc_switch", RCSwitchDumper)
def rc_switch_dumper(var, config):
    pass


# Samsung
(
    SamsungData,
    SamsungBinarySensor,
    SamsungTrigger,
    SamsungAction,
    SamsungDumper,
) = declare_protocol("Samsung")
SAMSUNG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint64_t,
        cv.Optional(CONF_NBITS, default=32): cv.int_range(32, 64),
    }
)


@register_binary_sensor("samsung", SamsungBinarySensor, SAMSUNG_SCHEMA)
def samsung_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                SamsungData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("samsung", SamsungTrigger, SamsungData)
def samsung_trigger(var, config):
    pass


@register_dumper("samsung", SamsungDumper)
def samsung_dumper(var, config):
    pass


@register_action("samsung", SamsungAction, SAMSUNG_SCHEMA)
async def samsung_action(var, config, args):
    template_ = await cg.templatable(config[CONF_DATA], args, cg.uint64)
    cg.add(var.set_data(template_))
    template_ = await cg.templatable(config[CONF_NBITS], args, cg.uint8)
    cg.add(var.set_nbits(template_))


# Samsung36
(
    Samsung36Data,
    Samsung36BinarySensor,
    Samsung36Trigger,
    Samsung36Action,
    Samsung36Dumper,
) = declare_protocol("Samsung36")
SAMSUNG36_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_COMMAND): cv.hex_uint32_t,
    }
)


@register_binary_sensor("samsung36", Samsung36BinarySensor, SAMSUNG36_SCHEMA)
def samsung36_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                Samsung36Data,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("samsung36", Samsung36Trigger, Samsung36Data)
def samsung36_trigger(var, config):
    pass


@register_dumper("samsung36", Samsung36Dumper)
def samsung36_dumper(var, config):
    pass


@register_action("samsung36", Samsung36Action, SAMSUNG36_SCHEMA)
async def samsung36_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint32)
    cg.add(var.set_command(template_))


# Toshiba AC
(
    ToshibaAcData,
    ToshibaAcBinarySensor,
    ToshibaAcTrigger,
    ToshibaAcAction,
    ToshibaAcDumper,
) = declare_protocol("ToshibaAc")
TOSHIBAAC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RC_CODE_1): cv.hex_uint64_t,
        cv.Optional(CONF_RC_CODE_2, default=0): cv.hex_uint64_t,
    }
)


@register_binary_sensor("toshiba_ac", ToshibaAcBinarySensor, TOSHIBAAC_SCHEMA)
def toshibaac_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                ToshibaAcData,
                ("rc_code_1", config[CONF_RC_CODE_1]),
                ("rc_code_2", config[CONF_RC_CODE_2]),
            )
        )
    )


@register_trigger("toshiba_ac", ToshibaAcTrigger, ToshibaAcData)
def toshibaac_trigger(var, config):
    pass


@register_dumper("toshiba_ac", ToshibaAcDumper)
def toshibaac_dumper(var, config):
    pass


@register_action("toshiba_ac", ToshibaAcAction, TOSHIBAAC_SCHEMA)
async def toshibaac_action(var, config, args):
    template_ = await cg.templatable(config[CONF_RC_CODE_1], args, cg.uint64)
    cg.add(var.set_rc_code_1(template_))
    template_ = await cg.templatable(config[CONF_RC_CODE_2], args, cg.uint64)
    cg.add(var.set_rc_code_2(template_))


# Panasonic
(
    PanasonicData,
    PanasonicBinarySensor,
    PanasonicTrigger,
    PanasonicAction,
    PanasonicDumper,
) = declare_protocol("Panasonic")
PANASONIC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_COMMAND): cv.hex_uint32_t,
    }
)


@register_binary_sensor("panasonic", PanasonicBinarySensor, PANASONIC_SCHEMA)
def panasonic_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                PanasonicData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("panasonic", PanasonicTrigger, PanasonicData)
def panasonic_trigger(var, config):
    pass


@register_dumper("panasonic", PanasonicDumper)
def panasonic_dumper(var, config):
    pass


@register_action("panasonic", PanasonicAction, PANASONIC_SCHEMA)
async def panasonic_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.uint32)
    cg.add(var.set_command(template_))


# Nexa
NexaData, NexaBinarySensor, NexaTrigger, NexaAction, NexaDumper = declare_protocol(
    "Nexa"
)
NEXA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEVICE): cv.hex_uint32_t,
        cv.Required(CONF_GROUP): cv.hex_uint8_t,
        cv.Required(CONF_STATE): cv.hex_uint8_t,
        cv.Required(CONF_CHANNEL): cv.hex_uint8_t,
        cv.Required(CONF_LEVEL): cv.hex_uint8_t,
    }
)


@register_binary_sensor("nexa", NexaBinarySensor, NEXA_SCHEMA)
def nexa_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                NexaData,
                ("device", config[CONF_DEVICE]),
                ("group", config[CONF_GROUP]),
                ("state", config[CONF_STATE]),
                ("channel", config[CONF_CHANNEL]),
                ("level", config[CONF_LEVEL]),
            )
        )
    )


@register_trigger("nexa", NexaTrigger, NexaData)
def nexa_trigger(var, config):
    pass


@register_dumper("nexa", NexaDumper)
def nexa_dumper(var, config):
    pass


@register_action("nexa", NexaAction, NEXA_SCHEMA)
def nexa_action(var, config, args):
    cg.add(var.set_device((yield cg.templatable(config[CONF_DEVICE], args, cg.uint32))))
    cg.add(var.set_group((yield cg.templatable(config[CONF_GROUP], args, cg.uint8))))
    cg.add(var.set_state((yield cg.templatable(config[CONF_STATE], args, cg.uint8))))
    cg.add(
        var.set_channel((yield cg.templatable(config[CONF_CHANNEL], args, cg.uint8)))
    )
    cg.add(var.set_level((yield cg.templatable(config[CONF_LEVEL], args, cg.uint8))))


# Midea
MideaData, MideaBinarySensor, MideaTrigger, MideaAction, MideaDumper = declare_protocol(
    "Midea"
)
MideaAction = ns.class_("MideaAction", RemoteTransmitterActionBase)
MIDEA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All([cv.hex_uint8_t], cv.Length(min=5, max=5)),
    }
)


@register_binary_sensor("midea", MideaBinarySensor, MIDEA_SCHEMA)
def midea_binary_sensor(var, config):
    cg.add(var.set_data(config[CONF_CODE]))


@register_trigger("midea", MideaTrigger, MideaData)
def midea_trigger(var, config):
    pass


@register_dumper("midea", MideaDumper)
def midea_dumper(var, config):
    pass


@register_action("midea", MideaAction, MIDEA_SCHEMA)
async def midea_action(var, config, args):
    vec_ = cg.std_vector.template(cg.uint8)
    template_ = await cg.templatable(config[CONF_CODE], args, vec_, vec_)
    cg.add(var.set_code(template_))


# AEHA
AEHAData, AEHABinarySensor, AEHATrigger, AEHAAction, AEHADumper = declare_protocol(
    "AEHA"
)
AEHA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_DATA): cv.All([cv.hex_uint8_t], cv.Length(min=2, max=35)),
    }
)


@register_binary_sensor("aeha", AEHABinarySensor, AEHA_SCHEMA)
def aeha_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                AEHAData,
                ("address", config[CONF_ADDRESS]),
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("aeha", AEHATrigger, AEHAData)
def aeha_trigger(var, config):
    pass


@register_dumper("aeha", AEHADumper)
def aeha_dumper(var, config):
    pass


@register_action("aeha", AEHAAction, AEHA_SCHEMA)
async def aeha_action(var, config, args):
    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = await cg.templatable(
        config[CONF_DATA], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_data(template_))


# Haier
HaierData, HaierBinarySensor, HaierTrigger, HaierAction, HaierDumper = declare_protocol(
    "Haier"
)
HaierAction = ns.class_("HaierAction", RemoteTransmitterActionBase)
HAIER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All([cv.hex_uint8_t], cv.Length(min=13, max=13)),
    }
)


@register_binary_sensor("haier", HaierBinarySensor, HAIER_SCHEMA)
def haier_binary_sensor(var, config):
    cg.add(var.set_code(config[CONF_CODE]))


@register_trigger("haier", HaierTrigger, HaierData)
def haier_trigger(var, config):
    pass


@register_dumper("haier", HaierDumper)
def haier_dumper(var, config):
    pass


@register_action("haier", HaierAction, HAIER_SCHEMA)
async def haier_action(var, config, args):
    vec_ = cg.std_vector.template(cg.uint8)
    template_ = await cg.templatable(config[CONF_CODE], args, vec_, vec_)
    cg.add(var.set_code(template_))


# ABBWelcome
(
    ABBWelcomeData,
    ABBWelcomeBinarySensor,
    ABBWelcomeTrigger,
    ABBWelcomeAction,
    ABBWelcomeDumper,
) = declare_protocol("ABBWelcome")

CONF_SOURCE_ADDRESS = "source_address"
CONF_DESTINATION_ADDRESS = "destination_address"
CONF_THREE_BYTE_ADDRESS = "three_byte_address"
CONF_MESSAGE_TYPE = "message_type"
CONF_MESSAGE_ID = "message_id"
CONF_RETRANSMISSION = "retransmission"

ABB_WELCOME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE_ADDRESS): cv.hex_uint32_t,
        cv.Required(CONF_DESTINATION_ADDRESS): cv.hex_uint32_t,
        cv.Optional(CONF_RETRANSMISSION, default=False): cv.boolean,
        cv.Optional(CONF_THREE_BYTE_ADDRESS, default=False): cv.boolean,
        cv.Required(CONF_MESSAGE_TYPE): cv.Any(cv.hex_uint8_t, cv.uint8_t),
        cv.Optional(CONF_MESSAGE_ID): cv.Any(cv.hex_uint8_t, cv.uint8_t),
        cv.Optional(CONF_DATA): cv.All(
            [cv.Any(cv.hex_uint8_t, cv.uint8_t)],
            cv.Length(min=0, max=7),
        ),
    }
)


@register_binary_sensor("abbwelcome", ABBWelcomeBinarySensor, ABB_WELCOME_SCHEMA)
def abbwelcome_binary_sensor(var, config):
    cg.add(var.set_three_byte_address(config[CONF_THREE_BYTE_ADDRESS]))
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_destination_address(config[CONF_DESTINATION_ADDRESS]))
    cg.add(var.set_retransmission(config[CONF_RETRANSMISSION]))
    cg.add(var.set_message_type(config[CONF_MESSAGE_TYPE]))
    cg.add(var.set_auto_message_id(CONF_MESSAGE_ID not in config))
    if CONF_MESSAGE_ID in config:
        cg.add(var.set_message_id(config[CONF_MESSAGE_ID]))
    if CONF_DATA in config:
        cg.add(var.set_data(config[CONF_DATA]))
    cg.add(var.finalize())


@register_trigger("abbwelcome", ABBWelcomeTrigger, ABBWelcomeData)
def abbwelcome_trigger(var, config):
    pass


@register_dumper("abbwelcome", ABBWelcomeDumper)
def abbwelcome_dumper(var, config):
    pass


@register_action("abbwelcome", ABBWelcomeAction, ABB_WELCOME_SCHEMA)
async def abbwelcome_action(var, config, args):
    cg.add(
        var.set_three_byte_address(
            await cg.templatable(config[CONF_THREE_BYTE_ADDRESS], args, cg.bool_)
        )
    )
    cg.add(
        var.set_source_address(
            await cg.templatable(config[CONF_SOURCE_ADDRESS], args, cg.uint16)
        )
    )
    cg.add(
        var.set_destination_address(
            await cg.templatable(config[CONF_DESTINATION_ADDRESS], args, cg.uint16)
        )
    )
    cg.add(
        var.set_retransmission(
            await cg.templatable(config[CONF_RETRANSMISSION], args, cg.bool_)
        )
    )
    cg.add(
        var.set_message_type(
            await cg.templatable(config[CONF_MESSAGE_TYPE], args, cg.uint8)
        )
    )
    cg.add(var.set_auto_message_id(CONF_MESSAGE_ID not in config))
    if CONF_MESSAGE_ID in config:
        cg.add(
            var.set_message_id(
                await cg.templatable(config[CONF_MESSAGE_ID], args, cg.uint8)
            )
        )
    if CONF_DATA in config:
        data_ = config[CONF_DATA]
        if cg.is_template(data_):
            template_ = await cg.templatable(
                data_, args, cg.std_vector.template(cg.uint8)
            )
            cg.add(var.set_data_template(template_))
        else:
            cg.add(var.set_data_static(data_))


# Mirage
(
    MirageData,
    MirageBinarySensor,
    MirageTrigger,
    MirageAction,
    MirageDumper,
) = declare_protocol("Mirage")

MIRAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All([cv.hex_uint8_t], cv.Length(min=14, max=14)),
    }
)


@register_binary_sensor("mirage", MirageBinarySensor, MIRAGE_SCHEMA)
def mirage_binary_sensor(var, config):
    cg.add(var.set_code(config[CONF_CODE]))


@register_trigger("mirage", MirageTrigger, MirageData)
def mirage_trigger(var, config):
    pass


@register_dumper("mirage", MirageDumper)
def mirage_dumper(var, config):
    pass


@register_action("mirage", MirageAction, MIRAGE_SCHEMA)
async def mirage_action(var, config, args):
    vec_ = cg.std_vector.template(cg.uint8)
    template_ = await cg.templatable(config[CONF_CODE], args, vec_, vec_)
    cg.add(var.set_code(template_))
