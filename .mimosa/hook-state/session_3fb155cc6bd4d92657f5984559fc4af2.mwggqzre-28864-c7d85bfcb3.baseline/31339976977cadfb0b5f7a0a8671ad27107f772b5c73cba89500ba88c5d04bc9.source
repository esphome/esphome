"""Helpers for the remote_transmitter component tests."""

import re

_PROTOCOL_CALL_BODY = re.compile(
    r"auto apply_call = \S+->(?:protocol|encode)_call<remote_base::\w+>\(\);\n(.*?)apply_call\.perform\(\);",
    re.DOTALL,
)


def protocol_call_bodies(main_cpp: str) -> list[str]:
    """The statements of each generated transmit action, in config order."""
    return _PROTOCOL_CALL_BODY.findall(main_cpp)
