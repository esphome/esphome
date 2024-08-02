import abc
import logging
from typing import Union

from esphome import codegen as cg
from esphome.config import Config
from esphome.core import CORE, ID, Lambda
from esphome.cpp_generator import (
    AssignmentExpression,
    CallExpression,
    Expression,
    ExpressionStatement,
    LambdaExpression,
    MockObj,
    RawExpression,
    RawStatement,
    SafeExpType,
    Statement,
    VariableDeclarationExpression,
    statement,
)
from esphome.yaml_util import ESPHomeDataBase

_LOGGER = logging.getLogger(__name__)

LVGL_COMP = "lvgl_comp"  # used as a lambda argument in lvgl_comp()


def get_line_marks(value) -> list:
    """
    If possible, return a preprocessor directive to identify the line number where the given id was defined.
    :param value: The id or other token to get the line number for
    :return: A list containing zero or more line directives
    """
    path = None
    if isinstance(value, ESPHomeDataBase):
        path = value.esp_range
    elif isinstance(value, ID) and isinstance(CORE.config, Config):
        path = CORE.config.get_path_for_id(value)[:-1]
        path = CORE.config.get_deepest_document_range_for_path(path)
    if path is None:
        return []
    return [path.start_mark.as_line_directive]


class IndentedStatement(Statement):
    def __init__(self, stmt: Statement, indent: int):
        self.statement = stmt
        self.indent = indent

    def __str__(self):
        result = " " * self.indent * 4 + str(self.statement).strip()
        if not isinstance(self.statement, RawStatement):
            result += ";"
        return result


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
    def start_block():
        CodeContext.append(RawStatement("{"))
        CodeContext.code_context.indent()

    @staticmethod
    def end_block():
        CodeContext.code_context.detent()
        CodeContext.append(RawStatement("}"))

    @staticmethod
    def append(expression: Union[Expression, Statement]):
        if CodeContext.code_context is not None:
            CodeContext.code_context.add(expression)
        return expression

    def __init__(self):
        self.previous: Union[CodeContext | None] = None
        self.indent_level = 0

    def __enter__(self):
        self.previous = CodeContext.code_context
        CodeContext.code_context = self

    def __exit__(self, *args):
        CodeContext.code_context = self.previous

    def indent(self):
        self.indent_level += 1

    def detent(self):
        self.indent_level -= 1

    def indented_statement(self, stmt):
        return IndentedStatement(stmt, self.indent_level)


class MainContext(CodeContext):
    """
    Code generation into the main() function
    """

    def add(self, expression: Union[Expression, Statement]):
        return cg.add(self.indented_statement(expression))


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
        return "\n".join(code) + "\n"

    def add(self, expression: Union[Expression, Statement]):
        return LvContext.lv_add(self.indented_statement(expression))

    def set_style(self, prop):
        return MockObj("lv_set_style_{prop}", "")


class LambdaContext(CodeContext):
    """
    A context that will accumlate code for use in a lambda.
    """

    def __init__(
        self,
        parameters: list[tuple[SafeExpType, str]] = None,
        return_type: SafeExpType = cg.void,
        capture: str = "",
        where=None,
    ):
        super().__init__()
        self.code_list: list[Statement] = []
        self.parameters = parameters
        self.return_type = return_type
        self.capture = capture
        self.where = where

    def add(self, expression: Union[Expression, Statement]):
        self.code_list.append(self.indented_statement(expression))
        return expression

    async def get_lambda(self) -> LambdaExpression:
        code_text = self.get_code()
        return await cg.process_lambda(
            Lambda("\n".join(code_text) + "\n\n"),
            self.parameters,
            capture=self.capture,
            return_type=self.return_type,
        )

    def get_code(self):
        code_text = []
        for exp in self.code_list:
            text = str(statement(exp))
            text = text.rstrip()
            code_text.append(text)
        return code_text

    def __enter__(self):
        super().__enter__()
        add_line_marks(self.where)
        return self


class LocalVariable(MockObj):
    """
    Create a local variable and enclose the code using it within a block.
    """

    def __init__(self, name, type, modifier=None, rhs=None):
        base = ID(name + "_VAR_", True, type)
        super().__init__(base, "")
        self.modifier = modifier
        self.rhs = rhs

    def __enter__(self):
        CodeContext.start_block()
        CodeContext.append(
            VariableDeclarationExpression(self.base.type, self.modifier, self.base.id)
        )
        if self.rhs is not None:
            CodeContext.append(AssignmentExpression(None, "", self.base, self.rhs))
        return MockObj(self.base)

    def __exit__(self, *args):
        CodeContext.end_block()


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


class LvConditional:
    def __init__(self, condition):
        self.condition = condition

    def __enter__(self):
        if self.condition is not None:
            CodeContext.append(RawStatement(f"if {self.condition} {{"))
            CodeContext.code_context.indent()

    def __exit__(self, *args):
        if self.condition is not None:
            CodeContext.code_context.detent()
            CodeContext.append(RawStatement("}"))

    def else_(self):
        assert self.condition is not None
        CodeContext.code_context.detent()
        CodeContext.append(RawStatement("} else {"))
        CodeContext.code_context.indent()


class ReturnStatement(ExpressionStatement):
    def __str__(self):
        return f"return {self.expression};"


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
# Operations on the LVGL component
lvgl_comp = MockObj(LVGL_COMP, "->")


# equivalent to cg.add() for the current code context
def lv_add(expression: Union[Expression, Statement]):
    return CodeContext.append(expression)


def add_line_marks(where):
    """
    Add line marks for the current code context
    :param where: An object to identify the source of the line marks
    :return:
    """
    for mark in get_line_marks(where):
        lv_add(cg.RawStatement(mark))


def lv_assign(target, expression):
    lv_add(AssignmentExpression("", "", target, expression))


def lv_Pvariable(type, name):
    """
    Create but do not initialise a pointer variable
    :param type: Type of the variable target
    :param name: name of the variable, or an ID
    :return:  A MockObj of the variable
    """
    if isinstance(name, str):
        name = ID(name, True, type)
    decl = VariableDeclarationExpression(type, "*", name)
    CORE.add_global(decl)
    var = MockObj(name, "->")
    CORE.register_variable(name, var)
    return var


def lv_variable(type, name):
    """
    Create but do not initialise a variable
    :param type: Type of the variable target
    :param name: name of the variable, or an ID
    :return:  A MockObj of the variable
    """
    if isinstance(name, str):
        name = ID(name, True, type)
    decl = VariableDeclarationExpression(type, "", name)
    CORE.add_global(decl)
    var = MockObj(name, ".")
    CORE.register_variable(name, var)
    return var
