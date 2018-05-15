from __future__ import print_function

import logging
import re
from collections import OrderedDict, deque

from esphomeyaml import core
from esphomeyaml.const import CONF_AVAILABILITY, CONF_COMMAND_TOPIC, CONF_DISCOVERY, \
    CONF_INVERTED, \
    CONF_MODE, CONF_NUMBER, CONF_PAYLOAD_AVAILABLE, CONF_PAYLOAD_NOT_AVAILABLE, CONF_PCF8574, \
    CONF_RETAIN, CONF_STATE_TOPIC, CONF_TOPIC
from esphomeyaml.core import ESPHomeYAMLError, HexInt, TimePeriodMicroseconds, \
    TimePeriodMilliseconds, TimePeriodSeconds

_LOGGER = logging.getLogger(__name__)


def ensure_unique_string(preferred_string, current_strings):
    test_string = preferred_string
    current_strings_set = set(current_strings)

    tries = 1

    while test_string in current_strings_set:
        tries += 1
        test_string = u"{}_{}".format(preferred_string, tries)

    return test_string


def indent_all_but_first_and_last(text, padding=u'  '):
    lines = text.splitlines(True)
    if len(lines) <= 2:
        return text
    return lines[0] + u''.join(padding + line for line in lines[1:-1]) + lines[-1]


def indent_list(text, padding=u'  '):
    return [padding + line for line in text.splitlines()]


def indent(text, padding=u'  '):
    return u'\n'.join(indent_list(text, padding))


class Expression(object):
    def __init__(self):
        self.requires = []
        self.required = False

    def __str__(self):
        raise NotImplementedError

    def require(self):
        self.required = True
        for require in self.requires:
            if require.required:
                continue
            require.require()


class RawExpression(Expression):
    def __init__(self, text):
        super(RawExpression, self).__init__()
        self.text = text

    def __str__(self):
        return self.text


# pylint: disable=redefined-builtin
class AssignmentExpression(Expression):
    def __init__(self, type, modifier, name, rhs, obj):
        super(AssignmentExpression, self).__init__()
        self.type = type
        self.modifier = modifier
        self.name = name
        self.rhs = safe_exp(rhs)
        self.requires.append(self.rhs)
        self.obj = obj

    def __str__(self):
        type_ = self.type
        if core.SIMPLIFY:
            type_ = u'auto'
        return u"{} {}{} = {}".format(type_, self.modifier, self.name, self.rhs)


class ExpressionList(Expression):
    def __init__(self, *args):
        super(ExpressionList, self).__init__()
        # Remove every None on end
        args = list(args)
        while args and args[-1] is None:
            args.pop()
        self.args = []
        for arg in args:
            exp = safe_exp(arg)
            self.requires.append(exp)
            self.args.append(exp)

    def __str__(self):
        text = u", ".join(unicode(x) for x in self.args)
        return indent_all_but_first_and_last(text)


class TemplateArguments(Expression):
    def __init__(self, *args):
        super(TemplateArguments, self).__init__()
        self.args = ExpressionList(*args)
        self.requires.append(self.args)

    def __str__(self):
        return u'<{}>'.format(self.args)


class CallExpression(Expression):
    def __init__(self, base, *args):
        super(CallExpression, self).__init__()
        self.base = base
        if args and isinstance(args[0], TemplateArguments):
            self.template_args = args[0]
            self.requires.append(self.template_args)
            args = args[1:]
        else:
            self.template_args = None
        self.args = ExpressionList(*args)
        self.requires.append(self.args)

    def __str__(self):
        if self.template_args is not None:
            return u'{}{}({})'.format(self.base, self.template_args, self.args)
        return u'{}({})'.format(self.base, self.args)


class StructInitializer(Expression):
    def __init__(self, base, *args):
        super(StructInitializer, self).__init__()
        self.base = base
        if not isinstance(args, OrderedDict):
            args = OrderedDict(args)
        self.args = OrderedDict()
        for key, value in args.iteritems():
            if value is None:
                continue
            exp = safe_exp(value)
            self.args[key] = exp
            self.requires.append(exp)

    def __str__(self):
        cpp = u'{}{{\n'.format(self.base)
        for key, value in self.args.iteritems():
            cpp += u'  .{} = {},\n'.format(key, value)
        cpp += u'}'
        return cpp


