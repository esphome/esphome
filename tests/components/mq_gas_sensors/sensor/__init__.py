import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The host build cannot link the adc sources (the ADC component has no host
    # implementation), so leave them out of the unit test build. They are only
    # needed for code generation of `pin:` configurations, which the suppressed
    # `to_code` of the test build never runs, and nothing compiled here
    # references their symbols.
    manifest.auto_load = ["sensor", "voltage_sampler", "text_sensor"]

    # The log mirror is compiled in only when a configuration asks for it; the
    # tests read the component's messages through it, so the build turns it on.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_MQ_GAS_SENSORS_LOG_SENSOR")

    manifest.to_code = to_code_testing
