import inspect

import math

# pylint: disable=unused-import, wrong-import-order
from typing import Any, Generator, List, Optional, Tuple, Type, Union, Dict, Callable  # noqa

from esphome.core import (  # noqa
    CORE, HexInt, ID, Lambda, TimePeriod, TimePeriodMicroseconds,
    TimePeriodMilliseconds, TimePeriodMinutes, TimePeriodSeconds, coroutine, Library, Define,
    EnumValue)
from esphome.helpers import cpp_string_escape, indent_all_but_first_and_last
from esphome.py_compat import integer_types, string_types, text_type
from esphome.util import OrderedDict


class Expression(object):
    def __str__(self):
        raise NotImplementedError


SafeExpType = Union[Expression, bool, str, text_type, int, float, TimePeriod,
                    Type[bool], Type[int], Type[float], List[Any]]


class RawExpression(Expression):
    def __init__(self, text):  # type: (Union[str, unicode]) -> None
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
        self.obj = obj

    def __str__(self):
        if self.type is None:
            return u"{} = {}".format(self.name, self.rhs)
        return u"{} {}{} = {}".format(self.type, self.modifier, self.name, self.rhs)


class VariableDeclarationExpression(Expression):
    def __init__(self, type, modifier, name):
        super(VariableDeclarationExpression, self).__init__()
        self.type = type
        self.modifier = modifier
        self.name = name

    def __str__(self):
        return u"{} {}{}".format(self.type, self.modifier, self.name)


class ExpressionList(Expression):
    def __init__(self, *args):
        super(ExpressionList, self).__init__()
        # Remove every None on end
        args = list(args)
        while args and args[-1] is None:
            args.pop()
        self.args = [safe_exp(arg) for arg in args]

    def __str__(self):
        text = u", ".join(text_type(x) for x in self.args)
        return indent_all_but_first_and_last(text)

    def __iter__(self):
        return iter(self.args)


class TemplateArguments(Expression):
    def __init__(self, *args):  # type: (*SafeExpType) -> None
        super(TemplateArguments, self).__init__()
        self.args = ExpressionList(*args)

    def __str__(self):
        return u'<{}>'.format(self.args)

    def __iter__(self):
        return iter(self.args)


class CallExpression(Expression):
    def __init__(self, base, *args):  # type: (Expression, *SafeExpType) -> None
        super(CallExpression, self).__init__()
        self.base = base
        if args and isinstance(args[0], TemplateArguments):
            self.template_args = args[0]
            args = args[1:]
        else:
            self.template_args = None
        self.args = ExpressionList(*args)

    def __str__(self):
        if self.template_args is not None:
            return u'{}{}({})'.format(self.base, self.template_args, self.args)
        return u'{}({})'.format(self.base, self.args)


class StructInitializer(Expression):
    def __init__(self, base, *args):  # type: (Expression, *Tuple[str, SafeExpType]) -> None
        super(StructInitializer, self).__init__()
        self.base = base
        if not isinstance(args, OrderedDict):
            args = OrderedDict(args)
        self.args = OrderedDict()
        for key, value in args.items():
            if value is None:
                continue
            exp = safe_exp(value)
            self.args[key] = exp

    def __str__(self):
        cpp = u'{}{{\n'.format(self.base)
        for key, value in self.args.items():
            cpp += u'  .{} = {},\n'.format(key, value)
        cpp += u'}'
        return cpp


class ArrayInitializer(Expression):
    def __init__(self, *args, **kwargs):  # type: (*Any, **Any) -> None
        super(ArrayInitializer, self).__init__()
        self.multiline = kwargs.get('multiline', False)
        self.args = []
        for arg in args:
            if arg is None:
                continue
            exp = safe_exp(arg)
            self.args.append(exp)

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


class ParameterExpression(Expression):
    def __init__(self, type, id):
        super(ParameterExpression, self).__init__()
        self.type = safe_exp(type)
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

    def __str__(self):
        return u", ".join(text_type(x) for x in self.parameters)


