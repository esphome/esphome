from collections.abc import Iterator
import io
import math
from pathlib import Path
from typing import Any

import pytest

from esphome import (
    config_validation as cv,
    cpp_generator as cg,
    cpp_types as ct,
    lambda_shorthand as ls,
)
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import (
    CORE,
    ID,
    LINE_DIRECTIVE_RESET,
    DocumentLocation,
    EsphomeError,
    Lambda,
)
from esphome.yaml_util import parse_yaml


class TestExpressions:
    @pytest.mark.parametrize(
        "target, expected",
        (
            (cg.RawExpression("foo && bar"), "foo && bar"),
            (cg.AssignmentExpression(None, None, "foo", "bar"), 'foo = "bar"'),
            (cg.AssignmentExpression(ct.float_, "*", "foo", 1), "float *foo = 1"),
            (cg.AssignmentExpression(ct.float_, "", "foo", 1), "float foo = 1"),
            (cg.VariableDeclarationExpression(ct.int32, "*", "foo"), "int32_t *foo"),
            (cg.VariableDeclarationExpression(ct.int32, "", "foo"), "int32_t foo"),
            (cg.ParameterExpression(ct.std_string, "foo"), "std::string foo"),
        ),
    )
    def test_str__simple(self, target: cg.Expression, expected: str):
        actual = str(target)

        assert actual == expected


class TestExpressionList:
    SAMPLE_ARGS = (1, "2", True, None, None)

    def test_str(self):
        target = cg.ExpressionList(*self.SAMPLE_ARGS)

        actual = str(target)

        assert actual == '1, "2", true'

    def test_iter(self):
        target = cg.ExpressionList(*self.SAMPLE_ARGS)

        actual = iter(target)

        assert isinstance(actual, Iterator)
        assert len(tuple(actual)) == 3


class TestTemplateArguments:
    SAMPLE_ARGS = (int, 1, "2", True, None, None)

    def test_str(self):
        target = cg.TemplateArguments(*self.SAMPLE_ARGS)

        actual = str(target)

        assert actual == '<int32_t, 1, "2", true>'

    def test_iter(self):
        target = cg.TemplateArguments(*self.SAMPLE_ARGS)

        actual = iter(target)

        assert isinstance(actual, Iterator)
        assert len(tuple(actual)) == 4


class TestCallExpression:
    def test_str__no_template_args(self):
        target = cg.CallExpression(cg.RawExpression("my_function"), 1, "2", False)

        actual = str(target)

        assert actual == 'my_function(1, "2", false)'

    def test_str__with_template_args(self):
        target = cg.CallExpression(
            cg.RawExpression("my_function"),
            cg.TemplateArguments(int, float),
            1,
            "2",
            False,
        )

        actual = str(target)

        assert actual == 'my_function<int32_t, float>(1, "2", false)'


class TestStaticCastExpression:
    def test_str(self):
        target = cg.StaticCastExpression(ct.bool_, 42)

        actual = str(target)

        assert actual == "static_cast<bool>(42)"


class TestStructInitializer:
    def test_str(self):
        target = cg.StructInitializer(
            cg.MockObjClass("foo::MyStruct", parents=()),
            ("state", "on"),
            ("min_length", 1),
            ("max_length", 5),
            ("foo", None),
        )

        actual = str(target)

        assert (
            actual == "foo::MyStruct{\n"
            '  .state = "on",\n'
            "  .min_length = 1,\n"
            "  .max_length = 5,\n"
            "}"
        )


class TestArrayInitializer:
    def test_str__empty(self):
        target = cg.ArrayInitializer(None, None)

        actual = str(target)

        assert actual == "{}"

    def test_str__not_multiline(self):
        target = cg.ArrayInitializer(1, 2, 3, 4)

        actual = str(target)

        assert actual == "{1, 2, 3, 4}"

    def test_str__multiline(self):
        target = cg.ArrayInitializer(1, 2, 3, 4, multiline=True)

        actual = str(target)

        assert actual == "{\n  1,\n  2,\n  3,\n  4,\n}"


class TestParameterListExpression:
    def test_str(self):
        target = cg.ParameterListExpression(
            cg.ParameterExpression(int, "foo"),
            (float, "bar"),
        )

        actual = str(target)

        assert actual == "int32_t foo, float bar"


