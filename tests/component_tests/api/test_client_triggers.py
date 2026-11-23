"""Tests for the argument types of the api client triggers."""

from esphome.components.api import _has_capturing_lambda
from esphome.core import Lambda


def test_capturing_lambda_needs_owned_args() -> None:
    """A nested lambda that captures may keep the arguments past the trigger."""
    conf = [
        {
            "lambda": Lambda(
                "App.scheduler.set_timeout(&k, 1000, [client_address]() {});"
            )
        }
    ]
    assert _has_capturing_lambda({"then": conf})
    assert _has_capturing_lambda([{"lambda": Lambda("auto f = [=] { return 1; };")}])


def test_plain_lambda_keeps_string_ref() -> None:
    """Lambdas that only read the arguments, or index an array, keep StringRef."""
    conf = {
        "then": [
            {"logger.log": {"args": [Lambda("return client_info.c_str();")]}},
            {"lambda": Lambda("buf[0] = client_address[0];")},
        ]
    }
    assert not _has_capturing_lambda(conf)