class LambdaExpression(Expression):
    def __init__(self, parts, parameters, capture='=', return_type=None):
        super(LambdaExpression, self).__init__()
        self.parts = parts
        if not isinstance(parameters, ParameterListExpression):
            parameters = ParameterListExpression(*parameters)
        self.parameters = parameters
        self.capture = capture
        self.return_type = safe_exp(return_type) if return_type is not None else None

    def __str__(self):
        cpp = u'[{}]({})'.format(self.capture, self.parameters)
        if self.return_type is not None:
            cpp += u' -> {}'.format(self.return_type)
        cpp += u' {{\n{}\n}}'.format(self.content)
        return indent_all_but_first_and_last(cpp)

    @property
    def content(self):
        return u''.join(text_type(part) for part in self.parts)


class Literal(Expression):
    def __str__(self):
        raise NotImplementedError


class StringLiteral(Literal):
    def __init__(self, string):  # type: (Union[str, unicode]) -> None
        super(StringLiteral, self).__init__()
        self.string = string

    def __str__(self):
        return u'{}'.format(cpp_string_escape(self.string))


class IntLiteral(Literal):
    def __init__(self, i):  # type: (Union[int, long]) -> None
        super(IntLiteral, self).__init__()
        self.i = i

    def __str__(self):
        if self.i > 4294967295:
            return u'{}ULL'.format(self.i)
        if self.i > 2147483647:
            return u'{}UL'.format(self.i)
        if self.i < -2147483648:
            return u'{}LL'.format(self.i)
        return text_type(self.i)


class BoolLiteral(Literal):
    def __init__(self, binary):  # type: (bool) -> None
        super(BoolLiteral, self).__init__()
        self.binary = binary

    def __str__(self):
        return u"true" if self.binary else u"false"


class HexIntLiteral(Literal):
    def __init__(self, i):  # type: (int) -> None
        super(HexIntLiteral, self).__init__()
        self.i = HexInt(i)

    def __str__(self):
        return str(self.i)


class FloatLiteral(Literal):
    def __init__(self, value):  # type: (float) -> None
        super(FloatLiteral, self).__init__()
        self.float_ = value

    def __str__(self):
        if math.isnan(self.float_):
            return u"NAN"
        return u"{:f}f".format(self.float_)


# pylint: disable=bad-continuation
def safe_exp(
        obj  # type: Union[Expression, bool, str, unicode, int, long, float, TimePeriod, list]
             ):
    # type: (...) -> Expression
    """Try to convert obj to an expression by automatically converting native python types to
    expressions/literals.
    """
    from esphome.cpp_types import bool_, float_, int32

    if isinstance(obj, Expression):
        return obj
    if isinstance(obj, EnumValue):
        return safe_exp(obj.enum_value)
    if isinstance(obj, bool):
        return BoolLiteral(obj)
    if isinstance(obj, string_types):
        return StringLiteral(obj)
    if isinstance(obj, HexInt):
        return HexIntLiteral(obj)
    if isinstance(obj, integer_types):
        return IntLiteral(obj)
    if isinstance(obj, float):
        return FloatLiteral(obj)
    if isinstance(obj, TimePeriodMicroseconds):
        return IntLiteral(int(obj.total_microseconds))
    if isinstance(obj, TimePeriodMilliseconds):
        return IntLiteral(int(obj.total_milliseconds))
    if isinstance(obj, TimePeriodSeconds):
        return IntLiteral(int(obj.total_seconds))
    if isinstance(obj, TimePeriodMinutes):
        return IntLiteral(int(obj.total_minutes))
    if isinstance(obj, (tuple, list)):
        return ArrayInitializer(*[safe_exp(o) for o in obj])
    if obj is bool:
        return bool_
    if obj is int:
        return int32
    if obj is float:
        return float_
    if isinstance(obj, ID):
        raise ValueError(u"Object {} is an ID. Did you forget to register the variable?"
                         u"".format(obj))
    if inspect.isgenerator(obj):
        raise ValueError(u"Object {} is a coroutine. Did you forget to await the expression with "
                         u"'yield'?".format(obj))
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


