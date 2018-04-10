from __future__ import print_function

import logging
import re
from collections import OrderedDict, deque

from esphomeyaml.const import CONF_AVAILABILITY, CONF_COMMAND_TOPIC, CONF_DISCOVERY, \
    CONF_INVERTED, \
    CONF_MODE, CONF_NUMBER, CONF_PAYLOAD_AVAILABLE, CONF_PAYLOAD_NOT_AVAILABLE, CONF_RETAIN, \
    CONF_STATE_TOPIC, CONF_TOPIC
from esphomeyaml.core import ESPHomeYAMLError, HexInt

_LOGGER = logging.getLogger(__name__)

SIMPLIFY = False


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
        pass

    def __str__(self):
        raise NotImplemented


class RawExpression(Expression):
    def __init__(self, text):
        super(RawExpression, self).__init__()
        self.text = text

    def __str__(self):
        return self.text


class AssignmentExpression(Expression):
    def __init__(self, lhs, rhs, obj):
        super(AssignmentExpression, self).__init__()
        self.obj = obj
        self.lhs = safe_exp(lhs)
        self.rhs = safe_exp(rhs)

    def __str__(self):
        return u"{} = {}".format(self.lhs, self.rhs)


class ExpressionList(Expression):
    def __init__(self, *args):
        super(ExpressionList, self).__init__()
        # Remove every None on end
        args = list(args)
        while args and args[-1] is None:
            args.pop()
        self.args = [safe_exp(x) for x in args]

    def __str__(self):
        text = u", ".join(unicode(x) for x in self.args)
        return indent_all_but_first_and_last(text)


class CallExpression(Expression):
    def __init__(self, base, *args):
        super(CallExpression, self).__init__()
        self.base = base
        self.args = ExpressionList(*args)

    def __str__(self):
        return u'{}({})'.format(self.base, self.args)


class StructInitializer(Expression):
    def __init__(self, base, *args):
        super(StructInitializer, self).__init__()
        self.base = base
        if not isinstance(args, OrderedDict):
            args = OrderedDict(args)
        self.args = OrderedDict()
        for key, value in args.iteritems():
            if value is not None:
                self.args[key] = safe_exp(value)

    def __str__(self):
        s = u'{}{{\n'.format(self.base)
        for key, value in self.args.iteritems():
            s += u'  .{} = {},\n'.format(key, value)
        s += u'}'
        return s


class ArrayInitializer(Expression):
    def __init__(self, *args):
        super(ArrayInitializer, self).__init__()
        self.args = [safe_exp(x) for x in args if x is not None]

    def __str__(self):
        if not self.args:
            return u'{}'
        s = u'{\n'
        for arg in self.args:
            s += u'  {},\n'.format(arg)
        s += u'}'
        return s


class Literal(Expression):
    def __init__(self):
        super(Literal, self).__init__()


class StringLiteral(Literal):
    def __init__(self, s):
        super(StringLiteral, self).__init__()
        self.s = s

    def __str__(self):
        return u'"{}"'.format(self.s)


class IntLiteral(Literal):
    def __init__(self, i):
        super(IntLiteral, self).__init__()
        self.i = i

    def __str__(self):
        return unicode(self.i)


class BoolLiteral(Literal):
    def __init__(self, b):
        super(BoolLiteral, self).__init__()
        self.b = b

    def __str__(self):
        return u"true" if self.b else u"false"


class HexIntLiteral(Literal):
    def __init__(self, i):
        super(HexIntLiteral, self).__init__()
        self.i = HexInt(i)

    def __str__(self):
        return str(self.i)


class FloatLiteral(Literal):
    def __init__(self, f):
        super(FloatLiteral, self).__init__()
        self.f = f

    def __str__(self):
        return u"{:f}f".format(self.f)


def safe_exp(obj):
    if isinstance(obj, Expression):
        return obj
    elif isinstance(obj, bool):
        return BoolLiteral(obj)
    elif isinstance(obj, str) or isinstance(obj, unicode):
        return StringLiteral(obj)
    elif isinstance(obj, (int, long)):
        return IntLiteral(obj)
    elif isinstance(obj, float):
        return FloatLiteral(obj)
    raise ValueError(u"Object is not an expression", obj)


class Statement(object):
    def __init__(self):
        pass

    def __str__(self):
        raise NotImplemented


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


def variable(type, id, rhs):
    lhs = RawExpression(u'{} {}'.format(type if not SIMPLIFY else u'auto', id))
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'.')
    add(AssignmentExpression(lhs, rhs, obj))
    _VARIABLES[id] = obj, type
    return obj


def Pvariable(type, id, rhs):
    lhs = RawExpression(u'{} *{}'.format(type if not SIMPLIFY else u'auto', id))
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'->')
    add(AssignmentExpression(lhs, rhs, obj))
    _VARIABLES[id] = obj, type
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
            result = _VARIABLES[id][0]
        elif type is not None:
            result = next((x[0] for x in _VARIABLES.itervalues() if x[1] == type), None)

        if result is None:
            raise ESPHomeYAMLError(u"Couldn't find ID '{}' with type {}".format(id, type))
    result.usages += 1
    return result


def add_task(func, config):
    _QUEUE.append((func, config))


def add(expression):
    _EXPRESSIONS.append(expression)
    return expression


class MockObj(Expression):
    def __init__(self, base, op=u'.', parent=None):
        self.base = base
        self.op = op
        self.usages = 0
        self.parent = parent
        super(MockObj, self).__init__()

    def __getattr__(self, attr):
        next_op = u'.'
        if attr.startswith(u'P'):
            attr = attr[1:]
            next_op = u'->'
        op = self.op
        return MockObj(u'{}{}{}'.format(self.base, op, attr), next_op, self)

    def __call__(self, *args, **kwargs):
        self.usages += 1
        it = self.parent
        while it is not None:
            it.usages += 1
            it = it.parent
        return CallExpression(self.base, *args)

    def __str__(self):
        return self.base


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
        exp = StructInitializer(
            u'mqtt::Availability',
            (u'topic', availability[CONF_TOPIC]),
            (u'payload_available', availability[CONF_PAYLOAD_AVAILABLE]),
            (u'payload_not_available', availability[CONF_PAYLOAD_NOT_AVAILABLE]),
        )
        add(obj.set_availability(exp))


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
