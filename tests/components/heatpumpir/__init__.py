from pathlib import Path

import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD sits on the climate platform, which the unit test build does not load.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]

    async def to_code(config: ConfigType) -> None:
        # Host tests stub HeatpumpIR headers in this directory.
        cg.add_build_flag(f"-I{Path(__file__).parent}")

    manifest.to_code = to_code