class TestLambdaExpression:
    def test_str__no_return(self):
        target = cg.LambdaExpression(
            (
                "if ((foo == 5) && (bar < 10))) {\n",
                "}",
            ),
            ((int, "foo"), (float, "bar")),
        )

        actual = str(target)

        assert actual == (
            "[=](int32_t foo, float bar) {\n  if ((foo == 5) && (bar < 10))) {\n  }\n}"
        )

    def test_str__with_return(self):
        target = cg.LambdaExpression(
            ("return (foo == 5) && (bar < 10));",),
            cg.ParameterListExpression((int, "foo"), (float, "bar")),
            "=",
            bool,
        )

        actual = str(target)

        assert actual == (
            "[=](int32_t foo, float bar) -> bool {\n"
            "  return (foo == 5) && (bar < 10));\n"
            "}"
        )

    def test_str__stateless_no_return(self):
        """Test stateless lambda (empty capture) generates correctly"""
        target = cg.LambdaExpression(
            ('ESP_LOGD("main", "Test message");',),
            (),  # No parameters
            "",  # Empty capture (stateless)
        )

        actual = str(target)

        assert actual == ('[]() {\n  ESP_LOGD("main", "Test message");\n}')

    def test_str__stateless_with_return(self):
        """Test stateless lambda with return type generates correctly"""
        target = cg.LambdaExpression(
            ("return global_value > 0;",),
            (),  # No parameters
            "",  # Empty capture (stateless)
            bool,  # Return type
        )

        actual = str(target)

        assert actual == ("[]() -> bool {\n  return global_value > 0;\n}")

    def test_str__stateless_with_params(self):
        """Test stateless lambda with parameters generates correctly"""
        target = cg.LambdaExpression(
            ("return foo + bar;",),
            ((int, "foo"), (float, "bar")),
            "",  # Empty capture (stateless)
            float,
        )

        actual = str(target)

        assert actual == (
            "[](int32_t foo, float bar) -> float {\n  return foo + bar;\n}"
        )

    def test_str__with_capture(self):
        """Test lambda with capture generates correctly"""
        target = cg.LambdaExpression(
            ("return captured_var + x;",),
            ((int, "x"),),
            "captured_var",  # Has capture (not stateless)
            int,
        )

        actual = str(target)

        assert actual == (
            "[captured_var](int32_t x) -> int32_t {\n  return captured_var + x;\n}"
        )

    def test_str__with_source_marks_end_of_body(self) -> None:
        """A lambda from the YAML marks the end of its body for the line reset."""
        target = cg.LambdaExpression(
            ("return 42;",), (), "", int, source=DocumentLocation("test.yaml", 4, 0)
        )

        actual = str(target)

        assert actual == (
            "[]() -> int32_t {\n"
            '  #line 5 "test.yaml"\n'
            "  return 42;\n"
            f"  {LINE_DIRECTIVE_RESET}\n"
            "}"
        )


class TestCallLambda:
    """Tests for the call_lambda() function."""

    def test_call_lambda__return_expression_casts_to_return_type(self):
        """A lambda body that is just a return statement reduces to the
        expression, cast to the lambda's return type."""
        lamb = cg.LambdaExpression(("return foo + 1;",), (), "", ct.bool_)

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.StaticCastExpression)
        assert str(result) == "static_cast<bool>(foo + 1)"

    def test_call_lambda__return_with_trailing_statements_is_called(self) -> None:
        """Only a lone return statement reduces; a longer body is called as is."""
        lamb = cg.LambdaExpression(("return 1;\nfoo();",), (), "", ct.int_)

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.CallExpression)
        assert str(result).endswith("}()")

    def test_call_lambda__braced_return_is_called(self) -> None:
        """A braced return needs the lambda's return type, so it is not reduced."""
        lamb = cg.LambdaExpression(("return {};",), (), "", ct.int_)

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.CallExpression)
        assert "static_cast" not in str(result)

    def test_call_lambda__return_expression_with_class_return_type_no_cast(self):
        """A class return type is not cast, since static_cast doesn't apply
        to arbitrary class types."""
        mock_class = cg.MockObjClass("foo::Bar", parents=())
        lamb = cg.LambdaExpression(("return get_bar();",), (), "", mock_class)

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.RawExpression)
        assert str(result) == "get_bar()"

    def test_call_lambda__no_return_with_parameters_calls_with_names(self):
        """A multi-statement lambda with parameters is called with the
        parameter names as arguments."""
        lamb = cg.LambdaExpression(
            ("do_something(x, y);",), ((int, "x"), (float, "y")), "=", ct.bool_
        )

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.CallExpression)
        assert str(result) == (
            "[=](int32_t x, float y) -> bool {\n  do_something(x, y);\n}(x, y)"
        )

    def test_call_lambda__no_return_type_raises(self):
        """Calling a lambda with no declared return type is a developer
        error: call_lambda is only for value-returning lambdas."""
        lamb = cg.LambdaExpression(("do_something();",), (), "=")

        with pytest.raises(AssertionError):
            cg.call_lambda(lamb)

    def test_call_lambda__identifier_starting_with_return_is_not_a_return_statement(
        self,
    ):
        """A body that merely starts with the substring "return" (e.g. a call
        to a function named returnValue()) must not be mistaken for a return
        statement -- the match requires a word boundary after "return"."""
        lamb = cg.LambdaExpression(("returnValue();",), (), "=", ct.bool_)

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.CallExpression)
        assert str(result) == "[=]() -> bool {\n  returnValue();\n}()"

    def test_call_lambda__no_return_no_parameters_calls_with_no_args(self):
        """A multi-statement lambda without parameters is called with no
        arguments."""
        lamb = cg.LambdaExpression(("do_something();",), (), "", ct.bool_)

        result = cg.call_lambda(lamb)

        assert isinstance(result, cg.CallExpression)
        assert str(result) == "[]() -> bool {\n  do_something();\n}()"


class TestLiterals:
    @pytest.mark.parametrize(
        "target, expected",
        (
            (cg.StringLiteral("foo"), '"foo"'),
            (cg.IntLiteral(0), "0"),
            (cg.IntLiteral(42), "42"),
            (cg.IntLiteral(4304967295), "4304967295ULL"),
            (cg.IntLiteral(2150483647), "2150483647UL"),
            (cg.IntLiteral(-2150083647), "-2150083647LL"),
            (cg.BoolLiteral(True), "true"),
            (cg.BoolLiteral(False), "false"),
            (cg.HexIntLiteral(0), "0x00"),
            (cg.HexIntLiteral(42), "0x2A"),
            (cg.HexIntLiteral(682), "0x2AA"),
            (cg.FloatLiteral(0.0), "0.0f"),
            (cg.FloatLiteral(4.2), "4.2f"),
            (cg.FloatLiteral(1.23456789), "1.23456789f"),
            (cg.FloatLiteral(math.nan), "NAN"),
            (cg.FlashStringLiteral("hello"), 'ESPHOME_F("hello")'),
            (cg.FlashStringLiteral(""), 'ESPHOME_F("")'),
            (
                cg.FlashStringLiteral('quote"here'),
                'ESPHOME_F("quote\\042here")',
            ),
        ),
    )
    def test_str__simple(self, target: cg.Literal, expected: str):
        actual = str(target)

        assert actual == expected


