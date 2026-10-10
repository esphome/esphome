"""select.is option lists live in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_select_is_options_use_shared_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("select_is.yaml"))

    tables = re.findall(
        r"static constexpr const char \* (select_is_options\w*)\[\] PROGMEM = \{([^;]*)\};",
        main_cpp,
    )
    # Two conditions with [one, two] share one table; [three] gets its own
    assert sorted(body for _, body in tables) == ['"one", "two"', '"three"']
    one_two = next(name for name, body in tables if body == '"one", "two"')
    assert main_cpp.count(f", {one_two});") == 2
    assert "SelectIsCondition<0" in main_cpp
