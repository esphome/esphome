import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor
from esphome.const import CONF_DATA, CONF_TRIGGER_ID, CONF_NBITS, CONF_ADDRESS, \
    CONF_COMMAND, CONF_CODE, CONF_PULSE_LENGTH, CONF_SYNC, CONF_ZERO, CONF_ONE, CONF_INVERTED, \
    CONF_PROTOCOL, CONF_GROUP, CONF_DEVICE, CONF_STATE, CONF_CHANNEL, CONF_FAMILY, CONF_REPEAT, \
    CONF_WAIT_TIME, CONF_TIMES, CONF_TYPE_ID, CONF_CARRIER_FREQUENCY
from esphome.core import coroutine
from esphome.py_compat import string_types, text_type
from esphome.util import Registry, SimpleRegistry

AUTO_LOAD = ['binary_sensor']

CONF_RECEIVER_ID = 'receiver_id'
CONF_TRANSMITTER_ID = 'transmitter_id'

ns = remote_base_ns = cg.esphome_ns.namespace('remote_base')
RemoteProtocol = ns.class_('RemoteProtocol')
RemoteReceiverListener = ns.class_('RemoteReceiverListener')
RemoteReceiverBinarySensorBase = ns.class_('RemoteReceiverBinarySensorBase',
                                           binary_sensor.BinarySensor, cg.Component)
RemoteReceiverTrigger = ns.class_('RemoteReceiverTrigger', automation.Trigger,
                                  RemoteReceiverListener)
RemoteTransmitterDumper = ns.class_('RemoteTransmitterDumper')
RemoteTransmitterActionBase = ns.class_('RemoteTransmitterActionBase', automation.Action)
RemoteReceiverBase = ns.class_('RemoteReceiverBase')
RemoteTransmitterBase = ns.class_('RemoteTransmitterBase')


def templatize(value):
    if isinstance(value, cv.Schema):
        value = value.schema
    ret = {}
    for key, val in value.items():
        ret[key] = cv.templatable(val)
    return cv.Schema(ret)


@coroutine
def register_listener(var, config):
    receiver = yield cg.get_variable(config[CONF_RECEIVER_ID])
    cg.add(receiver.register_listener(var))


def register_binary_sensor(name, type, schema):
    return BINARY_SENSOR_REGISTRY.register(name, type, schema)


def register_trigger(name, type, data_type):
    validator = automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(type),
        cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
    })
    registerer = TRIGGER_REGISTRY.register('on_{}'.format(name), validator)

    def decorator(func):
        @coroutine
        def new_func(config):
            var = cg.new_Pvariable(config[CONF_TRIGGER_ID])
            yield register_listener(var, config)
            yield coroutine(func)(var, config)
            yield automation.build_automation(var, [(data_type, 'x')], config)
            yield var

        return registerer(new_func)

    return decorator


def register_dumper(name, type):
    registerer = DUMPER_REGISTRY.register(name, type, {})

    def decorator(func):
        @coroutine
        def new_func(config, dumper_id):
            var = cg.new_Pvariable(dumper_id)
            yield coroutine(func)(var, config)
            yield var

        return registerer(new_func)

    return decorator


def validate_repeat(value):
    if isinstance(value, dict):
        return cv.Schema({
            cv.Required(CONF_TIMES): cv.templatable(cv.positive_int),
            cv.Optional(CONF_WAIT_TIME, default='10ms'):
                cv.templatable(cv.positive_time_period_microseconds),
        })(value)
    return validate_repeat({CONF_TIMES: value})


def register_action(name, type_, schema):
    validator = templatize(schema).extend({
        cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(RemoteTransmitterBase),
        cv.Optional(CONF_REPEAT): validate_repeat,
    })
    registerer = automation.register_action('remote_transmitter.transmit_{}'.format(name),
                                            type_, validator)

    def decorator(func):
        @coroutine
        def new_func(config, action_id, template_arg, args):
            transmitter = yield cg.get_variable(config[CONF_TRANSMITTER_ID])
            var = cg.new_Pvariable(action_id, template_arg)
            cg.add(var.set_parent(transmitter))
            if CONF_REPEAT in config:
                conf = config[CONF_REPEAT]
                template_ = yield cg.templatable(conf[CONF_TIMES], args, cg.uint32)
                cg.add(var.set_send_times(template_))
                template_ = yield cg.templatable(conf[CONF_WAIT_TIME], args, cg.uint32)
                cg.add(var.set_send_wait(template_))
            yield coroutine(func)(var, config, args)
            yield var

        return registerer(new_func)

    return decorator