FAKE_ENUM_VALUE = cg.EnumValue()
FAKE_ENUM_VALUE.enum_value = "foo"


@pytest.mark.parametrize(
    "obj, expected_type",
    (
        (cg.RawExpression("foo"), cg.RawExpression),
        (FAKE_ENUM_VALUE, cg.StringLiteral),
        (True, cg.BoolLiteral),
        ("foo", cg.StringLiteral),
        (cg.HexInt(42), cg.HexIntLiteral),
        (42, cg.IntLiteral),
        (42.1, cg.FloatLiteral),
        (cg.TimePeriodMicroseconds(microseconds=42), cg.IntLiteral),
        (cg.TimePeriodMilliseconds(milliseconds=42), cg.IntLiteral),
        (cg.TimePeriodSeconds(seconds=42), cg.IntLiteral),
        (cg.TimePeriodMinutes(minutes=42), cg.IntLiteral),
        ((1, 2, 3), cg.ArrayInitializer),
        ([1, 2, 3], cg.ArrayInitializer),
    ),
)
def test_safe_exp__allowed_values(obj, expected_type):
    actual = cg.safe_exp(obj)

    assert isinstance(actual, expected_type)


@pytest.mark.parametrize(
    "obj, expected_type",
    (
        (bool, ct.bool_),
        (int, ct.int32),
        (float, ct.float_),
    ),
)
def test_safe_exp__allowed_types(obj, expected_type):
    actual = cg.safe_exp(obj)

    assert actual is expected_type


@pytest.mark.parametrize(
    "obj, expected_error",
    (
        (cg.ID("foo"), "Object foo is an ID."),
        ((x for x in "foo"), r"Object <.*> is a coroutine."),
        (None, "Object is not an expression"),
    ),
)
def test_safe_exp__invalid_values(obj, expected_error):
    with pytest.raises(ValueError, match=expected_error):
        cg.safe_exp(obj)


class TestStatements:
    @pytest.mark.parametrize(
        "target, expected",
        (
            (cg.RawStatement("foo && bar"), "foo && bar"),
            (cg.ExpressionStatement("foo"), '"foo";'),
            (cg.ExpressionStatement(42), "42;"),
            (cg.LineComment("The point of foo is..."), "// The point of foo is..."),
            (
                cg.LineComment("Help help\nI'm being repressed"),
                "// Help help\n// I'm being repressed",
            ),
            (
                cg.ProgmemAssignmentExpression(ct.uint16, "foo", "bar"),
                'static constexpr uint16_t foo[] PROGMEM = "bar"',
            ),
        ),
    )
    def test_str__simple(self, target: cg.Statement, expected: str):
        actual = str(target)

        assert actual == expected


# TODO: This method has side effects in CORE
# def test_progmem_array():
#     pass


class TestMockObj:
    def test_getattr(self):
        target = cg.MockObj("foo")
        actual = target.eek
        assert isinstance(actual, cg.MockObj)
        assert actual.base == "foo.eek"
        assert actual.op == "."


class TestStatementFunction:
    """Tests for the statement() function."""

    def test_statement__expression_converted_to_statement(self):
        """Test that expressions are converted to ExpressionStatement."""
        expr = cg.RawExpression("foo()")
        result = cg.statement(expr)

        assert isinstance(result, cg.ExpressionStatement)
        assert str(result) == "foo();"

    def test_statement__statement_unchanged(self):
        """Test that statements are returned unchanged."""
        stmt = cg.RawStatement("foo()")
        result = cg.statement(stmt)

        assert result is stmt
        assert str(result) == "foo()"

    def test_statement__expression_statement_unchanged(self):
        """Test that ExpressionStatement is returned unchanged."""
        stmt = cg.ExpressionStatement(42)
        result = cg.statement(stmt)

        assert result is stmt
        assert str(result) == "42;"

    def test_statement__line_comment_unchanged(self):
        """Test that LineComment is returned unchanged."""
        stmt = cg.LineComment("This is a comment")
        result = cg.statement(stmt)

        assert result is stmt
        assert str(result) == "// This is a comment"


class TestLiteralFunction:
    """Tests for the literal() function."""

    def test_literal__creates_mockobj(self):
        """Test that literal() creates a MockObj."""
        result = cg.literal("MY_CONSTANT")

        assert isinstance(result, cg.MockObj)
        assert result.base == "MY_CONSTANT"
        assert result.op == ""

    def test_literal__string_representation(self):
        """Test that literal names appear unquoted in generated code."""
        result = cg.literal("nullptr")

        assert str(result) == "nullptr"

    def test_literal__can_be_used_in_expressions(self):
        """Test that literals can be used as part of larger expressions."""
        null_lit = cg.literal("nullptr")
        expr = cg.CallExpression(cg.RawExpression("my_func"), null_lit)

        assert str(expr) == "my_func(nullptr)"

    def test_literal__common_cpp_literals(self):
        """Test common C++ literal values."""
        test_cases = [
            ("nullptr", "nullptr"),
            ("true", "true"),
            ("false", "false"),
            ("NULL", "NULL"),
            ("NAN", "NAN"),
        ]

        for name, expected in test_cases:
            result = cg.literal(name)
            assert str(result) == expected


