from collections import OrderedDict

from esphome.core import CORE, HexInt, Lambda, TimePeriod, TimePeriodMicroseconds, \
    TimePeriodMilliseconds, TimePeriodSeconds
from esphome.helpers import cpp_string_escape, indent_all_but_first_and_last

# pylint: disable=unused-import, wrong-import-order
from typing import Any, Generator, List, Optional, Tuple, Union  # noqa
from esphome.core import ID  # noqa
from esphome.py_compat import text_type, string_types, integer_types


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


SafeExpType = Union[Expression, bool, str, text_type, int, float, TimePeriod]


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
        text = u", ".join(text_type(x) for x in self.args)
        return indent_all_but_first_and_last(text)


class TemplateArguments(Expression):
    def __init__(self, *args):  # type: (*SafeExpType) -> None
        super(TemplateArguments, self).__init__()
        self.args = ExpressionList(*args)
        self.requires.append(self.args)

    def __str__(self):
        return u'<{}>'.format(self.args)


class CallExpression(Expression):
    def __init__(self, base, *args):  # type: (Expression, *SafeExpType) -> None
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
    def __init__(self, base, *args):  # type: (Expression, *Tuple[str, SafeExpType]) -> None
        super(StructInitializer, self).__init__()
        self.base = base
        if isinstance(base, Expression):
            self.requires.append(base)
        if not isinstance(args, OrderedDict):
            args = OrderedDict(args)
        self.args = OrderedDict()
        for key, value in args.items():
            if value is None:
                continue
            exp = safe_exp(value)
            self.args[key] = exp
            self.requires.append(exp)

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
        return u", ".join(text_type(x) for x in self.parameters)


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
        return u"{:f}f".format(self.float_)


# pylint: disable=bad-continuation
def safe_exp(
        obj  # type: Union[Expression, bool, str, unicode, int, long, float, TimePeriod, list]
             ):
    # type: (...) -> Expression
    if isinstance(obj, Expression):
        return obj
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
    if isinstance(obj, (tuple, list)):
        return ArrayInitializer(*[safe_exp(o) for o in obj])
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
    obj.requires.append(assignment)
    return obj


def statement(expression):  # type: (Union[Expression, Statement]) -> Statement
    if isinstance(expression, Statement):
        return expression
    return ExpressionStatement(expression)


def variable(id,  # type: ID
             rhs,  # type: Expression
             type=None  # type: MockObj
             ):
    # type: (...) -> MockObj
    rhs = safe_exp(rhs)
    obj = MockObj(id, u'.')
    id.type = type or id.type
    assignment = AssignmentExpression(id.type, '', id, rhs, obj)
    CORE.add(assignment)
    CORE.register_variable(id, obj)
    obj.requires.append(assignment)
    return obj


def Pvariable(id,  # type: ID
              rhs,  # type: Expression
              has_side_effects=True,  # type: bool
              type=None  # type: MockObj
              ):
    # type: (...) -> MockObj
    rhs = safe_exp(rhs)
    if not has_side_effects and hasattr(rhs, '_has_side_effects'):
        # pylint: disable=attribute-defined-outside-init, protected-access
        rhs._has_side_effects = False
    obj = MockObj(id, u'->', has_side_effects=has_side_effects)
    id.type = type or id.type
    assignment = AssignmentExpression(id.type, '*', id, rhs, obj)
    CORE.add(assignment)
    CORE.register_variable(id, obj)
    obj.requires.append(assignment)
    return obj


def add(expression,  # type: Union[Expression, Statement]
        require=True  # type: bool
        ):
    # type: (...) -> None
    CORE.add(expression, require=require)


def get_variable(id):  # type: (ID) -> Generator[MockObj]
    for var in CORE.get_variable(id):
        yield None
    yield var


