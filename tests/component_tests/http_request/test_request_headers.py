"""Request headers are PROGMEM tables, shared only between all-constant lists."""

from collections.abc import Callable
from pathlib import Path
import re


def test_request_headers_share_constant_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("request_headers.yaml"))

    # Actions in config order: shared_a, shared_b, other, lambda_a, lambda_b
    tables = [
        (table, int(count))
        for table, count in re.findall(
            r"->set_request_headers\((\w+), (\d+)\);", main_cpp
        )
    ]
    assert len(tables) == 5
    shared_a, shared_b, other, lambda_a, lambda_b = tables
    assert shared_a == shared_b
    assert shared_a[1] == 2
    assert other[1] == 1
    # A list with a lambda may keep static state, so it never shares even with an identical one
    assert lambda_a[0] != lambda_b[0]
    assert len({shared_a[0], other[0], lambda_a[0], lambda_b[0]}) == 4
    for table, _ in tables:
        assert re.search(rf"\b{table}\[\] PROGMEM = \{{", main_cpp)