class ArrayInitializer(Expression):
    def __init__(self, *args, **kwargs):
        super(ArrayInitializer, self).__init__()
        self.multiline = kwargs.get('multiline', True)
        self.args = []
        for arg in args:
            if arg is None:
                continue
            exp = safe_exp(arg)
            self.args.append(exp)
            self.requires.append(exp)

    def __str__(self):
        if not self.args:
            return u'{}'
        if self.multiline:
            cpp = u'{\n'
            for arg in self.args:
                cpp += u'  {},\n'.format(arg)
            cpp += u'}'
        else:
            cpp = u'{' + u', '.join(str(arg) for arg in self.args) + u'}'
        return cpp


class Literal(Expression):
    def __str__(self):
        raise NotImplementedError


# From https://stackoverflow.com/a/14945195/8924614
def cpp_string_escape(s, encoding='utf-8'):
    if isinstance(s, unicode):
        s = s.encode(encoding)
    result = ''
    for c in s:
        if not (32 <= ord(c) < 127) or c in ('\\', '"'):
            result += '\\%03o' % ord(c)
        else:
            result += c
    return '"' + result + '"'


class StringLiteral(Literal):
    def __init__(self, string):
        super(StringLiteral, self).__init__()
        self.string = string

    def __str__(self):
        return u'{}'.format(cpp_string_escape(self.string))


class IntLiteral(Literal):
    def __init__(self, i):
        super(IntLiteral, self).__init__()
        self.i = i

    def __str__(self):
        return unicode(self.i)


class BoolLiteral(Literal):
    def __init__(self, binary):
        super(BoolLiteral, self).__init__()
        self.binary = binary

    def __str__(self):
        return u"true" if self.binary else u"false"


class HexIntLiteral(Literal):
    def __init__(self, i):
        super(HexIntLiteral, self).__init__()
        self.i = HexInt(i)

    def __str__(self):
        return str(self.i)


class FloatLiteral(Literal):
    def __init__(self, float_):
        super(FloatLiteral, self).__init__()
        self.float_ = float_

    def __str__(self):
        return u"{:f}f".format(self.float_)


def safe_exp(obj):
    if isinstance(obj, Expression):
        return obj
    elif isinstance(obj, bool):
        return BoolLiteral(obj)
    elif isinstance(obj, (str, unicode)):
        return StringLiteral(obj)
    elif isinstance(obj, HexInt):
        return HexIntLiteral(obj)
    elif isinstance(obj, (int, long)):
        return IntLiteral(obj)
    elif isinstance(obj, float):
        return FloatLiteral(obj)
    elif isinstance(obj, TimePeriodMicroseconds):
        return IntLiteral(int(obj.total_microseconds))
    elif isinstance(obj, TimePeriodMilliseconds):
        return IntLiteral(int(obj.total_milliseconds))
    elif isinstance(obj, TimePeriodSeconds):
        return IntLiteral(int(obj.total_seconds))
    raise ValueError(u"Object is not an expression", obj)


class Statement(object):
    def __init__(self):
        pass

    def __str__(self):
        raise NotImplementedError


class RawStatement(Statement):
    def __init__(self, text):
        super(RawStatement, self).__init__()
        self.text = text

    def __str__(self):
        return self.text


class ExpressionStatement(Statement):
    def __init__(self, expression):
        super(ExpressionStatement, self).__init__()
        self.expression = safe_exp(expression)

    def __str__(self):
        return u"{};".format(self.expression)


def statement(expression):
    if isinstance(expression, Statement):
        return expression
    return ExpressionStatement(expression)


# pylint: disable=redefined-builtin, invalid-name
def variable(type, id, rhs):
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'.')
    assignment = AssignmentExpression(type, '', id, rhs, obj)
    add(assignment)
    _VARIABLES[id] = obj, type
    obj.requires.append(assignment)
    return obj


def Pvariable(type, id, rhs):
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'->')
    assignment = AssignmentExpression(type, '*', id, rhs, obj)
    add(assignment)
    _VARIABLES[id] = obj, type
    obj.requires.append(assignment)
    return obj


_QUEUE = deque()
_VARIABLES = {}
_EXPRESSIONS = []


def get_variable(id, type=None):
    result = None
    while _QUEUE:
        if id is not None:
            if id in _VARIABLES:
                result = _VARIABLES[id][0]
                break
        elif type is not None:
            result = next((x[0] for x in _VARIABLES.itervalues() if x[1] == type), None)
            if result is not None:
                break
        func, config = _QUEUE.popleft()
        func(config)
    if id is None and type is None:
        return None
    if result is None:
        if id is not None:
            if id in _VARIABLES:
                result = _VARIABLES[id][0]
        elif type is not None:
            result = next((x[0] for x in _VARIABLES.itervalues() if x[1] == type), None)

        if result is None:
            raise ESPHomeYAMLError(u"Couldn't find ID '{}' with type {}".format(id, type))
    return result


