import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # Storage is sized by code generation; the gtests register three batteries and
        # read the cells of two of them.
        cg.add_define("PYLONTECH_LISTENER_COUNT", 3)
        cg.add_define("PYLONTECH_BATTERY_COUNT", 3)
        cg.add_define("PYLONTECH_CELL_BATTERY_COUNT", 2)
        cg.add_define("USE_LINE_READER")

    # A MULTI_CONF component gets no entry in the host build, so its to_code would never run.
    manifest.multi_conf = False
    manifest.to_code = to_code_testing