def process_lambda(value,  # type: Lambda
                   parameters,  # type: List[Tuple[Expression, str]]
                   capture='=',  # type: str
                   return_type=None  # type: Optional[Expression]
                   ):
    # type: (...) -> Generator[LambdaExpression]
    from esphome.components.globals import GlobalVariableComponent

    if value is None:
        yield
        return
    parts = value.parts[:]
    for i, id in enumerate(value.requires_ids):
        for full_id, var in CORE.get_variable_with_full_id(id):
            yield
        if full_id is not None and isinstance(full_id.type, MockObjClass) and \
                full_id.type.inherits_from(GlobalVariableComponent):
            parts[i * 3 + 1] = var.value()
            continue

        if parts[i * 3 + 2] == '.':
            parts[i * 3 + 1] = var._
        else:
            parts[i * 3 + 1] = var
        parts[i * 3 + 2] = ''
    yield LambdaExpression(parts, parameters, capture, return_type)


def templatable(value,  # type: Any
                input_type,  # type: Expression
                output_type  # type: Optional[Expression]
                ):
    if isinstance(value, Lambda):
        lambda_ = None
        for lambda_ in process_lambda(value, [(input_type, 'x')], return_type=output_type):
            yield None
        yield lambda_
    else:
        yield value


class MockObj(Expression):
    def __init__(self, base, op=u'.', has_side_effects=True):
        self.base = base
        self.op = op
        self._has_side_effects = has_side_effects
        super(MockObj, self).__init__()

    def __getattr__(self, attr):  # type: (str) -> MockObj
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

    def __call__(self, *args, **kwargs):  # type: (*Any, **Any) -> MockObj
        call = CallExpression(self.base, *args)
        obj = MockObj(call, self.op)
        obj.requires.append(self)
        obj.requires.append(call)
        return obj

    def __str__(self):  # type: () -> unicode
        return text_type(self.base)

    def require(self):  # type: () -> None
        self.required = True
        for require in self.requires:
            if require.required:
                continue
            require.require()

    def template(self, args):  # type: (Union[TemplateArguments, Expression]) -> MockObj
        if not isinstance(args, TemplateArguments):
            args = TemplateArguments(args)
        obj = MockObj(u'{}{}'.format(self.base, args))
        obj.requires.append(self)
        obj.requires.append(args)
        return obj

    def namespace(self, name):  # type: (str) -> MockObj
        obj = MockObj(u'{}{}{}'.format(self.base, self.op, name), u'::')
        obj.requires.append(self)
        return obj

    def class_(self, name, *parents):  # type: (str, *MockObjClass) -> MockObjClass
        op = '' if self.op == '' else '::'
        obj = MockObjClass(u'{}{}{}'.format(self.base, op, name), u'.', parents=parents)
        obj.requires.append(self)
        return obj

    def struct(self, name):  # type: (str) -> MockObjClass
        return self.class_(name)

    def enum(self, name, is_class=False):  # type: (str, bool) -> MockObj
        if is_class:
            return self.namespace(name)

        return self

    def operator(self, name):  # type: (str) -> MockObj
        if name == 'ref':
            obj = MockObj(u'{} &'.format(self.base), u'')
            obj.requires.append(self)
            return obj
        if name == 'ptr':
            obj = MockObj(u'{} *'.format(self.base), u'')
            obj.requires.append(self)
            return obj
        if name == "const":
            obj = MockObj(u'const {}'.format(self.base), u'')
            obj.requires.append(self)
            return obj
        raise NotImplementedError

    def has_side_effects(self):  # type: () -> bool
        return self._has_side_effects

    def __getitem__(self, item):  # type: (Union[str, Expression]) -> MockObj
        next_op = u'.'
        if isinstance(item, str) and item.startswith(u'P'):
            item = item[1:]
            next_op = u'->'
        obj = MockObj(u'{}[{}]'.format(self.base, item), next_op)
        obj.requires.append(self)
        if isinstance(item, Expression):
            obj.requires.append(item)
        return obj


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

    def template(self, args):  # type: (Union[TemplateArguments, Expression]) -> MockObjClass
        if not isinstance(args, TemplateArguments):
            args = TemplateArguments(args)
        new_parents = self._parents[:]
        new_parents.append(self)
        obj = MockObjClass(u'{}{}'.format(self.base, args), parents=new_parents)
        obj.requires.append(self)
        obj.requires.append(args)
        return obj
