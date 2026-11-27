"""Compile the runtime result lifecycle with host-only inference-engine and platform doubles."""

import shlex

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["host"]
    manifest.auto_load = []
    manifest.config_schema = cv.Schema({})
    # A multi_conf component gets an empty list in the test build, so to_code would not run
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        sdk = CORE.relative_src_path("tflite_micro_runtime", "__sdk")
        cg.add_build_flag(f"-I{shlex.quote(str(sdk))}")

    manifest.to_code = to_code_testing
