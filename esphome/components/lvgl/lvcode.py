import abc
import logging
from typing import Union

from esphome import codegen as cg
from esphome.core import ID, Lambda
from esphome.cpp_generator import (
    AssignmentExpression,
    CallExpression,
    Expression,
    LambdaExpression,
    Literal,
    MockObj,
    RawExpression,
    RawStatement,
    SafeExpType,
    Statement,
    VariableDeclarationExpression,
    statement,
)

from .helpers import get_line_marks

_LOGGER = logging.getLogger(__name__)


class CodeContext(abc.ABC):
    """
    A class providing a context for code generation. Generated code will be added to the
    current context. A new context will stack on the current context, and restore it
    when done. Used with the `with` statement.
    """

    code_context = None

    @abc.abstractmethod
    def add(self, expression: Union[Expression, Statement]):
        pass

    @staticmethod
    def append(expression: Union[Expression, Statement]):
        if CodeContext.code_context is not None:
            CodeContext.code_context.add(expression)
        return expression

    def __init__(self):
        self.previous: Union[CodeContext | None] = None

    def __enter__(self):
        self.previous = CodeContext.code_context
        CodeContext.code_context = self

    def __exit__(self, *args):
        CodeContext.code_context = self.previous


class MainContext(CodeContext):
    """
    Code generation into the main() function
    """

    def add(self, expression: Union[Expression, Statement]):
        return cg.add(expression)


class LvContext(CodeContext):
    """
    Code generation into the LVGL initialisation code (called in `setup()`)
    """

    lv_init_code: list["Statement"] = []

    @staticmethod
    def lv_add(expression: Union[Expression, Statement]):
        if isinstance(expression, Expression):
            expression = statement(expression)
        if not isinstance(expression, Statement):
            raise ValueError(
                f"Add '{expression}' must be expression or statement, not {type(expression)}"
            )
        LvContext.lv_init_code.append(expression)
        _LOGGER.debug("LV Adding: %s", expression)
        return expression

    @staticmethod
    def get_code():
        code = []
        for exp in LvContext.lv_init_code:
            text = str(statement(exp))
            text = text.rstrip()
            code.append(text)
        return "\n".join(code) + "\n\n"

    def add(self, expression: Union[Expression, Statement]):
        return LvContext.lv_add(expression)

    def set_style(self, prop):
        return MockObj("lv_set_style_{prop}", "")


class LambdaContext(CodeContext):
    """
    A context that will accumlate code for use in a lambda.
    """

    def __init__(
        self,
        parameters: list[tuple[SafeExpType, str]],
        return_type: SafeExpType = None,
    ):
        super().__init__()
        self.code_list: list[Statement] = []
        self.parameters = parameters
        self.return_type = return_type

    def add(self, expression: Union[Expression, Statement]):
        self.code_list.append(expression)
        return expression

    async def code(self) -> LambdaExpression:
        code_text = []
        for exp in self.code_list:
            text = str(statement(exp))
            text = text.rstrip()
            code_text.append(text)
        return await cg.process_lambda(
            Lambda("\n".join(code_text) + "\n\n"),
            self.parameters,
            return_type=self.return_type,
        )


class LocalVariable(MockObj):
    """
    Create a local variable and enclose the code using it within a block.
    """

    def __init__(self, name, type, modifier=None, rhs=None):
        base = ID(name, True, type)
        super().__init__(base, "")
        self.modifier = modifier
        self.rhs = rhs

    def __enter__(self):
        CodeContext.append(RawStatement("{"))
        CodeContext.append(
            VariableDeclarationExpression(self.base.type, self.modifier, self.base.id)
        )
        if self.rhs is not None:
            CodeContext.append(AssignmentExpression(None, "", self.base, self.rhs))
        return self.base

    def __exit__(self, *args):
        CodeContext.append(RawStatement("}"))


class MockLv:
    """
    A mock object that can be used to generate LVGL calls.
    """

    def __init__(self, base):
        self.base = base

    def __getattr__(self, attr: str) -> "MockLv":
        return MockLv(f"{self.base}{attr}")

    def append(self, expression):
        CodeContext.append(expression)

    def __call__(self, *args: SafeExpType) -> "MockObj":
        call = CallExpression(self.base, *args)
        result = MockObj(call, "")
        self.append(result)
        return result

    def __str__(self):
        return str(self.base)

    def __repr__(self):
        return f"MockLv<{str(self.base)}>"

    def call(self, prop, *args):
        call = CallExpression(RawExpression(f"{self.base}{prop}"), *args)
        result = MockObj(call, "")
        self.append(result)
        return result

    def cond_if(self, expression: Expression):
        CodeContext.append(RawExpression(f"if({expression}) {{"))

    def cond_else(self):
        CodeContext.append(RawExpression("} else {"))

    def cond_endif(self):
        CodeContext.append(RawExpression("}"))


class LvExpr(MockLv):
    def __getattr__(self, attr: str) -> "MockLv":
        return LvExpr(f"{self.base}{attr}")

    def append(self, expression):
        pass


# Top level mock for generic lv_ calls to be recorded
lv = MockLv("lv_")
# Just generate an expression
lv_expr = LvExpr("lv_")
# Mock for lv_obj_ calls
lv_obj = MockLv("lv_obj_")


# equivalent to cg.add() for the lvgl init context
def lv_add(expression: Union[Expression, Statement]):
    return CodeContext.append(expression)


def add_line_marks(where):
    for mark in get_line_marks(where):
        lv_add(cg.RawStatement(mark))


def lv_assign(target, expression):
    lv_add(RawExpression(f"{target} = {expression}"))


class ConstantLiteral(Literal):
    __slots__ = ("constant",)

    def __init__(self, constant: str):
        super().__init__()
        self.constant = constant

    def __str__(self):
        return self.constant