class LineComment(Statement):
    def __init__(self, value):  # type: (unicode) -> None
        super(LineComment, self).__init__()
        self._value = value

    def __str__(self):
        parts = self._value.split(u'\n')
        parts = [u'// {}'.format(x) for x in parts]
        return u'\n'.join(parts)


class ProgmemAssignmentExpression(AssignmentExpression):
    def __init__(self, type, name, rhs, obj):
        super(ProgmemAssignmentExpression, self).__init__(
            type, '', name, rhs, obj
        )

    def __str__(self):
        type_ = self.type
        return u"static const {} {}[] PROGMEM = {}".format(type_, self.name, self.rhs)


def progmem_array(id, rhs):
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'.')
    assignment = ProgmemAssignmentExpression(id.type, id, rhs, obj)
    CORE.add(assignment)
    CORE.register_variable(id, obj)
    return obj


def statement(expression):  # type: (Union[Expression, Statement]) -> Statement
    if isinstance(expression, Statement):
        return expression
    return ExpressionStatement(expression)


def variable(id,  # type: ID
             rhs,  # type: SafeExpType
             type=None  # type: MockObj
             ):
    # type: (...) -> MockObj
    """Declare a new variable (not pointer type) in the code generation.

    :param id: The ID used to declare the variable.
    :param rhs: The expression to place on the right hand side of the assignment.
    :param type: Manually define a type for the variable, only use this when it's not possible
      to do so during config validation phase (for example because of template arguments).

    :returns The new variable as a MockObj.
    """
    assert isinstance(id, ID)
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'.')
    if type is not None:
        id.type = type
    assignment = AssignmentExpression(id.type, '', id, rhs, obj)
    CORE.add(assignment)
    CORE.register_variable(id, obj)
    return obj


def Pvariable(id,  # type: ID
              rhs,  # type: SafeExpType
              type=None  # type: MockObj
              ):
    # type: (...) -> MockObj
    """Declare a new pointer variable in the code generation.

    :param id: The ID used to declare the variable.
    :param rhs: The expression to place on the right hand side of the assignment.
    :param type: Manually define a type for the variable, only use this when it's not possible
      to do so during config validation phase (for example because of template arguments).

    :returns The new variable as a MockObj.
    """
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'->')
    if type is not None:
        id.type = type
    decl = VariableDeclarationExpression(id.type, '*', id)
    CORE.add_global(decl)
    assignment = AssignmentExpression(None, None, id, rhs, obj)
    CORE.add(assignment)
    CORE.register_variable(id, obj)
    return obj


def new_Pvariable(id,  # type: ID
                  *args  # type: *SafeExpType
                  ):
    """Declare a new pointer variable in the code generation by calling it's constructor
    with the given arguments.

    :param id: The ID used to declare the variable (also specifies the type).
    :param args: The values to pass to the constructor.

    :returns The new variable as a MockObj.
    """
    if args and isinstance(args[0], TemplateArguments):
        id = id.copy()
        id.type = id.type.template(args[0])
        args = args[1:]
    rhs = id.type.new(*args)
    return Pvariable(id, rhs)


def add(expression,  # type: Union[Expression, Statement]
        ):
    # type: (...) -> None
    """Add an expression to the codegen section.

    After this is called, the given given expression will
    show up in the setup() function after this has been called.
    """
    CORE.add(expression)


def add_global(expression,  # type: Union[SafeExpType, Statement]
               ):
    # type: (...) -> None
    """Add an expression to the codegen global storage (above setup())."""
    CORE.add_global(expression)


def add_library(name,  # type: str
                version  # type: Optional[str]
                ):
    # type: (...) -> None
    """Add a library to the codegen library storage.

    :param name: The name of the library (for example 'AsyncTCP')
    :param version: The version of the library, may be None.
    """
    CORE.add_library(Library(name, version))


def add_build_flag(build_flag,  # type: str
                   ):
    # type: (...) -> None
    """Add a global build flag to the compiler flags."""
    CORE.add_build_flag(build_flag)


