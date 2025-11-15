"""Tests for lambda deduplication in cpp_generator."""

import pytest

from esphome import cpp_generator as cg
from esphome.core import CORE


@pytest.fixture(autouse=True)
def reset_core():
    """Reset CORE.data before each test."""
    CORE.reset()
    yield
    CORE.reset()


def test_deduplicate_identical_lambdas():
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
    func_name1 = cg._try_deduplicate_lambda(lambda1)
    func_name2 = cg._try_deduplicate_lambda(lambda2)

    # Both should get the same function name (deduplication happened)
    assert func_name1 == func_name2
    assert func_name1 == "shared_lambda_0"


def test_different_lambdas_not_deduplicated():
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

    func_name1 = cg._try_deduplicate_lambda(lambda1)
    func_name2 = cg._try_deduplicate_lambda(lambda2)

    # Different lambdas should get different function names
    assert func_name1 != func_name2
    assert func_name1 == "shared_lambda_0"
    assert func_name2 == "shared_lambda_1"


def test_different_return_types_not_deduplicated():
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

    func_name1 = cg._try_deduplicate_lambda(lambda1)
    func_name2 = cg._try_deduplicate_lambda(lambda2)

    # Different return types = different functions
    assert func_name1 != func_name2


def test_different_parameters_not_deduplicated():
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

    func_name1 = cg._try_deduplicate_lambda(lambda1)
    func_name2 = cg._try_deduplicate_lambda(lambda2)

    # Different parameters = different functions
    assert func_name1 != func_name2


def test_flush_lambda_dedup_declarations():
    """Test that deferred declarations are properly stored for later flushing."""
    # Create a lambda which will create a deferred declaration
    lambda1 = cg.LambdaExpression(
        parts=["return 42;"],
        parameters=[],
        capture="",
        return_type=cg.RawExpression("int"),
    )

    cg._try_deduplicate_lambda(lambda1)

    # Check that declaration was stored
    assert cg._KEY_LAMBDA_DEDUP_DECLARATIONS in CORE.data
    assert len(CORE.data[cg._KEY_LAMBDA_DEDUP_DECLARATIONS]) == 1

    # Verify the declaration content is correct
    declaration = CORE.data[cg._KEY_LAMBDA_DEDUP_DECLARATIONS][0]
    assert "shared_lambda_0" in declaration
    assert "return 42;" in declaration

    # Note: The actual flushing happens via CORE.add_job with FINAL priority
    # during real code generation, so we don't test that here


def test_shared_function_lambda_expression():
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


def test_lambda_deduplication_counter():
    """Test that lambda counter increments correctly."""
    # Create 3 different lambdas
    for i in range(3):
        lambda_expr = cg.LambdaExpression(
            parts=[f"return {i};"],
            parameters=[],
            capture="",
            return_type=cg.RawExpression("int"),
        )
        func_name = cg._try_deduplicate_lambda(lambda_expr)
        assert func_name == f"shared_lambda_{i}"


def test_lambda_format_body():
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
