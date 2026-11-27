"""The ble switch carries a wizard role, so Home Assistant can find it in the wizard."""

from collections.abc import Callable
import json
from pathlib import Path
import re

from esphome.helpers import zstd_module


def test_switch_has_its_wizard_role(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("switch_wizard_role.yaml"))

    match = re.search(
        r"const uint8_t esphome::api::API_WIZARD_DATA\[\] PROGMEM = \{([^}]*)\};",
        main_cpp,
    )
    assert match is not None
    blob = bytes(int(byte) for byte in match.group(1).split(", "))
    document = json.loads(zstd_module().decompress(blob))
    assert [entity["role"] for entity in document["pages"][0]["entities"]] == [
        "ble.enabled",
    ]
