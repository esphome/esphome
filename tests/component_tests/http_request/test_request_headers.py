"""Request headers are PROGMEM tables, shared only between all-constant lists."""

from collections.abc import Callable
from pathlib import Path
import re


def test_request_headers_share_constant_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("request_headers.yaml"))

    # Actions in config order: the five buttons, then the two scripts
    tables = [
        (table, int(count))
        for table, count in re.findall(
            r"->set_request_headers\((\w+), (\d+)\);", main_cpp
        )
    ]
    assert len(tables) == 7
    shared_a, shared_b, other, lambda_a, lambda_b, param_a, param_b = tables
    assert shared_a == shared_b
    assert shared_a[1] == 2
    assert other[1] == 1
    # A list with a lambda may keep static state, so it never shares even with an identical one
    assert lambda_a[0] != lambda_b[0]
    assert len({shared_a[0], other[0], lambda_a[0], lambda_b[0]}) == 4
    for table, _ in tables:
        assert re.search(rf"\b{table}\[\] PROGMEM = \{{", main_cpp)
    # Constant headers ignore their arguments, so scripts whose parameters differ
    # only by name share a table, and it is the only one of its argument type
    assert param_a == param_b
    assert param_a[0] not in {shared_a[0], other[0], lambda_a[0], lambda_b[0]}
    assert re.findall(r"RequestHeader<[^>]+> (\w+)\[\] PROGMEM = \{", main_cpp) == [
        param_a[0]
    ]