class TestLambdaConstructor:
    """Tests for the Lambda class constructor in core/__init__.py."""

    def test_lambda__from_string(self):
        """Test Lambda constructor with string argument."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x + 1;")

        assert lambda_obj.value == "return x + 1;"
        assert str(lambda_obj) == "return x + 1;"

    def test_lambda__from_expression(self):
        """Test Lambda constructor with Expression argument."""
        from esphome.core import Lambda

        expr = cg.RawExpression("x + 1")
        lambda_obj = Lambda(expr)

        # Expression should be converted to statement (with semicolon)
        assert lambda_obj.value == "x + 1;"

    def test_lambda__from_lambda(self):
        """Test Lambda constructor with another Lambda argument."""
        from esphome.core import Lambda

        original = Lambda("return x + 1;")
        copy = Lambda(original)

        assert copy.value == original.value
        assert copy.value == "return x + 1;"

    def test_lambda__parts_parsing(self):
        """Test that Lambda correctly parses parts with id() references."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return id(my_sensor).state;")
        parts = lambda_obj.parts

        # Parts should be split by LAMBDA_PROG regex: text, id, op, text
        assert len(parts) == 4
        assert parts[0] == "return "
        assert parts[1] == "my_sensor"
        assert parts[2] == "."
        assert parts[3] == "state;"

    def test_lambda__requires_ids(self):
        """Test that Lambda correctly extracts required IDs."""
        from esphome.core import ID, Lambda

        lambda_obj = Lambda("return id(sensor1).state + id(sensor2).value;")
        ids = lambda_obj.requires_ids

        assert len(ids) == 2
        assert all(isinstance(id_obj, ID) for id_obj in ids)
        assert ids[0].id == "sensor1"
        assert ids[1].id == "sensor2"

    def test_lambda__no_ids(self):
        """Test Lambda with no id() references."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return 42;")
        ids = lambda_obj.requires_ids

        assert len(ids) == 0

    def test_lambda__comment_removal(self):
        """Test that comments are removed when parsing parts."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return id(sensor).state; // Get sensor state")
        parts = lambda_obj.parts

        # Comment should be replaced with space, not affect parsing
        assert "my_sensor" not in str(parts)

    def test_lambda__multiline_string(self):
        """Test Lambda with multiline string."""
        from esphome.core import Lambda

        code = """if (id(sensor).state > 0) {
  return true;
}
return false;"""
        lambda_obj = Lambda(code)

        assert lambda_obj.value == code
        assert "sensor" in [id_obj.id for id_obj in lambda_obj.requires_ids]


