import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # Compile the port frequency path so the tests can reach it
        cg.add_define("I2C_PORT_FREQUENCY_COUNT", 1)

    # A MULTI_CONF component gets no entry in the host build, so its to_code would never run.
    manifest.multi_conf = False
    manifest.to_code = to_code_testing
