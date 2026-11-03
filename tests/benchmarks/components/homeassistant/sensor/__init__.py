import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # In production the platform's to_code emits this define; the host build
    # suppresses that to_code, and the API server needs the define for the
    # state subscription path the benchmarks drive.
    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_API_HOMEASSISTANT_STATES")

    manifest.to_code = to_code
