from collections.abc import Iterator

import math

import pytest

from esphome import cpp_generator as cg
from esphome import cpp_types as ct


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
            "[=](int32_t foo, float bar) {\n"
            "  if ((foo == 5) && (bar < 10))) {\n"
            "  }\n"
            "}"
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
                'static const uint16_t foo[] PROGMEM = "bar"',
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
