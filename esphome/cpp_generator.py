import abc
from collections.abc import Sequence
import inspect
import math
import re
from typing import Any, Callable, Optional, Union

from esphome.core import (
    CORE,
    ID,
    Define,
    EnumValue,
    HexInt,
    Lambda,
    Library,
    TimePeriod,
    TimePeriodMicroseconds,
    TimePeriodMilliseconds,
    TimePeriodMinutes,
    TimePeriodNanoseconds,
    TimePeriodSeconds,
)
from esphome.helpers import cpp_string_escape, indent_all_but_first_and_last
from esphome.util import OrderedDict
from esphome.yaml_util import ESPHomeDataBase


class Expression(abc.ABC):
    __slots__ = ()

    @abc.abstractmethod
    def __str__(self):
        """
        Convert expression into C++ code
        """


SafeExpType = Union[
    Expression,
    bool,
    str,
    str,
    int,
    float,
    TimePeriod,
    type[bool],
    type[int],
    type[float],
    Sequence[Any],
]


class RawExpression(Expression):
    __slots__ = ("text",)

    def __init__(self, text: str):
        self.text = text

    def __str__(self):
        return self.text


class AssignmentExpression(Expression):
    __slots__ = ("type", "modifier", "name", "rhs")

    def __init__(self, type_, modifier, name, rhs):
        self.type = type_
        self.modifier = modifier
        self.name = name
        self.rhs = safe_exp(rhs)

    def __str__(self):
        if self.type is None:
            return f"{self.name} = {self.rhs}"
        return f"{self.type} {self.modifier}{self.name} = {self.rhs}"


class VariableDeclarationExpression(Expression):
    __slots__ = ("type", "modifier", "name")

    def __init__(self, type_, modifier, name):
        self.type = type_
        self.modifier = modifier
        self.name = name

    def __str__(self):
        return f"{self.type} {self.modifier}{self.name}"


class ExpressionList(Expression):
    __slots__ = ("args",)

    def __init__(self, *args: Optional[SafeExpType]):
        # Remove every None on end
        args = list(args)
        while args and args[-1] is None:
            args.pop()
        self.args = [safe_exp(arg) for arg in args]

    def __str__(self):
        text = ", ".join(str(x) for x in self.args)
        return indent_all_but_first_and_last(text)

    def __iter__(self):
        return iter(self.args)


class TemplateArguments(Expression):
    __slots__ = ("args",)

    def __init__(self, *args: SafeExpType):
        self.args = ExpressionList(*args)

    def __str__(self):
        return f"<{self.args}>"

    def __iter__(self):
        return iter(self.args)


class CallExpression(Expression):
    __slots__ = ("base", "template_args", "args")

    def __init__(self, base: Expression, *args: SafeExpType):
        self.base = base
        if args and isinstance(args[0], TemplateArguments):
            self.template_args = args[0]
            args = args[1:]
        else:
            self.template_args = None
        self.args = ExpressionList(*args)

    def __str__(self):
        if self.template_args is not None:
            return f"{self.base}{self.template_args}({self.args})"
        return f"{self.base}({self.args})"


class StructInitializer(Expression):
    __slots__ = ("base", "args")

    def __init__(self, base: Expression, *args: tuple[str, Optional[SafeExpType]]):
        self.base = base
        # TODO: args is always a Tuple, is this check required?
        if not isinstance(args, OrderedDict):
            args = OrderedDict(args)
        self.args = OrderedDict()
        for key, value in args.items():
            if value is None:
                continue
            exp = safe_exp(value)
            self.args[key] = exp

    def __str__(self):
        cpp = f"{self.base}{{\n"
        for key, value in self.args.items():
            cpp += f"  .{key} = {value},\n"
        cpp += "}"
        return cpp


class ArrayInitializer(Expression):
    __slots__ = ("multiline", "args")

    def __init__(self, *args: Any, multiline: bool = False):
        self.multiline = multiline
        self.args = []
        for arg in args:
            if arg is None:
                continue
            exp = safe_exp(arg)
            self.args.append(exp)

    def __str__(self):
        if not self.args:
            return "{}"
        if self.multiline:
            cpp = "{\n  "
            cpp += ",\n  ".join(str(arg) for arg in self.args)
            cpp += ",\n}"
        else:
            cpp = f"{{{', '.join(str(arg) for arg in self.args)}}}"
        return cpp


