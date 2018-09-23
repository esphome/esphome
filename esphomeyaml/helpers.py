from __future__ import print_function

import inspect
import logging
import os
import re
from collections import OrderedDict, deque

from esphomeyaml import core
from esphomeyaml.const import CONF_AVAILABILITY, CONF_COMMAND_TOPIC, CONF_DISCOVERY, \
    CONF_INVERTED, \
    CONF_MODE, CONF_NUMBER, CONF_PAYLOAD_AVAILABLE, CONF_PAYLOAD_NOT_AVAILABLE, CONF_PCF8574, \
    CONF_RETAIN, CONF_STATE_TOPIC, CONF_TOPIC
from esphomeyaml.core import ESPHomeYAMLError, HexInt, Lambda, TimePeriodMicroseconds, \
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

    def has_side_effects(self):
        return self.required


class RawExpression(Expression):
    def __init__(self, text):
        super(RawExpression, self).__init__()
        self.text = text

    def __str__(self):
        return str(self.text)


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
        return u"{} {}{} = {}".format(type_, self.modifier, self.name, self.rhs)

    def has_side_effects(self):
        return self.rhs.has_side_effects()


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
        text = u", ".join(str(x) for x in self.args)
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
        if isinstance(base, Expression):
            self.requires.append(base)
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


# pylint: disable=invalid-name
class ParameterExpression(Expression):
    def __init__(self, type, id):
        super(ParameterExpression, self).__init__()
        self.type = type
        self.id = id

    def __str__(self):
        return u"{} {}".format(self.type, self.id)


class ParameterListExpression(Expression):
    def __init__(self, *parameters):
        super(ParameterListExpression, self).__init__()
        self.parameters = []
        for parameter in parameters:
            if not isinstance(parameter, ParameterExpression):
                parameter = ParameterExpression(*parameter)
            self.parameters.append(parameter)
            self.requires.append(parameter)

    def __str__(self):
        return u", ".join(unicode(x) for x in self.parameters)


class LambdaExpression(Expression):
    def __init__(self, parts, parameters, capture='=', return_type=None):
        super(LambdaExpression, self).__init__()
        self.parts = parts
        if not isinstance(parameters, ParameterListExpression):
            parameters = ParameterListExpression(*parameters)
        self.parameters = parameters
        self.requires.append(self.parameters)
        self.capture = capture
        self.return_type = return_type
        if return_type is not None:
            self.requires.append(return_type)
        for i in range(1, len(parts), 3):
            self.requires.append(parts[i])

    def __str__(self):
        cpp = u'[{}]({})'.format(self.capture, self.parameters)
        if self.return_type is not None:
            cpp += u' -> {}'.format(self.return_type)
        cpp += u' {\n'
        for part in self.parts:
            cpp += unicode(part)
        cpp += u'\n}'
        return indent_all_but_first_and_last(cpp)


class Literal(Expression):
    def __str__(self):
        raise NotImplementedError


# From https://stackoverflow.com/a/14945195/8924614
def cpp_string_escape(string, encoding='utf-8'):
    if isinstance(string, unicode):
        string = string.encode(encoding)
    result = ''
    for character in string:
        if not (32 <= ord(character) < 127) or character in ('\\', '"'):
            result += '\\%03o' % ord(character)
        else:
            result += character
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
        if self.i > 4294967295:
            return u'{}ULL'.format(self.i)
        if self.i > 2147483647:
            return u'{}UL'.format(self.i)
        if self.i < -2147483648:
            return u'{}LL'.format(self.i)
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
    def __init__(self, value):
        super(FloatLiteral, self).__init__()
        self.float_ = value

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


def register_variable(id, obj):
    _LOGGER.debug("Registered variable %s of type %s", id.id, id.type)
    _VARIABLES[id] = obj


# pylint: disable=redefined-builtin, invalid-name
def variable(id, rhs, type=None):
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'.')
    id.type = type or id.type
    assignment = AssignmentExpression(id.type, '', id, rhs, obj)
    add(assignment)
    register_variable(id, obj)
    obj.requires.append(assignment)
    return obj


def Pvariable(id, rhs, has_side_effects=True, type=None):
    rhs = safe_exp(rhs)
    if not has_side_effects and hasattr(rhs, '_has_side_effects'):
        # pylint: disable=attribute-defined-outside-init, protected-access
        rhs._has_side_effects = False
    obj = MockObj(id, u'->', has_side_effects=has_side_effects)
    id.type = type or id.type
    assignment = AssignmentExpression(id.type, '*', id, rhs, obj)
    add(assignment)
    register_variable(id, obj)
    obj.requires.append(assignment)
    return obj


_TASKS = deque()
_VARIABLES = {}
_EXPRESSIONS = []


def get_variable(id):
    while True:
        if id in _VARIABLES:
            yield _VARIABLES[id]
            return
        _LOGGER.debug("Waiting for variable %s", id)
        yield None


def process_lambda(value, parameters, capture='=', return_type=None):
    if value is None:
        yield
        return
    parts = value.parts[:]
    for i, id in enumerate(value.requires_ids):
        var = None
        for var in get_variable(id):
            yield
        if parts[i * 3 + 2] == '.':
            parts[i * 3 + 1] = var._
        else:
            parts[i * 3 + 1] = var
        parts[i * 3 + 2] = ''
    yield LambdaExpression(parts, parameters, capture, return_type)
    return


def templatable(value, input_type, output_type):
    if isinstance(value, Lambda):
        lambda_ = None
        for lambda_ in process_lambda(value, [(input_type, 'x')], return_type=output_type):
            yield None
        yield lambda_
    else:
        yield value


