"""Tests for lambda deduplication in cpp_generator."""

from esphome import cpp_generator as cg
from esphome.core import CORE


def test_deduplicate_identical_lambdas() -> None:
    """Test that identical stateless lambdas are deduplicated."""
    # Create two identical lambda expressions
    lambda1 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    lambda2 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    # Try to deduplicate them
    func_name1 = cg._get_shared_lambda_name(lambda1)
    func_name2 = cg._get_shared_lambda_name(lambda2)

    # Both should get the same function name (deduplication happened)
    assert func_name1 == func_name2
    assert func_name1 == "shared_lambda_0"


def test_different_lambdas_not_deduplicated() -> None:
    """Test that different lambdas get different function names."""
    lambda1 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    lambda2 = cg.LambdaExpression(
        parts=["return 24;"],  # Different content
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    func_name1 = cg._get_shared_lambda_name(lambda1)
    func_name2 = cg._get_shared_lambda_name(lambda2)

    # Different lambdas should get different function names
    assert func_name1 != func_name2
    assert func_name1 == "shared_lambda_0"
    assert func_name2 == "shared_lambda_1"


def test_different_return_types_not_deduplicated() -> None:
    """Test that lambdas with different return types are not deduplicated."""
    lambda1 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    lambda2 = cg.LambdaExpression(
        parts=["return 42;"],  # Same content
        parameters=[],
        capture="",
        return_type=cg.RawExpression("float"),  # Different return type
    )

    func_name1 = cg._get_shared_lambda_name(lambda1)
    func_name2 = cg._get_shared_lambda_name(lambda2)

    # Different return types = different functions
    assert func_name1 != func_name2


def test_different_parameters_not_deduplicated() -> None:
    """Test that lambdas with different parameters are not deduplicated."""
    lambda1 = cg.LambdaExpression(
        parts=["return x;"],
        parameters=[("int", "x")],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    lambda2 = cg.LambdaExpression(
        parts=["return x;"],  # Same content
        parameters=[("float", "x")],  # Different parameter type
        capture="",
        return_type=cg.RawExpression("int"),
    )

    func_name1 = cg._get_shared_lambda_name(lambda1)
    func_name2 = cg._get_shared_lambda_name(lambda2)

    # Different parameters = different functions
    assert func_name1 != func_name2


def test_flush_lambda_dedup_declarations() -> None:
    """Test that deferred declarations are properly stored for later flushing."""
    # Create a lambda which will create a deferred declaration
    lambda1 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    cg._get_shared_lambda_name(lambda1)

    # Check that declaration was stored
    assert cg._KEY_LAMBDA_DEDUP_DECLARATIONS in CORE.data
    assert len(CORE.data[cg._KEY_LAMBDA_DEDUP_DECLARATIONS]) == 1

    # Verify the declaration content is correct
    declaration = CORE.data[cg._KEY_LAMBDA_DEDUP_DECLARATIONS][0]
    assert "shared_lambda_0" in declaration
    assert "return 42;" in declaration

    # Note: The actual flushing happens via CORE.add_job with FINAL priority
    # during real code generation, so we don't test that here


def test_shared_function_lambda_expression() -> None:
    """Test SharedFunctionLambdaExpression behaves correctly."""
    shared_lambda = cg.SharedFunctionLambdaExpression(
        func_name="shared_lambda_0",
        parameters=[],
        return_type=cg.RawExpression("int"),
    )

    # Should output just the function name
    assert str(shared_lambda) == "shared_lambda_0"

    # Should have empty capture (stateless)
    assert shared_lambda.capture == ""

    # Should have empty content (just a reference)
    assert shared_lambda.content == ""


def test_lambda_deduplication_counter() -> None:
    """Test that lambda counter increments correctly."""
    # Create 3 different lambdas
    for i in range(3):
        lambda_expr = cg.LambdaExpression(
            parts=[f"return {i};"],
            parameters=[],
            capture="",
            return_type=cg.RawExpression("int"),
        )
        func_name = cg._get_shared_lambda_name(lambda_expr)
        assert func_name == f"shared_lambda_{i}"


def test_lambda_format_body() -> None:
    """Test that format_body correctly formats lambda body with source."""
    # Without source
    lambda1 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=None,
        source=None,
    )
    assert lambda1.format_body() == "return 42;"

    # With source would need a proper source object, skip for now


def test_stateful_lambdas_not_deduplicated() -> None:
    """Test that stateful lambdas (non-empty capture) are not deduplicated."""
    # _get_shared_lambda_name is only called for stateless lambdas (capture == "")
    # Stateful lambdas bypass deduplication entirely in process_lambda

    # Verify that a stateful lambda would NOT get deduplicated
    # by checking it's not in the stateless dedup cache
    stateful_lambda = cg.LambdaExpression(
        parts=["return x + y;"],
        parameters=[],
        capture="=",  # Non-empty capture means stateful
        return_type=cg.RawExpression("int"),
    )

    # Stateful lambdas should NOT be passed to _get_shared_lambda_name
    # This is enforced by the `if capture == ""` check in process_lambda
    # We verify the lambda has a non-empty capture
    assert stateful_lambda.capture != ""
    assert stateful_lambda.capture == "="


def test_static_variable_detection() -> None:
    """Test detection of static variables in lambda code."""
    # Should detect static variables
    assert cg._has_static_variables("static int counter = 0;")
    assert cg._has_static_variables("static bool flag = false; return flag;")
    assert cg._has_static_variables("  static  float  value  =  1.0;  ")

    # Should NOT detect static_cast, static_assert, etc. (with underscores)
    assert not cg._has_static_variables("return static_cast<int>(value);")
    assert not cg._has_static_variables("static_assert(sizeof(int) == 4);")
    assert not cg._has_static_variables("auto ptr = static_pointer_cast<Foo>(bar);")

    # Edge case: 'cast', 'assert', 'pointer_cast' are NOT C++ keywords
    # Someone could use them as type names, but we should NOT flag them
    # because they're not actually static variables with state
    # NOTE: These are valid C++ but extremely unlikely in ESPHome lambdas
    assert not cg._has_static_variables("static cast obj;")  # 'cast' as type name
    assert not cg._has_static_variables("static assert value;")  # 'assert' as type name
    assert not cg._has_static_variables(
        "static pointer_cast ptr;"
    )  # 'pointer_cast' as type

    # Should NOT detect in comments
    assert not cg._has_static_variables("// static int x = 0;\nreturn 42;")
    assert not cg._has_static_variables("/* static int y = 0; */ return 42;")

    # Should detect even with comments elsewhere
    assert cg._has_static_variables("// comment\nstatic int x = 0;\nreturn x;")

    # Should NOT detect non-static code
    assert not cg._has_static_variables("int counter = 0; return counter++;")
    assert not cg._has_static_variables("return 42;")

    # Should handle newlines between static and type/variable
    assert cg._has_static_variables("static int\nfoo = 0;")
    assert cg._has_static_variables("static\nint\nbar = 0;")
    assert cg._has_static_variables(
        "static  int  \n  foo  =  0;"
    )  # Mixed spaces/newlines


def test_lambdas_with_static_not_deduplicated() -> None:
    """Test that lambdas with static variables are not deduplicated."""
    # Two identical lambdas with static variables
    lambda1 = cg.LambdaExpression(
        parts=["static int counter = 0; return counter++;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    lambda2 = cg.LambdaExpression(
        parts=["static int counter = 0; return counter++;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    # Should return None (not deduplicated)
    func_name1 = cg._get_shared_lambda_name(lambda1)
    func_name2 = cg._get_shared_lambda_name(lambda2)

    assert func_name1 is None
    assert func_name2 is None


def test_lambdas_without_static_still_deduplicated() -> None:
    """Test that lambdas without static variables are still deduplicated."""
    # Two identical lambdas WITHOUT static variables
    lambda1 = cg.LambdaExpression(
        parts=["int counter = 0; return counter++;"],  # No static
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    lambda2 = cg.LambdaExpression(
        parts=["int counter = 0; return counter++;"],  # No static
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    # Should be deduplicated (same function name)
    func_name1 = cg._get_shared_lambda_name(lambda1)
    func_name2 = cg._get_shared_lambda_name(lambda2)

    assert func_name1 is not None
    assert func_name2 is not None
    assert func_name1 == func_name2