def declare_protocol(name):
    data = ns.struct('{}Data'.format(name))
    binary_sensor_ = ns.class_('{}BinarySensor'.format(name), RemoteReceiverBinarySensorBase)
    trigger = ns.class_('{}Trigger'.format(name), RemoteReceiverTrigger)
    action = ns.class_('{}Action'.format(name), RemoteTransmitterActionBase)
    dumper = ns.class_('{}Dumper'.format(name), RemoteTransmitterDumper)
    return data, binary_sensor_, trigger, action, dumper


BINARY_SENSOR_REGISTRY = Registry(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
}))
validate_binary_sensor = cv.validate_registry_entry('remote receiver', BINARY_SENSOR_REGISTRY)
TRIGGER_REGISTRY = SimpleRegistry()
DUMPER_REGISTRY = Registry({
    cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
})


def validate_dumpers(value):
    if isinstance(value, string_types) and value.lower() == 'all':
        return validate_dumpers(list(DUMPER_REGISTRY.keys()))
    return cv.validate_registry('dumper', DUMPER_REGISTRY)(value)


def validate_triggers(base_schema):
    assert isinstance(base_schema, cv.Schema)

    def validator(config):
        added_keys = {}
        for key, (_, valid) in TRIGGER_REGISTRY.items():
            added_keys[cv.Optional(key)] = valid
        new_schema = base_schema.extend(added_keys)
        return new_schema(config)

    return validator


@coroutine
def build_binary_sensor(full_config):
    registry_entry, config = cg.extract_registry_entry_config(BINARY_SENSOR_REGISTRY, full_config)
    type_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    var = cg.new_Pvariable(type_id)
    yield cg.register_component(var, full_config)
    yield register_listener(var, full_config)
    yield builder(var, config)
    yield var


@coroutine
def build_triggers(full_config):
    for key in TRIGGER_REGISTRY:
        for config in full_config.get(key, []):
            func = TRIGGER_REGISTRY[key][0]
            yield func(config)


@coroutine
def build_dumpers(config):
    dumpers = []
    for conf in config:
        dumper = yield cg.build_registry_entry(DUMPER_REGISTRY, conf)
        receiver = yield cg.get_variable(conf[CONF_RECEIVER_ID])
        cg.add(receiver.register_dumper(dumper))
        dumpers.append(dumper)
    yield dumpers


# JVC
JVCData, JVCBinarySensor, JVCTrigger, JVCAction, JVCDumper = declare_protocol('JVC')
JVC_SCHEMA = cv.Schema({cv.Required(CONF_DATA): cv.hex_uint32_t})


@register_binary_sensor('jvc', JVCBinarySensor, JVC_SCHEMA)
def jvc_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        JVCData,
        ('data', config[CONF_DATA]),
    )))


@register_trigger('jvc', JVCTrigger, JVCData)
def jvc_trigger(var, config):
    pass


@register_dumper('jvc', JVCDumper)
def jvc_dumper(var, config):
    pass


@register_action('jvc', JVCAction, JVC_SCHEMA)
def jvc_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_DATA], args, cg.uint32)
    cg.add(var.set_data(template_))


# LG
LGData, LGBinarySensor, LGTrigger, LGAction, LGDumper = declare_protocol('LG')
LG_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): cv.hex_uint32_t,
    cv.Optional(CONF_NBITS, default=28): cv.one_of(28, 32, int=True),
})


@register_binary_sensor('lg', LGBinarySensor, LG_SCHEMA)
def lg_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        LGData,
        ('data', config[CONF_DATA]),
        ('nbits', config[CONF_NBITS]),
    )))


@register_trigger('lg', LGTrigger, LGData)
def lg_trigger(var, config):
    pass


@register_dumper('lg', LGDumper)
def lg_dumper(var, config):
    pass


@register_action('lg', LGAction, LG_SCHEMA)
def lg_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_DATA], args, cg.uint32)
    cg.add(var.set_data(template_))
    template_ = yield cg.templatable(config[CONF_NBITS], args, cg.uint8)
    cg.add(var.set_nbits(template_))