def add_define(name,  # type: str
               value=None,  # type: Optional[SafeExpType]
               ):
    # type: (...) -> None
    """Add a global define to the auto-generated defines.h file.

    Optionally define a value to set this define to.
    """
    if value is None:
        CORE.add_define(Define(name))
    else:
        CORE.add_define(Define(name, safe_exp(value)))


@coroutine
def get_variable(id):  # type: (ID) -> Generator[MockObj]
    """
    Wait for the given ID to be defined in the code generation and
    return it as a MockObj.

    This is a coroutine, you need to await it with a 'yield' expression!

    :param id: The ID to retrieve
    :return: The variable as a MockObj.
    """
    var = yield CORE.get_variable(id)
    yield var


@coroutine
def get_variable_with_full_id(id):  # type: (ID) -> Generator[ID, MockObj]
    """
    Wait for the given ID to be defined in the code generation and
    return it as a MockObj.

    This is a coroutine, you need to await it with a 'yield' expression!

    :param id: The ID to retrieve
    :return: The variable as a MockObj.
    """
    full_id, var = yield CORE.get_variable_with_full_id(id)
    yield full_id, var


@coroutine
def process_lambda(value,  # type: Lambda
                   parameters,  # type: List[Tuple[SafeExpType, str]]
                   capture='=',  # type: str
                   return_type=None  # type: Optional[SafeExpType]
                   ):
    # type: (...) -> Generator[LambdaExpression]
    """Process the given lambda value into a LambdaExpression.

    This is a coroutine because lambdas can depend on other IDs,
    you need to await it with 'yield'!

    :param value: The lambda to process.
    :param parameters: The parameters to pass to the Lambda, list of tuples
    :param capture: The capture expression for the lambda, usually ''.
    :param return_type: The return type of the lambda.
    :return: The generated lambda expression.
    """
    from esphome.components.globals import GlobalsComponent

    if value is None:
        yield
        return
    parts = value.parts[:]
    for i, id in enumerate(value.requires_ids):
        full_id, var = yield CORE.get_variable_with_full_id(id)
        if full_id is not None and isinstance(full_id.type, MockObjClass) and \
                full_id.type.inherits_from(GlobalsComponent):
            parts[i * 3 + 1] = var.value()
            continue

        if parts[i * 3 + 2] == '.':
            parts[i * 3 + 1] = var._
        else:
            parts[i * 3 + 1] = var
        parts[i * 3 + 2] = ''
    yield LambdaExpression(parts, parameters, capture, return_type)


def is_template(value):
    """Return if value is a lambda expression."""
    return isinstance(value, Lambda)


@coroutine
def templatable(value,  # type: Any
                args,  # type: List[Tuple[SafeExpType, str]]
                output_type,  # type: Optional[SafeExpType],
                to_exp=None  # type: Optional[Any]
                ):
    """Generate code for a templatable config option.

    If `value` is a templated value, the lambda expression is returned.
    Otherwise the value is returned as-is (optionally process with to_exp).

    :param value: The value to process.
    :param args: The arguments for the lambda expression.
    :param output_type: The output type of the lambda expression.
    :param to_exp: An optional callable to use for converting non-templated values.
    :return: The potentially templated value.
    """
    if is_template(value):
        lambda_ = yield process_lambda(value, args, return_type=output_type)
        yield lambda_
    else:
        if to_exp is None:
            yield value
        elif isinstance(to_exp, dict):
            yield to_exp[value]
        else:
            yield to_exp(value)


