import math

import pytest

from esphome import cpp_generator as cpp_gen
from esphome import cpp_types


class TestExpressions:
    @pytest.mark.parametrize("expression, expected", (
        (cpp_gen.RawExpression("foo && bar"), "foo && bar"),

        (cpp_gen.AssignmentExpression(None, None, "foo", "bar", None), 'foo = "bar"'),
        (cpp_gen.AssignmentExpression(cpp_types.float_, "*", "foo", 1, None), 'float *foo = 1'),
        (cpp_gen.AssignmentExpression(cpp_types.float_, "", "foo", 1, None), 'float foo = 1'),

        (cpp_gen.VariableDeclarationExpression(cpp_types.int32, "*", "foo"), "int32_t *foo"),
        (cpp_gen.VariableDeclarationExpression(cpp_types.int32, "", "foo"), "int32_t foo"),

        (cpp_gen.ParameterExpression(cpp_types.std_string, "foo"), "std::string foo"),
    ))
    def test_str__simple(self, expression: cpp_gen.Expression, expected: str):
        actual = str(expression)

        assert actual == expected


class TestLiterals:
    @pytest.mark.parametrize("literal, expected", (
        (cpp_gen.StringLiteral("foo"), '"foo"'),

        (cpp_gen.IntLiteral(0), "0"),
        (cpp_gen.IntLiteral(42), "42"),
        (cpp_gen.IntLiteral(4304967295), "4304967295ULL"),
        (cpp_gen.IntLiteral(2150483647), "2150483647UL"),
        (cpp_gen.IntLiteral(-2150083647), "-2150083647LL"),

        (cpp_gen.BoolLiteral(True), "true"),
        (cpp_gen.BoolLiteral(False), "false"),

        (cpp_gen.HexIntLiteral(0), "0x00"),
        (cpp_gen.HexIntLiteral(42), "0x2A"),
        (cpp_gen.HexIntLiteral(682), "0x2AA"),

        (cpp_gen.FloatLiteral(0.0), "0.0f"),
        (cpp_gen.FloatLiteral(4.2), "4.2f"),
        (cpp_gen.FloatLiteral(1.23456789), "1.23456789f"),
        (cpp_gen.FloatLiteral(math.nan), "NAN"),
    ))
    def test_str__simple(self, literal: cpp_gen.Literal, expected: str):
        actual = str(literal)

        assert actual == expected


class TestStatements:
    @pytest.mark.parametrize("statement, expected", (
        (cpp_gen.RawStatement("foo && bar"), "foo && bar"),

        (cpp_gen.ExpressionStatement("foo"), '"foo";'),
        (cpp_gen.ExpressionStatement(42), '42;'),

        (cpp_gen.LineComment("The point of foo is..."), "// The point of foo is..."),
        (cpp_gen.LineComment("Help help\nI'm being repressed"), "// Help help\n// I'm being repressed"),

        (
            cpp_gen.ProgmemAssignmentExpression(cpp_types.uint16, "foo", "bar", None),
            'static const uint16_t foo[] PROGMEM = "bar"'
        )
    ))
    def test_str__simple(self, statement: cpp_gen.Statement, expected: str):
        actual = str(statement)

        assert actual == expected