# NEC
NECData, NECBinarySensor, NECTrigger, NECAction, NECDumper = declare_protocol('NEC')
NEC_SCHEMA = cv.Schema({
    cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
    cv.Required(CONF_COMMAND): cv.hex_uint16_t,
})


@register_binary_sensor('nec', NECBinarySensor, NEC_SCHEMA)
def nec_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        NECData,
        ('address', config[CONF_ADDRESS]),
        ('command', config[CONF_COMMAND]),
    )))


@register_trigger('nec', NECTrigger, NECData)
def nec_trigger(var, config):
    pass


@register_dumper('nec', NECDumper)
def nec_dumper(var, config):
    pass


@register_action('nec', NECAction, NEC_SCHEMA)
def nec_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = yield cg.templatable(config[CONF_COMMAND], args, cg.uint16)
    cg.add(var.set_command(template_))


# Sony
SonyData, SonyBinarySensor, SonyTrigger, SonyAction, SonyDumper = declare_protocol('Sony')
SONY_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): cv.hex_uint32_t,
    cv.Optional(CONF_NBITS, default=12): cv.one_of(12, 15, 20, int=True),
})


@register_binary_sensor('sony', SonyBinarySensor, SONY_SCHEMA)
def sony_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        SonyData,
        ('data', config[CONF_DATA]),
        ('nbits', config[CONF_NBITS]),
    )))


@register_trigger('sony', SonyTrigger, SonyData)
def sony_trigger(var, config):
    pass


@register_dumper('sony', SonyDumper)
def sony_dumper(var, config):
    pass


@register_action('sony', SonyAction, SONY_SCHEMA)
def sony_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_DATA], args, cg.uint16)
    cg.add(var.set_data(template_))
    template_ = yield cg.templatable(config[CONF_NBITS], args, cg.uint32)
    cg.add(var.set_nbits(template_))


# Raw
def validate_raw_alternating(value):
    assert isinstance(value, list)
    last_negative = None
    for i, val in enumerate(value):
        this_negative = val < 0
        if i != 0:
            if this_negative == last_negative:
                raise cv.Invalid("Values must alternate between being positive and negative, "
                                 "please see index {} and {}".format(i, i + 1), [i])
        last_negative = this_negative
    return value


RawData, RawBinarySensor, RawTrigger, RawAction, RawDumper = declare_protocol('Raw')
CONF_CODE_STORAGE_ID = 'code_storage_id'
RAW_SCHEMA = cv.Schema({
    cv.Required(CONF_CODE): cv.All([cv.Any(cv.int_, cv.time_period_microseconds)],
                                   cv.Length(min=1), validate_raw_alternating),
    cv.GenerateID(CONF_CODE_STORAGE_ID): cv.declare_id(cg.int32),
})


@register_binary_sensor('raw', RawBinarySensor, RAW_SCHEMA)
def raw_binary_sensor(var, config):
    code_ = config[CONF_CODE]
    arr = cg.progmem_array(config[CONF_CODE_STORAGE_ID], code_)
    cg.add(var.set_data(arr))
    cg.add(var.set_len(len(code_)))


@register_trigger('raw', RawTrigger, cg.std_vector.template(cg.int32))
def raw_trigger(var, config):
    pass


@register_dumper('raw', RawDumper)
def raw_dumper(var, config):
    pass


@register_action('raw', RawAction, RAW_SCHEMA.extend({
    cv.Optional(CONF_CARRIER_FREQUENCY, default='0Hz'): cv.All(cv.frequency, cv.int_),
}))
def raw_action(var, config, args):
    code_ = config[CONF_CODE]
    if cg.is_template(code_):
        template_ = yield cg.templatable(code_, args, cg.std_vector.template(cg.int32))
        cg.add(var.set_code_template(template_))
    else:
        code_ = config[CONF_CODE]
        arr = cg.progmem_array(config[CONF_CODE_STORAGE_ID], code_)
        cg.add(var.set_code_static(arr, len(code_)))
    templ = yield cg.templatable(config[CONF_CARRIER_FREQUENCY], args, cg.uint32)
    cg.add(var.set_carrier_frequency(templ))


# RC5
RC5Data, RC5BinarySensor, RC5Trigger, RC5Action, RC5Dumper = declare_protocol('RC5')
RC5_SCHEMA = cv.Schema({
    cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0x1F)),
    cv.Required(CONF_COMMAND): cv.All(cv.hex_int, cv.Range(min=0, max=0x3F)),
})