@pytest.mark.asyncio
class TestProcessLambda:
    """Tests for the process_lambda() async function."""

    async def test_process_lambda__none_value(self):
        """Test that None returns None."""
        result = await cg.process_lambda(None, [])

        assert result is None

    async def test_process_lambda__with_expression(self):
        """Test process_lambda with Expression argument."""

        expr = cg.RawExpression("return x + 1")
        result = await cg.process_lambda(expr, [(int, "x")])

        assert isinstance(result, cg.LambdaExpression)
        assert "x + 1" in str(result)

    async def test_process_lambda__simple_lambda_no_ids(self):
        """Test process_lambda with simple Lambda without id() references."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x + 1;")
        result = await cg.process_lambda(lambda_obj, [(int, "x")])

        assert isinstance(result, cg.LambdaExpression)
        # Should have parameter
        lambda_str = str(result)
        assert "int32_t x" in lambda_str
        assert "return x + 1;" in lambda_str

    async def test_process_lambda__with_return_type(self):
        """Test process_lambda with return type specified."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x > 0;")
        result = await cg.process_lambda(lambda_obj, [(int, "x")], return_type=bool)

        assert isinstance(result, cg.LambdaExpression)
        lambda_str = str(result)
        assert "-> bool" in lambda_str

    async def test_process_lambda__with_capture(self):
        """Test process_lambda with capture specified."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return captured + x;")
        result = await cg.process_lambda(lambda_obj, [(int, "x")], capture="captured")

        assert isinstance(result, cg.LambdaExpression)
        lambda_str = str(result)
        assert "[captured]" in lambda_str

    async def test_process_lambda__empty_capture(self):
        """Test process_lambda with empty capture (stateless lambda)."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x + 1;")
        result = await cg.process_lambda(lambda_obj, [(int, "x")], capture="")

        assert isinstance(result, cg.LambdaExpression)
        lambda_str = str(result)
        assert "[]" in lambda_str

    async def test_process_lambda__no_parameters(self):
        """Test process_lambda with no parameters."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return 42;")
        result = await cg.process_lambda(lambda_obj, [])

        assert isinstance(result, cg.LambdaExpression)
        lambda_str = str(result)
        # Should have empty parameter list
        assert "()" in lambda_str

    async def test_process_lambda__multiple_parameters(self):
        """Test process_lambda with multiple parameters."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x + y + z;")
        result = await cg.process_lambda(
            lambda_obj, [(int, "x"), (float, "y"), (bool, "z")]
        )

        assert isinstance(result, cg.LambdaExpression)
        lambda_str = str(result)
        assert "int32_t x" in lambda_str
        assert "float y" in lambda_str
        assert "bool z" in lambda_str

    async def test_process_lambda__parameter_validation(self):
        """Test that malformed parameters raise assertion error."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")

        # Test invalid parameter format (not list of tuples)
        with pytest.raises(AssertionError):
            await cg.process_lambda(lambda_obj, "invalid")

        # Test invalid tuple format (not 2-element tuples)
        with pytest.raises(AssertionError):
            await cg.process_lambda(lambda_obj, [(int, "x", "extra")])

        # Test invalid tuple format (single element)
        with pytest.raises(AssertionError):
            await cg.process_lambda(lambda_obj, [(int,)])

    def test_str__with_line_mark_in_body_marks_end_of_body(self) -> None:
        """A #line written into the body, as lvgl does, gets the reset too."""
        target = cg.LambdaExpression(('#line 3 "test.yaml"\nfoo();',), (), "", None)

        actual = str(target)

        assert actual == (
            f'[]() {{\n  #line 3 "test.yaml"\n  foo();\n  {LINE_DIRECTIVE_RESET}\n}}'
        )

    async def test_process_lambda__twice_keeps_line(self) -> None:
        """Processing one lambda twice, as a YAML alias does, keeps its line."""
        config = parse_yaml(Path("test.yaml"), io.StringIO("x: |-\n  return 42;\n"))
        lambda_obj = cv.lambda_(config["x"])

        first = await cg.process_lambda(lambda_obj, [])
        second = await cg.process_lambda(lambda_obj, [])

        # The body of the block scalar is the second line, 1 counted from 0
        assert first.source.line == 1
        assert second.source.line == 1

    async def test_process_lambda__argument_shorthand_matches_parameter(self) -> None:
        """`argument: x` succeeds when `x` is one of the available parameters."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        result = await cg.process_lambda(lambda_obj, [(float, "x")])

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__argument_shorthand_unknown_name_raises(self) -> None:
        """`argument: y` must fail when the call site only offers `x` -- this would
        otherwise only surface as a C++ 'y' was not declared in this scope error."""
        from esphome.core import EsphomeError, Lambda

        lambda_obj = Lambda("return y;")
        lambda_obj.argument_name = "y"
        with pytest.raises(
            EsphomeError, match="does not match any available parameter"
        ):
            await cg.process_lambda(lambda_obj, [(float, "x")])

    async def test_process_lambda__argument_shorthand_error_carries_location(
        self,
    ) -> None:
        """The `argument:` shorthand attaches the YAML source location (see
        `_lambda_at_source` in config_validation.py) for exactly this purpose --
        `_check_argument_shorthand` must surface it in the error rather than leaving
        the user to grep for `argument: y`."""
        from esphome.core import DocumentLocation, DocumentRange, EsphomeError, Lambda
        from esphome.yaml_util import make_data_base

        source = make_data_base("dummy")
        source._esp_range = DocumentRange(
            DocumentLocation(document="test.yaml", line=3, column=5),
            DocumentLocation(document="test.yaml", line=3, column=20),
        )
        lambda_obj = make_data_base(Lambda("return y;"), from_database=source)
        lambda_obj.argument_name = "y"

        with pytest.raises(EsphomeError, match=r"at test\.yaml 3:5"):
            await cg.process_lambda(lambda_obj, [(float, "x")])

    async def test_process_lambda__argument_shorthand_no_parameters_raises(
        self,
    ) -> None:
        """`argument: x` must fail when the call site offers no parameters at all."""
        from esphome.core import EsphomeError, Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        with pytest.raises(EsphomeError, match="none available here"):
            await cg.process_lambda(lambda_obj, [])

    async def test_process_lambda__argument_shorthand_incompatible_type_raises(
        self,
    ) -> None:
        """A std::string parameter fed into a float-returning field would fail to
        compile; `argument:` should catch that at config-generation time instead."""
        from esphome.core import EsphomeError, Lambda
        from esphome.cpp_types import std_string

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        with pytest.raises(EsphomeError, match="not compatible"):
            await cg.process_lambda(lambda_obj, [(std_string, "x")], return_type=float)

    async def test_process_lambda__argument_shorthand_bool_to_numeric_allowed(
        self,
    ) -> None:
        """A bool parameter and a numeric field are mutually compatible in C++, so this
        must not raise even though the kinds differ."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        result = await cg.process_lambda(lambda_obj, [(bool, "x")], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__non_shorthand_lambda_unaffected(self) -> None:
        """A hand-written lambda (argument_name unset) is never subject to this check,
        even when its source text happens to reference a name absent from parameters."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return not_a_parameter + 1;")
        result = await cg.process_lambda(lambda_obj, [(float, "x")])

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__argument_shorthand_exact_kind_match_allowed(
        self,
    ) -> None:
        """A parameter and return type of the identical kind (both numeric here) must
        not raise -- covers the `arg_kind == return_kind` short-circuit."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        result = await cg.process_lambda(lambda_obj, [(float, "x")], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    @staticmethod
    def _entity_state_lambda(entity_name: str, declared_type: Any) -> Lambda:
        """Build a Lambda shaped like `convert_lambda_shorthand`'s `entity_state:`
        output -- `return id(<entity_name>).state;` with a searching id typed as
        every known state-bearing type -- and register `entity_name` as a declared
        id of `declared_type` so `process_lambda`'s id resolution finds it."""
        from esphome.core import CORE, ID, Lambda
        from esphome.lambda_shorthand import state_bearing_types

        declared = ID(entity_name, is_declaration=True, type=declared_type)
        CORE.register_variable(declared, cg.MockObj(entity_name))
        lambda_obj = Lambda(f"return id({entity_name}).state;")
        types = tuple(t for t, _ in state_bearing_types())
        lambda_obj.set_requires_ids([ID(entity_name, is_declaration=False, type=types)])
        return lambda_obj

    async def test_process_lambda__entity_state_shorthand_incompatible_kind_raises(
        self,
    ) -> None:
        """A TextSensor's std::string state fed into a float-returning field would
        fail to compile; the codegen-time check should catch it instead -- this is
        exactly the gap validator-identity narrowing left for composed validators."""
        from esphome.components.text_sensor import TextSensor
        from esphome.core import EsphomeError

        lambda_obj = self._entity_state_lambda("my_text_sensor", TextSensor)

        with pytest.raises(EsphomeError, match="not compatible"):
            await cg.process_lambda(lambda_obj, [], return_type=float)

    async def test_process_lambda__entity_state_shorthand_string_to_c_string_raises(
        self,
    ) -> None:
        """A std::string state does not convert to a `const char *` field."""
        from esphome.components.text import Text
        from esphome.core import EsphomeError

        lambda_obj = self._entity_state_lambda("my_text", Text)

        with pytest.raises(EsphomeError, match="not compatible"):
            await cg.process_lambda(lambda_obj, [], return_type=ct.const_char_ptr)

    async def test_process_lambda__entity_state_shorthand_enum_state_unchecked(
        self,
    ) -> None:
        """A lock's enum state has no kind, so it is not compared with the field."""
        from esphome.components.lock import Lock

        lambda_obj = self._entity_state_lambda("my_lock", Lock)
        result = await cg.process_lambda(lambda_obj, [], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__entity_state_shorthand_compatible_kind_allowed(
        self,
    ) -> None:
        """A Sensor's numeric state fed into a float-returning field is fine."""
        from esphome.components.sensor import Sensor

        lambda_obj = self._entity_state_lambda("my_sensor", Sensor)
        result = await cg.process_lambda(lambda_obj, [], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__entity_state_shorthand_bool_to_numeric_allowed(
        self,
    ) -> None:
        """A BinarySensor's bool state and a numeric field are mutually compatible in
        C++, so this must not raise even though the kinds differ."""
        from esphome.components.binary_sensor import BinarySensor

        lambda_obj = self._entity_state_lambda("my_binary_sensor", BinarySensor)
        result = await cg.process_lambda(lambda_obj, [], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__entity_state_shorthand_no_return_type_unchecked(
        self,
    ) -> None:
        """No return_type means no field type to compare against -- matches
        `argument:`'s equivalent leniency for call sites that don't pass one."""
        from esphome.components.text_sensor import TextSensor

        lambda_obj = self._entity_state_lambda("my_text_sensor", TextSensor)
        result = await cg.process_lambda(lambda_obj, [])

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__entity_state_shorthand_untyped_resolved_id_unaffected(
        self,
    ) -> None:
        """`full_id.type` not being a `MockObjClass` (e.g. an id declared with no
        type at all) leaves nothing to classify -- must not raise."""
        lambda_obj = self._entity_state_lambda("untyped_id", None)
        result = await cg.process_lambda(lambda_obj, [], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__entity_state_shorthand_non_state_bearing_type_unaffected(
        self,
    ) -> None:
        """A resolved type outside the known state-bearing set -- in normal operation
        the id-resolution pass would already have rejected this before codegen runs,
        so this only matters when process_lambda is exercised directly -- is left
        unchecked here too, since there's no kind to compare against."""
        from esphome.components.climate import Climate

        lambda_obj = self._entity_state_lambda("my_climate", Climate)
        result = await cg.process_lambda(lambda_obj, [], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__hand_written_id_reference_unaffected(self) -> None:
        """A hand-written `return id(x).state;` (no `entity_state:` shorthand behind
        it) parses to an untyped id -- `id.type` is `None`, not a tuple -- so this
        check must not fire for it, even with an incompatible return_type."""
        from esphome.components.text_sensor import TextSensor
        from esphome.core import ID, Lambda

        declared = ID("my_text_sensor", is_declaration=True, type=TextSensor)
        cg.CORE.register_variable(declared, cg.MockObj("my_text_sensor"))
        lambda_obj = Lambda("return id(my_text_sensor).state;")

        result = await cg.process_lambda(lambda_obj, [], return_type=float)

        assert isinstance(result, cg.LambdaExpression)


class TestArgKind:
    """Direct tests for lambda_shorthand.arg_kind()/kinds_compatible(), the coarse
    type classification `argument:` and `entity_state:` use to sanity-check a value
    against a field's return type without policing every stylistic mismatch."""

    @pytest.mark.parametrize(
        "type_, expected",
        [
            (bool, ls.ArgKind.BOOLEAN),
            (float, ls.ArgKind.NUMERIC),
            (int, ls.ArgKind.NUMERIC),
            (str, ls.ArgKind.STRING),
            (ct.float_, ls.ArgKind.NUMERIC),
            (ct.int_, ls.ArgKind.NUMERIC),
            (ct.uint32, ls.ArgKind.NUMERIC),
            (ct.bool_, ls.ArgKind.BOOLEAN),
            (ct.std_string, ls.ArgKind.STRING),
            (ct.const_char_ptr, ls.ArgKind.C_STRING),
        ],
    )
    def test_arg_kind__recognized_types(self, type_: Any, expected: ls.ArgKind) -> None:
        assert ls.arg_kind(type_) == expected

    def test_arg_kind__unrecognized_type_returns_none(self) -> None:
        """A custom/enum MockObjClass isn't in the recognized set -- returns None so
        the caller stays lenient rather than guessing wrong."""
        custom_type = cg.MockObjClass("my_component::MyEnum", parents=())

        assert ls.arg_kind(custom_type) is None

    @pytest.mark.parametrize(
        "source, target, expected",
        [
            (ls.ArgKind.NUMERIC, ls.ArgKind.NUMERIC, True),
            (ls.ArgKind.STRING, ls.ArgKind.STRING, True),
            (ls.ArgKind.BOOLEAN, ls.ArgKind.NUMERIC, True),
            (ls.ArgKind.NUMERIC, ls.ArgKind.BOOLEAN, True),
            (ls.ArgKind.STRING, ls.ArgKind.NUMERIC, False),
            (ls.ArgKind.STRING, ls.ArgKind.BOOLEAN, False),
            (ls.ArgKind.C_STRING, ls.ArgKind.STRING, True),
            (ls.ArgKind.STRING, ls.ArgKind.C_STRING, False),
            (ls.ArgKind.C_STRING, ls.ArgKind.NUMERIC, False),
        ],
    )
    def test_arg_kinds_compatible(
        self, source: ls.ArgKind, target: ls.ArgKind, expected: bool
    ) -> None:
        assert ls.kinds_compatible(source, target) is expected


@pytest.mark.asyncio
async def test_templatable__string_with_std_string_returns_flash_literal() -> None:
    """Static string with std::string output_type returns FlashStringLiteral."""
    result = await cg.templatable("hello", [], ct.std_string)

    assert isinstance(result, cg.FlashStringLiteral)
    assert str(result) == 'ESPHOME_F("hello")'


@pytest.mark.asyncio
async def test_templatable__empty_string_with_std_string() -> None:
    """Empty static string with std::string output_type returns FlashStringLiteral."""
    result = await cg.templatable("", [], ct.std_string)

    assert isinstance(result, cg.FlashStringLiteral)
    assert str(result) == 'ESPHOME_F("")'


@pytest.mark.asyncio
async def test_templatable__string_with_none_output_type() -> None:
    """Static string with output_type=None returns raw string (no wrapping)."""
    result = await cg.templatable("hello", [], None)

    assert isinstance(result, str)
    assert result == "hello"


@pytest.mark.asyncio
async def test_templatable__int_with_std_string() -> None:
    """Non-string value with std::string output_type returns raw value."""
    result = await cg.templatable(42, [], ct.std_string)

    assert result == 42


@pytest.mark.asyncio
async def test_templatable__string_with_non_string_output_type() -> None:
    """Static string with non-std::string output_type returns stateless lambda."""
    result = await cg.templatable("hello", [], ct.bool_)

    assert isinstance(result, cg.LambdaExpression)
    assert result.capture == ""


@pytest.mark.asyncio
async def test_templatable__with_to_exp_callable() -> None:
    """When to_exp is provided, it is applied to non-template values."""
    result = await cg.templatable(42, [], None, to_exp=lambda x: x * 2)

    assert result == 84


@pytest.mark.asyncio
async def test_templatable__with_to_exp_callable_and_output_type() -> None:
    """When to_exp is provided with non-string output_type, result is lambda-wrapped."""
    result = await cg.templatable(42, [], ct.int_, to_exp=lambda x: x * 2)

    assert isinstance(result, cg.LambdaExpression)
    assert result.capture == ""


@pytest.mark.asyncio
async def test_templatable__with_to_exp_dict() -> None:
    """When to_exp is a dict, value is looked up."""
    mapping: dict[str, int] = {"on": 1, "off": 0}
    result = await cg.templatable("on", [], None, to_exp=mapping)

    assert result == 1


@pytest.mark.asyncio
async def test_templatable__lambda_with_std_string() -> None:
    """Lambda value returns LambdaExpression, not FlashStringLiteral."""
    from esphome.core import Lambda

    lambda_obj = Lambda('return "hello";')
    result = await cg.templatable(lambda_obj, [], ct.std_string)

    assert isinstance(result, cg.LambdaExpression)


class TestSharedProgmemArray:
    def test_identical_contents_share_one_array(self) -> None:
        CORE.config = {}
        a = cg.shared_progmem_array("table", ct.uint8, [1, 2, 3])
        b = cg.shared_progmem_array("table", ct.uint8, [1, 2, 3])
        c = cg.shared_progmem_array("table", ct.uint8, [4])
        assert a is b
        assert str(a) != str(c)
        late = CORE.global_statements_after_includes
        assert sum("PROGMEM" in str(st) for st in late) == 2
        assert not any("PROGMEM" in str(st) for st in CORE.main_statements)

    def test_share_false_always_emits_a_new_array(self) -> None:
        CORE.config = {}
        a = cg.shared_progmem_array("table", ct.uint8, [1], share=False)
        b = cg.shared_progmem_array("table", ct.uint8, [1], share=False)
        assert str(a) != str(b)
        late = CORE.global_statements_after_includes
        assert sum("PROGMEM" in str(st) for st in late) == 2

    def test_is_static_pointer_only_for_placement_new(self) -> None:
        CORE.config = {}
        static = cg.new_Pvariable(ID("static_obj", is_declaration=True, type=ct.uint8))
        dynamic = cg.Pvariable(
            ID("dynamic_obj", is_declaration=True, type=ct.uint8),
            cg.RawExpression("nullptr"),
        )
        assert cg.is_static_pointer(static)
        assert not cg.is_static_pointer(dynamic)

    def test_constexpr_and_const_arrays_are_not_shared(self) -> None:
        CORE.config = {}
        a = cg.shared_progmem_array("table", ct.uint8, [1], constexpr=False)
        b = cg.shared_progmem_array("table", ct.uint8, [1])
        assert str(a) != str(b)

    def test_constexpr_false_rejects_dynamic_pointers(self) -> None:
        CORE.config = {}
        dynamic = cg.Pvariable(
            ID("dynamic_obj", is_declaration=True, type=ct.uint8),
            cg.RawExpression("nullptr"),
        )
        with pytest.raises(EsphomeError, match="dynamic_obj"):
            cg.shared_progmem_array(
                "table",
                ct.uint8.operator("ptr"),
                cg.ArrayInitializer(dynamic),
                constexpr=False,
            )

    def test_constexpr_false_emits_a_const_array(self) -> None:
        CORE.config = {}
        cg.shared_progmem_array("table", ct.uint8, [1], constexpr=False)
        (statement,) = (
            str(st)
            for st in CORE.global_statements_after_includes
            if "PROGMEM" in str(st)
        )
        assert statement.startswith(
            "ESPHOME_FLASH_CONSTINIT static uint8_t const table[] PROGMEM = {1}"
        )

    def test_same_contents_different_type_are_separate(self) -> None:
        CORE.config = {}
        a = cg.shared_progmem_array("table", ct.uint8, [1])
        b = cg.shared_progmem_array("table", ct.uint16, [1])
        assert str(a) != str(b)

    def test_name_avoids_config_ids(self) -> None:
        CORE.config = {"sensor": [{"id": ID("table", is_declaration=True)}]}
        array = cg.shared_progmem_array("table", ct.uint8, [1])
        assert str(array) == "table_2"

    def test_name_avoids_registered_variables(self) -> None:
        CORE.config = {}
        CORE.register_variable(ID("table", is_declaration=True), cg.MockObj("table"))
        array = cg.shared_progmem_array("table", ct.uint8, [1])
        assert str(array) == "table_2"


class TestProgmemString:
    @staticmethod
    def _set_platform(platform: str) -> None:
        CORE.config = {}
        CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}

    def test_esp8266_shares_one_array_per_text(self) -> None:
        self._set_platform(PLATFORM_ESP8266)
        a = cg.shared_progmem_string("level")
        b = cg.shared_progmem_string("level")
        c = cg.shared_progmem_string('say "hi"')
        assert a is b
        assert str(a) == "progmem_str"
        assert str(c) == "progmem_str_2"
        assert [str(st) for st in CORE.global_statements_after_includes] == [
            'static constexpr char progmem_str[] PROGMEM = "level";',
            'static constexpr char progmem_str_2[] PROGMEM = "say \\042hi\\042";',
        ]

    def test_other_platforms_return_the_literal(self) -> None:
        self._set_platform(PLATFORM_ESP32)
        assert str(cg.shared_progmem_string('say "hi"')) == '"say \\042hi\\042"'
        assert not CORE.global_statements
        assert not CORE.global_statements_after_includes


def test_extern_progmem_array_is_an_externally_linked_global() -> None:
    array = cg.extern_progmem_array("esphome::api::API_TABLE", ct.uint8, [1, 2])
    assert str(array) == "esphome::api::API_TABLE"
    assert [str(st) for st in CORE.global_statements if "PROGMEM" in str(st)] == [
        "const uint8_t esphome::api::API_TABLE[] PROGMEM = {1, 2};"
    ]


def test_static_function_declares_early_and_defines_after_includes() -> None:
    fn = cg.static_function("esphome__act__fn", ct.void, [(ct.int32, "x")], ["return;"])
    assert str(fn) == "esphome__act__fn"
    assert [str(st) for st in CORE.global_statements] == [
        "static void esphome__act__fn(int32_t x);"
    ]
    assert [str(st) for st in CORE.global_statements_after_includes] == [
        "static void esphome__act__fn(int32_t x) {\n  return;\n}"
    ]


def test_add_global_after_includes_rejects_non_statements() -> None:
    with pytest.raises(ValueError, match="must be expression or statement"):
        CORE.add_global_after_includes("not a statement")


class TestEnumOptions:
    def test_class_enum_with_prefix(self) -> None:
        mode = cg.MockObj("esphome::foo", "::").enum("Mode", is_class=True)
        options = mode.options("fast", "slow-ish", "2.5x", prefix="MODE_")
        assert list(options) == ["fast", "slow-ish", "2.5x"]
        assert {k: str(v) for k, v in options.items()} == {
            "fast": "esphome::foo::Mode::MODE_FAST",
            "slow-ish": "esphome::foo::Mode::MODE_SLOW_ISH",
            "2.5x": "esphome::foo::Mode::MODE_2_5X",
        }

    def test_matches_attribute_access(self) -> None:
        mode = cg.MockObj("esphome::foo", "::").enum("Mode")
        options = mode.options("LOW", "HIGH")
        assert str(options["LOW"]) == str(mode.LOW) == "esphome::foo::LOW"
        assert str(options["HIGH"]) == str(mode.HIGH)

    def test_result_works_with_cv_enum(self) -> None:
        mode = cg.MockObj("esphome::foo", "::").enum("Mode", is_class=True)
        options = mode.options("on", "off", prefix="MODE_")
        validator = cv.enum(options, lower=True)
        assert str(validator("ON").enum_value) == "esphome::foo::Mode::MODE_ON"
        assert validator(cv.SCHEMA_EXTRACT) is options

    @pytest.mark.parametrize(
        "names",
        [("OFF", "OFF"), ("slow-ish", "slow_ish"), ("2.5x", "2_5X")],
    )
    def test_names_must_map_to_distinct_enumerators(
        self, names: tuple[str, ...]
    ) -> None:
        mode = cg.MockObj("esphome::foo", "::").enum("Mode")
        with pytest.raises(ValueError, match="distinct enumerators"):
            mode.options(*names)