class MockObj(Expression):
    """A general expression that can be used to represent any value.

    Mostly consists of magic methods that allow ESPHome's codegen syntax.
    """
    def __init__(self, base, op=u'.'):
        self.base = base
        self.op = op
        super(MockObj, self).__init__()

    def __getattr__(self, attr):  # type: (str) -> MockObj
        next_op = u'.'
        if attr.startswith(u'P') and self.op not in ['::', '']:
            attr = attr[1:]
            next_op = u'->'
        if attr.startswith(u'_'):
            attr = attr[1:]
        return MockObj(u'{}{}{}'.format(self.base, self.op, attr), next_op)

    def __call__(self, *args):  # type: (SafeExpType) -> MockObj
        call = CallExpression(self.base, *args)
        return MockObj(call, self.op)

    def __str__(self):  # type: () -> unicode
        return text_type(self.base)

    def __repr__(self):
        return u'MockObj<{}>'.format(text_type(self.base))

    @property
    def _(self):  # type: () -> MockObj
        return MockObj(u'{}{}'.format(self.base, self.op))

    @property
    def new(self):  # type: () -> MockObj
        return MockObj(u'new {}'.format(self.base), u'->')

    def template(self, *args):  # type: (*SafeExpType) -> MockObj
        if len(args) != 1 or not isinstance(args[0], TemplateArguments):
            args = TemplateArguments(*args)
        else:
            args = args[0]
        return MockObj(u'{}{}'.format(self.base, args))

    def namespace(self, name):  # type: (str) -> MockObj
        return MockObj(u'{}{}'.format(self._, name), u'::')

    def class_(self, name, *parents):  # type: (str, *MockObjClass) -> MockObjClass
        op = '' if self.op == '' else '::'
        return MockObjClass(u'{}{}{}'.format(self.base, op, name), u'.', parents=parents)

    def struct(self, name):  # type: (str) -> MockObjClass
        return self.class_(name)

    def enum(self, name, is_class=False):  # type: (str, bool) -> MockObj
        return MockObjEnum(enum=name, is_class=is_class, base=self.base, op=self.op)

    def operator(self, name):  # type: (str) -> MockObj
        if name == 'ref':
            return MockObj(u'{} &'.format(self.base), u'')
        if name == 'ptr':
            return MockObj(u'{} *'.format(self.base), u'')
        if name == "const":
            return MockObj(u'const {}'.format(self.base), u'')
        raise NotImplementedError

    @property
    def using(self):  # type: () -> MockObj
        assert self.op == '::'
        return MockObj(u'using namespace {}'.format(self.base))

    def __getitem__(self, item):  # type: (Union[str, Expression]) -> MockObj
        next_op = u'.'
        if isinstance(item, str) and item.startswith(u'P'):
            item = item[1:]
            next_op = u'->'
        return MockObj(u'{}[{}]'.format(self.base, item), next_op)


class MockObjEnum(MockObj):
    def __init__(self, *args, **kwargs):
        self._enum = kwargs.pop('enum')
        self._is_class = kwargs.pop('is_class')
        base = kwargs.pop('base')
        if self._is_class:
            base = base + '::' + self._enum
            kwargs['op'] = '::'
        kwargs['base'] = base
        MockObj.__init__(self, *args, **kwargs)

    def __str__(self):  # type: () -> unicode
        if self._is_class:
            return super(MockObjEnum, self).__str__()
        return u'{}{}{}'.format(self.base, self.op, self._enum)

    def __repr__(self):
        return u'MockObj<{}>'.format(text_type(self.base))


class MockObjClass(MockObj):
    def __init__(self, *args, **kwargs):
        parens = kwargs.pop('parents')
        MockObj.__init__(self, *args, **kwargs)
        self._parents = []
        for paren in parens:
            if not isinstance(paren, MockObjClass):
                raise ValueError
            self._parents.append(paren)
            # pylint: disable=protected-access
            self._parents += paren._parents

    def inherits_from(self, other):  # type: (MockObjClass) -> bool
        if self == other:
            return True
        for parent in self._parents:
            if parent == other:
                return True
        return False

    def template(self, *args):
        # type: (*SafeExpType) -> MockObjClass
        if len(args) != 1 or not isinstance(args[0], TemplateArguments):
            args = TemplateArguments(*args)
        else:
            args = args[0]
        new_parents = self._parents[:]
        new_parents.append(self)
        return MockObjClass(u'{}{}'.format(self.base, args), parents=new_parents)

    def __repr__(self):
        return u'MockObjClass<{}, parents={}>'.format(text_type(self.base), self._parents)