@register_binary_sensor('rc5', RC5BinarySensor, RC5_SCHEMA)
def rc5_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        RC5Data,
        ('address', config[CONF_ADDRESS]),
        ('command', config[CONF_COMMAND]),
    )))


@register_trigger('rc5', RC5Trigger, RC5Data)
def rc5_trigger(var, config):
    pass


@register_dumper('rc5', RC5Dumper)
def rc5_dumper(var, config):
    pass


@register_action('rc5', RC5Action, RC5_SCHEMA)
def rc5_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
    cg.add(var.set_address(template_))
    template_ = yield cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_command(template_))


# RC Switch Raw
RC_SWITCH_TIMING_SCHEMA = cv.All([cv.uint8_t], cv.Length(min=2, max=2))

RC_SWITCH_PROTOCOL_SCHEMA = cv.Any(
    cv.int_range(min=1, max=8),
    cv.Schema({
        cv.Required(CONF_PULSE_LENGTH): cv.uint32_t,
        cv.Optional(CONF_SYNC, default=[1, 31]): RC_SWITCH_TIMING_SCHEMA,
        cv.Optional(CONF_ZERO, default=[1, 3]): RC_SWITCH_TIMING_SCHEMA,
        cv.Optional(CONF_ONE, default=[3, 1]): RC_SWITCH_TIMING_SCHEMA,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    })
)


def validate_rc_switch_code(value):
    if not isinstance(value, (str, text_type)):
        raise cv.Invalid("All RCSwitch codes must be in quotes ('')")
    for c in value:
        if c not in ('0', '1'):
            raise cv.Invalid(u"Invalid RCSwitch code character '{}'. Only '0' and '1' are allowed"
                             u"".format(c))
    if len(value) > 64:
        raise cv.Invalid("Maximum length for RCSwitch codes is 64, code '{}' has length {}"
                         "".format(value, len(value)))
    if not value:
        raise cv.Invalid("RCSwitch code must not be empty")
    return value


def validate_rc_switch_raw_code(value):
    if not isinstance(value, (str, text_type)):
        raise cv.Invalid("All RCSwitch raw codes must be in quotes ('')")
    for c in value:
        if c not in ('0', '1', 'x'):
            raise cv.Invalid(
                "Invalid RCSwitch raw code character '{}'.Only '0', '1' and 'x' are allowed"
                .format(c))
    if len(value) > 64:
        raise cv.Invalid("Maximum length for RCSwitch raw codes is 64, code '{}' has length {}"
                         "".format(value, len(value)))
    if not value:
        raise cv.Invalid("RCSwitch raw code must not be empty")
    return value


def build_rc_switch_protocol(config):
    if isinstance(config, int):
        return rc_switch_protocols[config]
    pl = config[CONF_PULSE_LENGTH]
    return RCSwitchBase(config[CONF_SYNC][0] * pl, config[CONF_SYNC][1] * pl,
                        config[CONF_ZERO][0] * pl, config[CONF_ZERO][1] * pl,
                        config[CONF_ONE][0] * pl, config[CONF_ONE][1] * pl,
                        config[CONF_INVERTED])