class ParameterExpression(Expression):
    __slots__ = ("type", "id")

    def __init__(self, type_, id_):
        self.type = safe_exp(type_)
        self.id = id_

    def __str__(self):
        return f"{self.type} {self.id}"


class ParameterListExpression(Expression):
    __slots__ = ("parameters",)

    def __init__(
        self, *parameters: Union[ParameterExpression, tuple[SafeExpType, str]]
    ):
        self.parameters = []
        for parameter in parameters:
            if not isinstance(parameter, ParameterExpression):
                parameter = ParameterExpression(*parameter)
            self.parameters.append(parameter)

    def __str__(self):
        return ", ".join(str(x) for x in self.parameters)


class LambdaExpression(Expression):
    __slots__ = ("parts", "parameters", "capture", "return_type", "source")

    def __init__(
        self, parts, parameters, capture: str = "=", return_type=None, source=None
    ):
        self.parts = parts
        if not isinstance(parameters, ParameterListExpression):
            parameters = ParameterListExpression(*parameters)
        self.parameters = parameters
        self.source = source
        self.capture = capture
        self.return_type = safe_exp(return_type) if return_type is not None else None

    def __str__(self):
        cpp = f"[{self.capture}]({self.parameters})"
        if self.return_type is not None:
            cpp += f" -> {self.return_type}"
        cpp += " {\n"
        if self.source is not None:
            cpp += f"{self.source.as_line_directive}\n"
        cpp += f"{self.content}\n}}"
        return indent_all_but_first_and_last(cpp)

    @property
    def content(self):
        return "".join(str(part) for part in self.parts)


# pylint: disable=abstract-method
class Literal(Expression, metaclass=abc.ABCMeta):
    __slots__ = ()


class StringLiteral(Literal):
    __slots__ = ("string",)

    def __init__(self, string: str):
        super().__init__()
        self.string = string

    def __str__(self):
        return cpp_string_escape(self.string)


class IntLiteral(Literal):
    __slots__ = ("i",)

    def __init__(self, i: int):
        super().__init__()
        self.i = i

    def __str__(self):
        if self.i > 4294967295:
            return f"{self.i}ULL"
        if self.i > 2147483647:
            return f"{self.i}UL"
        if self.i < -2147483648:
            return f"{self.i}LL"
        return str(self.i)


class BoolLiteral(Literal):
    __slots__ = ("binary",)

    def __init__(self, binary: bool):
        super().__init__()
        self.binary = binary

    def __str__(self):
        return "true" if self.binary else "false"


class HexIntLiteral(Literal):
    __slots__ = ("i",)

    def __init__(self, i: int):
        super().__init__()
        self.i = HexInt(i)

    def __str__(self):
        return str(self.i)


class FloatLiteral(Literal):
    __slots__ = ("f",)

    def __init__(self, value: float):
        super().__init__()
        self.f = value

    def __str__(self):
        if math.isnan(self.f):
            return "NAN"
        return f"{self.f}f"


class BinOpExpression(Expression):
    __slots__ = ("op", "lhs", "rhs")

    def __init__(self, lhs: SafeExpType, op: str, rhs: SafeExpType):
        self.lhs = safe_exp(lhs)
        self.op = op
        self.rhs = safe_exp(rhs)

    def __str__(self):
        # Surround with parentheses to ensure generated code has same
        # order as python one
        return f"({self.lhs} {self.op} {self.rhs})"


class UnaryOpExpression(Expression):
    __slots__ = ("op", "exp")

    def __init__(self, op: str, exp: SafeExpType):
        self.op = op
        self.exp = safe_exp(exp)

    def __str__(self):
        return f"({self.op}{self.exp})"


def safe_exp(obj: SafeExpType) -> Expression:
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
    if isinstance(obj, str):
        return StringLiteral(obj)
    if isinstance(obj, HexInt):
        return HexIntLiteral(obj)
    if isinstance(obj, int):
        return IntLiteral(obj)
    if isinstance(obj, float):
        return FloatLiteral(obj)
    if isinstance(obj, TimePeriodNanoseconds):
        return IntLiteral(int(obj.total_nanoseconds))
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
        raise ValueError(
            f"Object {obj} is an ID. Did you forget to register the variable?"
        )
    if inspect.isgenerator(obj):
        raise ValueError(
            f"Object {obj} is a coroutine. Did you forget to await the expression with 'await'?"
        )
    raise ValueError("Object is not an expression", obj)


