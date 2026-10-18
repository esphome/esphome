import functools

from esphome.components.json import enable_arena
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # json's to_code calls cg.add_library("bblanchon/ArduinoJson", ...). C++
    # unit test builds that pull json in transitively (e.g. api) need that
    # library registration to happen, otherwise json_util.cpp fails to find
    # ArduinoJson.h.
    manifest.enable_codegen()
    # The JsonArena host test needs the arena compiled in, as a consumer would request it
    real_to_code = manifest.to_code

    @functools.wraps(real_to_code)
    async def to_code_with_arena(config):
        await real_to_code(config)
        enable_arena()

    manifest.to_code = to_code_with_arena