def add_job(func, *args, **kwargs):
    domain = kwargs.get('domain')
    if inspect.isgeneratorfunction(func):
        def func_():
            yield
            for _ in func(*args):
                yield
    else:
        def func_():
            yield
            func(*args)
    gen = func_()
    _TASKS.append((gen, domain))
    return gen


def flush_tasks():
    i = 0
    while _TASKS:
        i += 1
        if i > 1000000:
            raise ESPHomeYAMLError("Circular dependency detected!")

        task, domain = _TASKS.popleft()
        _LOGGER.debug("Executing task for domain=%s", domain)
        try:
            task.next()
            _TASKS.append((task, domain))
        except StopIteration:
            _LOGGER.debug(" -> %s finished", domain)


def add(expression, require=True):
    if require and isinstance(expression, Expression):
        expression.require()
    _EXPRESSIONS.append(expression)
    _LOGGER.debug("Adding: %s", statement(expression))
    return expression


class MockObj(Expression):
    def __init__(self, base, op=u'.', has_side_effects=True):
        self.base = base
        self.op = op
        self._has_side_effects = has_side_effects
        super(MockObj, self).__init__()

    def __getattr__(self, attr):
        if attr == u'_':
            obj = MockObj(u'{}{}'.format(self.base, self.op))
            obj.requires.append(self)
            return obj
        if attr == u'new':
            obj = MockObj(u'new {}'.format(self.base), u'->')
            obj.requires.append(self)
            return obj
        next_op = u'.'
        if attr.startswith(u'P') and self.op not in ['::', '']:
            attr = attr[1:]
            next_op = u'->'
        if attr.startswith(u'_'):
            attr = attr[1:]
        obj = MockObj(u'{}{}{}'.format(self.base, self.op, attr), next_op)
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

    def template(self, args):
        if not isinstance(args, TemplateArguments):
            args = TemplateArguments(args)
        obj = MockObj(u'{}{}'.format(self.base, args))
        obj.requires.append(self)
        obj.requires.append(args)
        return obj

    def namespace(self, name):
        obj = MockObj(u'{}{}{}'.format(self.base, self.op, name), u'::')
        obj.requires.append(self)
        return obj

    def operator(self, name):
        if name == 'ref':
            obj = MockObj(u'{} &'.format(self.base), u'')
            obj.requires.append(self)
            return obj
        raise NotImplementedError()

    def has_side_effects(self):
        return self._has_side_effects

    def __getitem__(self, item):
        next_op = u'.'
        if isinstance(item, str) and item.startswith(u'P'):
            item = item[1:]
            next_op = u'->'
        obj = MockObj(u'{}[{}]'.format(self.base, item), next_op)
        obj.requires.append(self)
        if isinstance(item, Expression):
            obj.requires.append(item)
        return obj


global_ns = MockObj('', '')
float_ = global_ns.namespace('float')
bool_ = global_ns.namespace('bool')
std_ns = global_ns.namespace('std')
std_string = std_ns.string
uint8 = global_ns.namespace('uint8_t')
uint16 = global_ns.namespace('uint16_t')
uint32 = global_ns.namespace('uint32_t')
const_char_p = global_ns.namespace('const char *')
NAN = global_ns.namespace('NAN')
esphomelib_ns = global_ns  # using namespace esphomelib;
NoArg = esphomelib_ns.NoArg
App = esphomelib_ns.App
Application = esphomelib_ns.namespace('Application')
optional = esphomelib_ns.optional

GPIOPin = esphomelib_ns.GPIOPin
GPIOOutputPin = esphomelib_ns.GPIOOutputPin
GPIOInputPin = esphomelib_ns.GPIOInputPin


def get_gpio_pin_number(conf):
    if isinstance(conf, int):
        return conf
    return conf[CONF_NUMBER]


def generic_gpio_pin_expression_(conf, mock_obj, default_mode):
    if conf is None:
        return
    number = conf[CONF_NUMBER]
    inverted = conf.get(CONF_INVERTED)
    if CONF_PCF8574 in conf:
        hub = None
        for hub in get_variable(conf[CONF_PCF8574]):
            yield None
        if default_mode == u'INPUT':
            mode = conf.get(CONF_MODE, u'INPUT')
            yield hub.make_input_pin(number,
                                     RawExpression('PCF8574_' + mode),
                                     inverted)
            return
        elif default_mode == u'OUTPUT':
            yield hub.make_output_pin(number, inverted)
            return
        else:
            raise ESPHomeYAMLError(u"Unknown default mode {}".format(default_mode))
    if len(conf) == 1:
        yield IntLiteral(number)
        return
    mode = RawExpression(conf.get(CONF_MODE, default_mode))
    yield mock_obj(number, mode, inverted)


def gpio_output_pin_expression(conf):
    exp = None
    for exp in generic_gpio_pin_expression_(conf, GPIOOutputPin, 'OUTPUT'):
        yield None
    yield exp


def gpio_input_pin_expression(conf):
    exp = None
    for exp in generic_gpio_pin_expression_(conf, GPIOInputPin, 'INPUT'):
        yield None
    yield exp


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
        if not availability:
            add(obj.disable_availability())
        else:
            add(obj.set_availability(availability[CONF_TOPIC], availability[CONF_PAYLOAD_AVAILABLE],
                                     availability[CONF_PAYLOAD_NOT_AVAILABLE]))


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


def relative_path(path):
    return os.path.join(os.path.dirname(core.CONFIG_PATH), os.path.expanduser(path))