def add_task(func, config):
    _QUEUE.append((func, config))


def add(expression, require=True):
    if require and isinstance(expression, Expression):
        expression.require()
    _EXPRESSIONS.append(expression)
    return expression


class MockObj(Expression):
    def __init__(self, base, op=u'.'):
        self.base = base
        self.op = op
        super(MockObj, self).__init__()

    def __getattr__(self, attr):
        next_op = u'.'
        if attr.startswith(u'P'):
            attr = attr[1:]
            next_op = u'->'
        op = self.op
        obj = MockObj(u'{}{}{}'.format(self.base, op, attr), next_op)
        obj.requires.append(self)
        return obj

    def __call__(self, *args, **kwargs):
        call = CallExpression(self.base, *args)
        obj = MockObj(call, self.op)
        obj.requires.append(self)
        obj.requires.append(call)
        return obj

    def __str__(self):
        return unicode(self.base)

    def require(self):
        self.required = True
        for require in self.requires:
            if require.required:
                continue
            require.require()


App = MockObj(u'App')

GPIOPin = MockObj(u'GPIOPin')
GPIOOutputPin = MockObj(u'GPIOOutputPin')
GPIOInputPin = MockObj(u'GPIOInputPin')


def get_gpio_pin_number(conf):
    if isinstance(conf, int):
        return conf
    return conf[CONF_NUMBER]


def exp_gpio_pin_(obj, conf, default_mode):
    if isinstance(conf, int):
        return conf

    if CONF_PCF8574 in conf:
        hub = get_variable(conf[CONF_PCF8574], 'io::PCF8574Component')
        if default_mode == u'INPUT':
            return hub.make_input_pin(conf[CONF_NUMBER],
                                      RawExpression('PCF8574_' + conf[CONF_MODE]),
                                      conf[CONF_INVERTED])
        elif default_mode == u'OUTPUT':
            return hub.make_output_pin(conf[CONF_NUMBER], conf[CONF_INVERTED])
        else:
            raise ESPHomeYAMLError(u"Unknown default mode {}".format(default_mode))

    if conf.get(CONF_INVERTED) is None:
        return obj(conf[CONF_NUMBER], conf.get(CONF_MODE))
    return obj(conf[CONF_NUMBER], RawExpression(conf.get(CONF_MODE, default_mode)),
               conf[CONF_INVERTED])


def exp_gpio_pin(conf):
    return GPIOPin(conf[CONF_NUMBER], conf[CONF_MODE], conf.get(CONF_INVERTED))


def exp_gpio_output_pin(conf):
    return exp_gpio_pin_(GPIOOutputPin, conf, u'OUTPUT')


def exp_gpio_input_pin(conf):
    return exp_gpio_pin_(GPIOInputPin, conf, u'INPUT')


def setup_mqtt_component(obj, config):
    if CONF_RETAIN in config:
        add(obj.set_retain(config[CONF_RETAIN]))
    if not config.get(CONF_DISCOVERY, True):
        add(obj.disable_discovery())
    if CONF_STATE_TOPIC in config:
        add(obj.set_custom_state_topic(config[CONF_STATE_TOPIC]))
    if CONF_COMMAND_TOPIC in config:
        add(obj.set_custom_command_topic(config[CONF_COMMAND_TOPIC]))
    if CONF_AVAILABILITY in config:
        availability = config[CONF_AVAILABILITY]
        add(obj.set_availability(availability[CONF_TOPIC], availability[CONF_PAYLOAD_AVAILABLE],
                                 availability[CONF_PAYLOAD_NOT_AVAILABLE]))


def exp_empty_optional(type):
    return RawExpression(u'Optional<{}>()'.format(type))


def exp_optional(type, value):
    if value is None:
        return exp_empty_optional(type)
    return value


# shlex's quote for Python 2.7
_find_unsafe = re.compile(r'[^\w@%+=:,./-]').search


def quote(s):
    """Return a shell-escaped version of the string *s*."""
    if not s:
        return u"''"
    if _find_unsafe(s) is None:
        return s

    # use single quotes, and put single quotes into double quotes
    # the string $'b is then quoted as '$'"'"'b'
    return u"'" + s.replace(u"'", u"'\"'\"'") + u"'"


def color(the_color, message='', reset=None):
    """Color helper."""
    from colorlog.escape_codes import escape_codes, parse_colors
    if not message:
        return parse_colors(the_color)
    return parse_colors(the_color) + message + escape_codes[reset or 'reset']