class Statement(abc.ABC):
    __slots__ = ()

    @abc.abstractmethod
    def __str__(self):
        """
        Convert statement into C++ code
        """


class RawStatement(Statement):
    __slots__ = ("text",)

    def __init__(self, text: str):
        self.text = text

    def __str__(self):
        return self.text


class ExpressionStatement(Statement):
    __slots__ = ("expression",)

    def __init__(self, expression):
        self.expression = safe_exp(expression)

    def __str__(self):
        return f"{self.expression};"


class LineComment(Statement):
    __slots__ = ("value",)

    def __init__(self, value: str):
        self.value = value

    def __str__(self):
        parts = re.sub(r"\\\s*\n", r"<cont>\n", self.value, re.MULTILINE).split("\n")
        parts = [f"// {x}" for x in parts]
        return "\n".join(parts)


class ProgmemAssignmentExpression(AssignmentExpression):
    __slots__ = ()

    def __init__(self, type_, name, rhs):
        super().__init__(type_, "", name, rhs)

    def __str__(self):
        return f"static const {self.type} {self.name}[] PROGMEM = {self.rhs}"


class StaticConstAssignmentExpression(AssignmentExpression):
    __slots__ = ()

    def __init__(self, type_, name, rhs):
        super().__init__(type_, "", name, rhs)

    def __str__(self):
        return f"static const {self.type} {self.name}[] = {self.rhs}"


def progmem_array(id_, rhs) -> "MockObj":
    rhs = safe_exp(rhs)
    obj = MockObj(id_, ".")
    assignment = ProgmemAssignmentExpression(id_.type, id_, rhs)
    CORE.add(assignment)
    CORE.register_variable(id_, obj)
    return obj


def static_const_array(id_, rhs) -> "MockObj":
    rhs = safe_exp(rhs)
    obj = MockObj(id_, ".")
    assignment = StaticConstAssignmentExpression(id_.type, id_, rhs)
    CORE.add(assignment)
    CORE.register_variable(id_, obj)
    return obj


def statement(expression: Union[Expression, Statement]) -> Statement:
    """Convert expression into a statement unless is already a statement."""
    if isinstance(expression, Statement):
        return expression
    return ExpressionStatement(expression)


def variable(
    id_: ID, rhs: SafeExpType, type_: "MockObj" = None, register=True
) -> "MockObj":
    """Declare a new variable, not pointer type, in the code generation.

    :param id_: The ID used to declare the variable.
    :param rhs: The expression to place on the right hand side of the assignment.
    :param type_: Manually define a type for the variable, only use this when it's not possible
      to do so during config validation phase (for example because of template arguments).
    :param register: If true register the variable with the core

    :return: The new variable as a MockObj.
    """
    assert isinstance(id_, ID)
    rhs = safe_exp(rhs)
    obj = MockObj(id_, ".")
    if type_ is not None:
        id_.type = type_
    assignment = AssignmentExpression(id_.type, "", id_, rhs)
    CORE.add(assignment)
    if register:
        CORE.register_variable(id_, obj)
    return obj


def with_local_variable(id_: ID, rhs: SafeExpType, callback: Callable, *args) -> None:
    """Declare a new variable, not pointer type, in the code generation, within a scoped block
    The variable is only usable within the callback
    The callback cannot be async.

    :param id_: The ID used to declare the variable.
    :param rhs: The expression to place on the right hand side of the assignment.
    :param callback: The function to invoke that will receive the temporary variable
    :param args: args to pass to the callback in addition to the temporary variable

    """

    # throw if the callback is async:
    assert not inspect.iscoroutinefunction(
        callback
    ), "with_local_variable() callback cannot be async!"

    CORE.add(RawStatement("{"))  # output opening curly brace
    obj = variable(id_, rhs, None, True)
    # invoke user-provided callback to generate code with this local variable
    callback(obj, *args)
    CORE.add(RawStatement("}"))  # output closing curly brace


