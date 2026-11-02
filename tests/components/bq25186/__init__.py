import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One empty config entry so to_code runs and compiles the PG/GPO switch path.
    manifest.multi_conf = False
    manifest.dependencies = manifest.dependencies + ["i2c", "switch"]

    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_BQ25186_PG_GPO")

    manifest.to_code = to_code
