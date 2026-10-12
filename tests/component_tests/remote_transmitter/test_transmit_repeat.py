"""repeat on a generated transmit action, and toto's default."""

from collections.abc import Callable
from pathlib import Path
import re


def test_transmit_repeat(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("transmit_repeat.yaml"))
    bodies = re.findall(
        r"auto apply_call = \S+->protocol_call<remote_base::\w+>\(\);\n(.*?)apply_call\.perform\(\);",
        main_cpp,
        re.DOTALL,
    )
    assert len(bodies) == 4
    toto_default, toto_explicit, nec_repeat, nec_once = bodies
    # toto sends 3 times 36 ms apart unless the config says otherwise.
    assert "apply_call.set_send_times(3);" in toto_default
    assert "apply_call.set_send_wait(36000);" in toto_default
    assert "apply_call.set_send_times(2);" in toto_explicit
    assert "apply_call.set_send_wait(20000);" in toto_explicit
    # A bare count gets the 25 ms default wait, in microseconds.
    assert "apply_call.set_send_times(4);" in nec_repeat
    assert "apply_call.set_send_wait(25000);" in nec_repeat
    # Without repeat neither setter is emitted; the call sends once.
    assert "set_send_times" not in nec_once
    assert "set_send_wait" not in nec_once