def new_variable(id_: ID, rhs: SafeExpType, type_: "MockObj" = None) -> "MockObj":
    """Declare and define a new variable, not pointer type, in the code generation.

    :param id_: The ID used to declare the variable.
    :param rhs: The expression to place on the right hand side of the assignment.
    :param type_: Manually define a type for the variable, only use this when it's not possible
      to do so during config validation phase (for example because of template arguments).

    :return: The new variable as a MockObj.
    """
    assert isinstance(id_, ID)
    rhs = safe_exp(rhs)
    obj = MockObj(id_, ".")
    if type_ is not None:
        id_.type = type_
    decl = VariableDeclarationExpression(id_.type, "", id_)
    CORE.add_global(decl)
    assignment = AssignmentExpression(None, "", id_, rhs)
    CORE.add(assignment)
    CORE.register_variable(id_, obj)
    return obj


def Pvariable(id_: ID, rhs: SafeExpType, type_: "MockObj" = None) -> "MockObj":
    """Declare a new pointer variable in the code generation.

    :param id_: The ID used to declare the variable.
    :param rhs: The expression to place on the right hand side of the assignment.
    :param type_: Manually define a type for the variable, only use this when it's not possible
      to do so during config validation phase (for example because of template arguments).

    :return: The new variable as a MockObj.
    """
    rhs = safe_exp(rhs)
    obj = MockObj(id_, "->")
    if type_ is not None:
        id_.type = type_
    decl = VariableDeclarationExpression(id_.type, "*", id_)
    CORE.add_global(decl)
    assignment = AssignmentExpression(None, None, id_, rhs)
    CORE.add(assignment)
    CORE.register_variable(id_, obj)
    return obj


def new_Pvariable(id_: ID, *args: SafeExpType) -> Pvariable:
    """Declare a new pointer variable in the code generation by calling it's constructor
    with the given arguments.

    :param id_: The ID used to declare the variable (also specifies the type).
    :param args: The values to pass to the constructor.

    :return: The new variable as a MockObj.
    """
    if args and isinstance(args[0], TemplateArguments):
        id_ = id_.copy()
        id_.type = id_.type.template(args[0])
        args = args[1:]
    rhs = id_.type.new(*args)
    return Pvariable(id_, rhs)


def add(expression: Union[Expression, Statement]):
    """Add an expression to the codegen section.

    After this is called, the given given expression will
    show up in the setup() function after this has been called.
    """
    CORE.add(expression)


def add_global(expression: Union[SafeExpType, Statement], prepend: bool = False):
    """Add an expression to the codegen global storage (above setup())."""
    CORE.add_global(expression, prepend)


def add_library(name: str, version: Optional[str], repository: Optional[str] = None):
    """Add a library to the codegen library storage.

    :param name: The name of the library (for example 'AsyncTCP')
    :param version: The version of the library, may be None.
    :param repository: The repository for the library
    """
    CORE.add_library(Library(name, version, repository))


def add_build_flag(build_flag: str):
    """Add a global build flag to the compiler flags."""
    CORE.add_build_flag(build_flag)


def add_define(name: str, value: SafeExpType = None):
    """Add a global define to the auto-generated defines.h file.

    Optionally define a value to set this define to.
    """
    if value is None:
        CORE.add_define(Define(name))
    else:
        CORE.add_define(Define(name, safe_exp(value)))


def add_platformio_option(key: str, value: Union[str, list[str]]):
    CORE.add_platformio_option(key, value)


async def get_variable(id_: ID) -> "MockObj":
    """
    Wait for the given ID to be defined in the code generation and
    return it as a MockObj.

    This is a coroutine, you need to await it with a 'await' expression!

    :param id_: The ID to retrieve
    :return: The variable as a MockObj.
    """
    return await CORE.get_variable(id_)


async def get_variable_with_full_id(id_: ID) -> tuple[ID, "MockObj"]:
    """
    Wait for the given ID to be defined in the code generation and
    return it as a MockObj.

    This is a coroutine, you need to await it with a 'await' expression!

    :param id_: The ID to retrieve
    :return: The variable as a MockObj.
    """
    return await CORE.get_variable_with_full_id(id_)


