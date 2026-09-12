import esphome.codegen as cg
from esphome.components.mk2pvrouter import Mk2PVRouter, register_mk2pvrouter_listener
from esphome.components.mk2pvrouter.sensor import Mk2PVRouterSensor, is_centi_scaled
import esphome.config_validation as cv
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # populate_dependency_config()'s domain.platform branch (used here for
    # "sensor.mk2pvrouter") only appends {platform: "mk2pvrouter"} to
    # config["sensor"] -- no schema ever runs for it, so config[CONF_ID],
    # [CONF_TAG] and [CONF_MK2PVROUTER_ID] don't exist and the real to_code()
    # can't run. All the gtests need from codegen here is
    # MK2PVROUTER_LISTENER_COUNT being defined (>=1), so stub to_code to build
    # standalone IDs and call register_mk2pvrouter_listener() directly. The
    # dummy hub is never exercised at runtime (this build's setup() is never
    # called; gtests construct the C++ classes directly), so it doesn't need
    # to be a "real", config-resolved hub instance.
    #
    # ID(None, ...) objects compare/hash equal regardless of type (ID.__eq__
    # only looks at .id), so auto-generated IDs for the hub and sensor here
    # would collide with each other -- explicit id strings avoid that.
    async def to_code_testing(config: ConfigType) -> None:
        hub = cg.new_Pvariable(
            cv.declare_id(Mk2PVRouter)("mk2pvrouter_test_dummy_hub_id")
        )
        tag = "P"
        var = cg.new_Pvariable(
            cv.declare_id(Mk2PVRouterSensor)("mk2pvrouter_test_dummy_sensor_id"),
            tag,
            is_centi_scaled(tag),
        )
        await register_mk2pvrouter_listener(hub, var)

    manifest.to_code = to_code_testing