RC_SWITCH_RAW_SCHEMA = cv.Schema({
    cv.Required(CONF_CODE): validate_rc_switch_raw_code,
    cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_A_SCHEMA = cv.Schema({
    cv.Required(CONF_GROUP): cv.All(validate_rc_switch_code, cv.Length(min=5, max=5)),
    cv.Required(CONF_DEVICE): cv.All(validate_rc_switch_code, cv.Length(min=5, max=5)),
    cv.Required(CONF_STATE): cv.boolean,
    cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_B_SCHEMA = cv.Schema({
    cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=4),
    cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
    cv.Required(CONF_STATE): cv.boolean,
    cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_C_SCHEMA = cv.Schema({
    cv.Required(CONF_FAMILY): cv.one_of('a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
                                        'l', 'm', 'n', 'o', 'p', lower=True),
    cv.Required(CONF_GROUP): cv.int_range(min=1, max=4),
    cv.Required(CONF_DEVICE): cv.int_range(min=1, max=4),
    cv.Required(CONF_STATE): cv.boolean,
    cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_D_SCHEMA = cv.Schema({
    cv.Required(CONF_GROUP): cv.one_of('a', 'b', 'c', 'd', lower=True),
    cv.Required(CONF_DEVICE): cv.int_range(min=1, max=3),
    cv.Required(CONF_STATE): cv.boolean,
    cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TRANSMITTER = cv.Schema({
    cv.Optional(CONF_REPEAT, default={CONF_TIMES: 5}): cv.Schema({
        cv.Required(CONF_TIMES): cv.templatable(cv.positive_int),
        cv.Optional(CONF_WAIT_TIME, default='10ms'):
            cv.templatable(cv.positive_time_period_microseconds),
    }),
})

rc_switch_protocols = ns.rc_switch_protocols
RCSwitchBase = ns.class_('RCSwitchBase')
RCSwitchDumper = ns.class_('RCSwitchDumper', RemoteTransmitterDumper)
RCSwitchRawAction = ns.class_('RCSwitchRawAction', RemoteTransmitterActionBase)
RCSwitchTypeAAction = ns.class_('RCSwitchTypeAAction', RemoteTransmitterActionBase)
RCSwitchTypeBAction = ns.class_('RCSwitchTypeBAction', RemoteTransmitterActionBase)
RCSwitchTypeCAction = ns.class_('RCSwitchTypeCAction', RemoteTransmitterActionBase)
RCSwitchTypeDAction = ns.class_('RCSwitchTypeDAction', RemoteTransmitterActionBase)
RCSwitchRawReceiver = ns.class_('RCSwitchRawReceiver', RemoteReceiverBinarySensorBase)


@register_binary_sensor('rc_switch_raw', RCSwitchRawReceiver, RC_SWITCH_RAW_SCHEMA)
def rc_switch_raw_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_code(config[CONF_CODE]))


@register_action('rc_switch_raw', RCSwitchRawAction,
                 RC_SWITCH_RAW_SCHEMA.extend(RC_SWITCH_TRANSMITTER))
def rc_switch_raw_action(var, config, args):
    proto = yield cg.templatable(config[CONF_PROTOCOL], args, RCSwitchBase,
                                 to_exp=build_rc_switch_protocol)
    cg.add(var.set_protocol(proto))
    cg.add(var.set_code((yield cg.templatable(config[CONF_CODE], args, cg.std_string))))


@register_binary_sensor('rc_switch_type_a', RCSwitchRawReceiver, RC_SWITCH_TYPE_A_SCHEMA)
def rc_switch_type_a_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_type_a(config[CONF_GROUP], config[CONF_DEVICE], config[CONF_STATE]))


@register_action('rc_switch_type_a', RCSwitchTypeAAction,
                 RC_SWITCH_TYPE_A_SCHEMA.extend(RC_SWITCH_TRANSMITTER))
def rc_switch_type_a_action(var, config, args):
    proto = yield cg.templatable(config[CONF_PROTOCOL], args, RCSwitchBase,
                                 to_exp=build_rc_switch_protocol)
    cg.add(var.set_protocol(proto))
    cg.add(var.set_group((yield cg.templatable(config[CONF_GROUP], args, cg.std_string))))
    cg.add(var.set_device((yield cg.templatable(config[CONF_DEVICE], args, cg.std_string))))
    cg.add(var.set_state((yield cg.templatable(config[CONF_STATE], args, bool))))


@register_binary_sensor('rc_switch_type_b', RCSwitchRawReceiver, RC_SWITCH_TYPE_B_SCHEMA)
def rc_switch_type_b_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_type_b(config[CONF_ADDRESS], config[CONF_CHANNEL], config[CONF_STATE]))


@register_action('rc_switch_type_b', RCSwitchTypeBAction,
                 RC_SWITCH_TYPE_B_SCHEMA.extend(RC_SWITCH_TRANSMITTER))
def rc_switch_type_b_action(var, config, args):
    proto = yield cg.templatable(config[CONF_PROTOCOL], args, RCSwitchBase,
                                 to_exp=build_rc_switch_protocol)
    cg.add(var.set_protocol(proto))
    cg.add(var.set_address((yield cg.templatable(config[CONF_ADDRESS], args, cg.uint8))))
    cg.add(var.set_channel((yield cg.templatable(config[CONF_CHANNEL], args, cg.uint8))))
    cg.add(var.set_state((yield cg.templatable(config[CONF_STATE], args, bool))))


@register_binary_sensor('rc_switch_type_c', RCSwitchRawReceiver, RC_SWITCH_TYPE_C_SCHEMA)
def rc_switch_type_c_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_type_c(config[CONF_FAMILY], config[CONF_GROUP], config[CONF_DEVICE],
                          config[CONF_STATE]))


@register_action('rc_switch_type_c', RCSwitchTypeCAction,
                 RC_SWITCH_TYPE_C_SCHEMA.extend(RC_SWITCH_TRANSMITTER))
def rc_switch_type_c_action(var, config, args):
    proto = yield cg.templatable(config[CONF_PROTOCOL], args, RCSwitchBase,
                                 to_exp=build_rc_switch_protocol)
    cg.add(var.set_protocol(proto))
    cg.add(var.set_family((yield cg.templatable(config[CONF_FAMILY], args, cg.std_string))))
    cg.add(var.set_group((yield cg.templatable(config[CONF_GROUP], args, cg.uint8))))
    cg.add(var.set_device((yield cg.templatable(config[CONF_DEVICE], args, cg.uint8))))
    cg.add(var.set_state((yield cg.templatable(config[CONF_STATE], args, bool))))


@register_binary_sensor('rc_switch_type_d', RCSwitchRawReceiver,
                        RC_SWITCH_TYPE_D_SCHEMA.extend(RC_SWITCH_TRANSMITTER))
def rc_switch_type_d_binary_sensor(var, config):
    cg.add(var.set_protocol(build_rc_switch_protocol(config[CONF_PROTOCOL])))
    cg.add(var.set_type_d(config[CONF_GROUP], config[CONF_DEVICE], config[CONF_STATE]))


@register_action('rc_switch_type_d', RCSwitchTypeDAction,
                 RC_SWITCH_TYPE_D_SCHEMA.extend(RC_SWITCH_TRANSMITTER))
def rc_switch_type_d_action(var, config, args):
    proto = yield cg.templatable(config[CONF_PROTOCOL], args, RCSwitchBase,
                                 to_exp=build_rc_switch_protocol)
    cg.add(var.set_protocol(proto))
    cg.add(var.set_group((yield cg.templatable(config[CONF_GROUP], args, cg.std_string))))
    cg.add(var.set_device((yield cg.templatable(config[CONF_DEVICE], args, cg.uint8))))
    cg.add(var.set_state((yield cg.templatable(config[CONF_STATE], args, bool))))


@register_dumper('rc_switch', RCSwitchDumper)
def rc_switch_dumper(var, config):
    pass


# Samsung
(SamsungData, SamsungBinarySensor, SamsungTrigger, SamsungAction,
 SamsungDumper) = declare_protocol('Samsung')
SAMSUNG_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): cv.hex_uint32_t,
})


@register_binary_sensor('samsung', SamsungBinarySensor, SAMSUNG_SCHEMA)
def samsung_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        SamsungData,
        ('data', config[CONF_DATA]),
    )))