async def process_lambda(
    value: Lambda,
    parameters: list[tuple[SafeExpType, str]],
    capture: str = "=",
    return_type: SafeExpType = None,
) -> Union[LambdaExpression, None]:
    """Process the given lambda value into a LambdaExpression.

    This is a coroutine because lambdas can depend on other IDs,
    you need to await it with 'await'!

    :param value: The lambda to process.
    :param parameters: The parameters to pass to the Lambda, list of tuples
    :param capture: The capture expression for the lambda, usually ''.
    :param return_type: The return type of the lambda.
    :return: The generated lambda expression.
    """
    from esphome.components.globals import (
        GlobalsComponent,
        RestoringGlobalsComponent,
        RestoringGlobalStringComponent,
    )

    if value is None:
        return None
    parts = value.parts[:]
    for i, id in enumerate(value.requires_ids):
        full_id, var = await get_variable_with_full_id(id)
        if (
            full_id is not None
            and isinstance(full_id.type, MockObjClass)
            and (
                full_id.type.inherits_from(GlobalsComponent)
                or full_id.type.inherits_from(RestoringGlobalsComponent)
                or full_id.type.inherits_from(RestoringGlobalStringComponent)
            )
        ):
            parts[i * 3 + 1] = var.value()
            continue

        if parts[i * 3 + 2] == ".":
            parts[i * 3 + 1] = var._
        else:
            parts[i * 3 + 1] = var
        parts[i * 3 + 2] = ""

    if isinstance(value, ESPHomeDataBase) and value.esp_range is not None:
        location = value.esp_range.start_mark
        location.line += value.content_offset
    else:
        location = None
    return LambdaExpression(parts, parameters, capture, return_type, location)


def is_template(value):
    """Return if value is a lambda expression."""
    return isinstance(value, Lambda)


async def templatable(
    value: Any,
    args: list[tuple[SafeExpType, str]],
    output_type: Optional[SafeExpType],
    to_exp: Union[Callable, dict] = None,
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
        return await process_lambda(value, args, return_type=output_type)
    if to_exp is None:
        return value
    if isinstance(to_exp, dict):
        return to_exp[value]
    return to_exp(value)


class MockObj(Expression):
    """A general expression that can be used to represent any value.

    Mostly consists of magic methods that allow ESPHome's codegen syntax.
    """

    __slots__ = ("base", "op")

    def __init__(self, base, op="."):
        self.base = base
        self.op = op

    def __getattr__(self, attr: str) -> "MockObj":
        # prevent python dunder methods being replaced by mock objects
        if attr.startswith("__"):
            raise AttributeError()
        next_op = "."
        if attr.startswith("P") and self.op not in ["::", ""]:
            attr = attr[1:]
            next_op = "->"
        if attr.startswith("_"):
            attr = attr[1:]
        return MockObj(f"{self.base}{self.op}{attr}", next_op)

    def __call__(self, *args: SafeExpType) -> "MockObj":
        call = CallExpression(self.base, *args)
        return MockObj(call, self.op)

    def __str__(self):
        return str(self.base)

    def __repr__(self):
        return f"MockObj<{str(self.base)}>"

    @property
    def _(self) -> "MockObj":
        return MockObj(f"{self.base}{self.op}")

    @property
    def new(self) -> "MockObj":
        return MockObj(f"new {self.base}", "->")

    def template(self, *args: SafeExpType) -> "MockObj":
        """Apply template parameters to this object."""
        if len(args) != 1 or not isinstance(args[0], TemplateArguments):
            args = TemplateArguments(*args)
        else:
            args = args[0]
        return MockObj(f"{self.base}{args}")

    def namespace(self, name: str) -> "MockObj":
        return MockObj(f"{self._}{name}", "::")

    def class_(self, name: str, *parents: "MockObjClass") -> "MockObjClass":
        op = "" if self.op == "" else "::"
        return MockObjClass(f"{self.base}{op}{name}", ".", parents=parents)

    def struct(self, name: str) -> "MockObjClass":
        return self.class_(name)

    def enum(self, name: str, is_class: bool = False) -> "MockObj":
        return MockObjEnum(enum=name, is_class=is_class, base=self.base, op=self.op)

    def operator(self, name: str) -> "MockObj":
        """Various other operations.

        Named operator because it's a C++ keyword and can't occur in valid code.
        """
        if name == "ref":
            return MockObj(f"{self.base} &", "")
        if name == "ptr":
            return MockObj(f"{self.base} *", "")
        if name == "const_ptr":
            return MockObj(f"{self.base} *const", "")
        if name == "const":
            return MockObj(f"const {self.base}", "")
        raise ValueError("Expected one of ref, ptr, const_ptr, const.")

    @property
    def using(self) -> "MockObj":
        assert self.op == "::"
        return MockObj(f"using namespace {self.base}")

    def __getitem__(self, item: Union[str, Expression]) -> "MockObj":
        next_op = "."
        if isinstance(item, str) and item.startswith("P"):
            item = item[1:]
            next_op = "->"
        return MockObj(f"{self.base}[{item}]", next_op)

    def __lt__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "<", other)
        return MockObj(op)

    def __le__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "<=", other)
        return MockObj(op)

    def __eq__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "==", other)
        return MockObj(op)

    def __ne__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "!=", other)
        return MockObj(op)

    def __gt__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, ">", other)
        return MockObj(op)

    def __ge__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, ">=", other)
        return MockObj(op)

    def __add__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "+", other)
        return MockObj(op)

    def __sub__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "-", other)
        return MockObj(op)

    def __mul__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "*", other)
        return MockObj(op)

    def __truediv__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "/", other)
        return MockObj(op)

    def __mod__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "%", other)
        return MockObj(op)

    def __lshift__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "<<", other)
        return MockObj(op)

    def __rshift__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, ">>", other)
        return MockObj(op)

    def __and__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "&", other)
        return MockObj(op)

    def __xor__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "^", other)
        return MockObj(op)

    def __or__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "|", other)
        return MockObj(op)

    def __radd__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "+", self)
        return MockObj(op)

    def __rsub__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "-", self)
        return MockObj(op)

    def __rmul__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "*", self)
        return MockObj(op)

    def __rtruediv__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "/", self)
        return MockObj(op)

    def __rmod__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "%", self)
        return MockObj(op)

    def __rlshift__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "<<", self)
        return MockObj(op)

    def __rrshift__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, ">>", self)
        return MockObj(op)

    def __rand__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "&", self)
        return MockObj(op)

    def __rxor__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "^", self)
        return MockObj(op)

    def __ror__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(other, "|", self)
        return MockObj(op)

    def __iadd__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "+=", other)
        return MockObj(op)

    def __isub__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "-=", other)
        return MockObj(op)

    def __imul__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "*=", other)
        return MockObj(op)

    def __itruediv__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "/=", other)
        return MockObj(op)

    def __imod__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "%=", other)
        return MockObj(op)

    def __ilshift__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "<<=", other)
        return MockObj(op)

    def __irshift__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, ">>=", other)
        return MockObj(op)

    def __iand__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "&=", other)
        return MockObj(op)

    def __ixor__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "^=", other)
        return MockObj(op)

    def __ior__(self, other: SafeExpType) -> "MockObj":
        op = BinOpExpression(self, "|=", other)
        return MockObj(op)

    def __neg__(self) -> "MockObj":
        op = UnaryOpExpression("-", self)
        return MockObj(op)

    def __pos__(self) -> "MockObj":
        op = UnaryOpExpression("+", self)
        return MockObj(op)

    def __invert__(self) -> "MockObj":
        op = UnaryOpExpression("~", self)
        return MockObj(op)


