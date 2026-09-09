from collections.abc import Iterator
import math

import pytest

from esphome import cpp_generator as cg, cpp_types as ct


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

    async def test_process_lambda__argument_shorthand_matches_parameter(self):
        """`argument: x` succeeds when `x` is one of the available parameters."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        result = await cg.process_lambda(lambda_obj, [(float, "x")])

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__argument_shorthand_unknown_name_raises(self):
        """`argument: y` must fail when the call site only offers `x` -- this would
        otherwise only surface as a C++ 'y' was not declared in this scope error."""
        from esphome.core import EsphomeError, Lambda

        lambda_obj = Lambda("return y;")
        lambda_obj.argument_name = "y"
        with pytest.raises(
            EsphomeError, match="does not match any available parameter"
        ):
            await cg.process_lambda(lambda_obj, [(float, "x")])

    async def test_process_lambda__argument_shorthand_error_carries_location(self):
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

    async def test_process_lambda__argument_shorthand_no_parameters_raises(self):
        """`argument: x` must fail when the call site offers no parameters at all."""
        from esphome.core import EsphomeError, Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        with pytest.raises(EsphomeError, match="none available here"):
            await cg.process_lambda(lambda_obj, [])

    async def test_process_lambda__argument_shorthand_incompatible_type_raises(self):
        """A std::string parameter fed into a float-returning field would fail to
        compile; `argument:` should catch that at config-generation time instead."""
        from esphome.core import EsphomeError, Lambda
        from esphome.cpp_types import std_string

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        with pytest.raises(EsphomeError, match="not compatible"):
            await cg.process_lambda(lambda_obj, [(std_string, "x")], return_type=float)

    async def test_process_lambda__argument_shorthand_bool_to_numeric_allowed(self):
        """A bool parameter and a numeric field are mutually compatible in C++, so this
        must not raise even though the kinds differ."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        result = await cg.process_lambda(lambda_obj, [(bool, "x")], return_type=float)

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__non_shorthand_lambda_unaffected(self):
        """A hand-written lambda (argument_name unset) is never subject to this check,
        even when its source text happens to reference a name absent from parameters."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return not_a_parameter + 1;")
        result = await cg.process_lambda(lambda_obj, [(float, "x")])

        assert isinstance(result, cg.LambdaExpression)

    async def test_process_lambda__argument_shorthand_exact_kind_match_allowed(self):
        """A parameter and return type of the identical kind (both numeric here) must
        not raise -- covers the `arg_kind == return_kind` short-circuit."""
        from esphome.core import Lambda

        lambda_obj = Lambda("return x;")
        lambda_obj.argument_name = "x"
        result = await cg.process_lambda(lambda_obj, [(float, "x")], return_type=float)

        assert isinstance(result, cg.LambdaExpression)


class TestArgKind:
    """Direct tests for _arg_kind()/_arg_kinds_compatible(), the coarse type
    classification `argument:` uses to sanity-check a parameter against a field's
    return type without policing every stylistic mismatch."""

    @pytest.mark.parametrize(
        "type_, expected",
        [
            (bool, cg._ARG_KIND_BOOLEAN),
            (float, cg._ARG_KIND_NUMERIC),
            (int, cg._ARG_KIND_NUMERIC),
            (str, cg._ARG_KIND_STRING),
            (ct.float_, cg._ARG_KIND_NUMERIC),
            (ct.int_, cg._ARG_KIND_NUMERIC),
            (ct.uint32, cg._ARG_KIND_NUMERIC),
            (ct.bool_, cg._ARG_KIND_BOOLEAN),
            (ct.std_string, cg._ARG_KIND_STRING),
            (ct.const_char_ptr, cg._ARG_KIND_STRING),
        ],
    )
    def test_arg_kind__recognized_types(self, type_, expected):
        assert cg._arg_kind(type_) == expected

    def test_arg_kind__unrecognized_type_returns_none(self):
        """A custom/enum MockObjClass isn't in the recognized set -- returns None so
        the caller stays lenient rather than guessing wrong."""
        custom_type = cg.MockObjClass("my_component::MyEnum", parents=())

        assert cg._arg_kind(custom_type) is None

    @pytest.mark.parametrize(
        "arg_kind, return_kind, expected",
        [
            (cg._ARG_KIND_NUMERIC, cg._ARG_KIND_NUMERIC, True),
            (cg._ARG_KIND_STRING, cg._ARG_KIND_STRING, True),
            (cg._ARG_KIND_BOOLEAN, cg._ARG_KIND_NUMERIC, True),
            (cg._ARG_KIND_NUMERIC, cg._ARG_KIND_BOOLEAN, True),
            (cg._ARG_KIND_STRING, cg._ARG_KIND_NUMERIC, False),
            (cg._ARG_KIND_STRING, cg._ARG_KIND_BOOLEAN, False),
        ],
    )
    def test_arg_kinds_compatible(self, arg_kind, return_kind, expected):
        assert cg._arg_kinds_compatible(arg_kind, return_kind) is expected


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