@register_trigger('samsung', SamsungTrigger, SamsungData)
def samsung_trigger(var, config):
    pass


@register_dumper('samsung', SamsungDumper)
def samsung_dumper(var, config):
    pass


@register_action('samsung', SamsungAction, SAMSUNG_SCHEMA)
def samsung_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_DATA], args, cg.uint32)
    cg.add(var.set_data(template_))


# Panasonic
(PanasonicData, PanasonicBinarySensor, PanasonicTrigger, PanasonicAction,
 PanasonicDumper) = declare_protocol('Panasonic')
PANASONIC_SCHEMA = cv.Schema({
    cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
    cv.Required(CONF_COMMAND): cv.hex_uint32_t,
})


@register_binary_sensor('panasonic', PanasonicBinarySensor, PANASONIC_SCHEMA)
def panasonic_binary_sensor(var, config):
    cg.add(var.set_data(cg.StructInitializer(
        PanasonicData,
        ('address', config[CONF_ADDRESS]),
        ('command', config[CONF_COMMAND]),
    )))


@register_trigger('panasonic', PanasonicTrigger, PanasonicData)
def panasonic_trigger(var, config):
    pass


@register_dumper('panasonic', PanasonicDumper)
def panasonic_dumper(var, config):
    pass


@register_action('panasonic', PanasonicAction, PANASONIC_SCHEMA)
def panasonic_action(var, config, args):
    template_ = yield cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(template_))
    template_ = yield cg.templatable(config[CONF_COMMAND], args, cg.uint32)
    cg.add(var.set_command(template_))