class MockObjEnum(MockObj):
    def __init__(self, *args, **kwargs):
        self._enum = kwargs.pop("enum")
        self._is_class = kwargs.pop("is_class")
        base = kwargs.pop("base")
        if self._is_class:
            base = f"{base}::{self._enum}"
        kwargs["op"] = "::"
        kwargs["base"] = base
        MockObj.__init__(self, *args, **kwargs)

    def __str__(self):
        if self._is_class:
            return super().__str__()
        return f"{self.base}{self.op}{self._enum}"

    def __repr__(self):
        return f"MockObj<{str(self.base)}>"


class MockObjClass(MockObj):
    def __init__(self, *args, **kwargs):
        parens = kwargs.pop("parents")
        MockObj.__init__(self, *args, **kwargs)
        self._parents = []
        for paren in parens:
            if not isinstance(paren, MockObjClass):
                raise ValueError
            self._parents.append(paren)
            # pylint: disable=protected-access
            self._parents += paren._parents

    def inherits_from(self, other: "MockObjClass") -> bool:
        if str(self) == str(other):
            return True
        for parent in self._parents:
            if str(parent) == str(other):
                return True
        return False

    def template(self, *args: SafeExpType) -> "MockObjClass":
        if len(args) != 1 or not isinstance(args[0], TemplateArguments):
            args = TemplateArguments(*args)
        else:
            args = args[0]
        new_parents = self._parents[:]
        new_parents.append(self)
        return MockObjClass(f"{self.base}{args}", parents=new_parents)

    def __repr__(self):
        return f"MockObjClass<{str(self.base)}, parents={self._parents}>"
