import esphome.codegen as cg
from esphome.components.mk2pvrouter import Mk2PVRouter
import esphome.config_validation as cv
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The C++ unit test harness synthesizes this component's config from its
    # bare schema ({}), bypassing the normal config-validation pass that
    # resolves auto-generated IDs (config[CONF_ID] stays an unresolved
    # ID(None, ...), which codegen emits as an empty/invalid C++ variable
    # name). It also bypasses CORE.component_ids registration, so
    # cg.register_component() fails ("Component ID  was not declared...").
    # The gtests never exercise the generated App/Component wiring anyway
    # (they construct the C++ class directly), and the sensor platform's own
    # test stub builds its own independent dummy hub rather than resolving
    # this one -- so all that's needed here is a Pvariable with an explicit,
    # resolved id to produce valid (if unused) codegen output.
    async def to_code_testing(config: ConfigType) -> None:
        cg.new_Pvariable(cv.declare_id(Mk2PVRouter)("mk2pvrouter_test_hub_id"))

    manifest.to_code = to_code_testing
