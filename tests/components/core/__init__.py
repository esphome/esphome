import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def _reserve_test_sensors(config: ConfigType) -> None:
    # test_component_iterator.cpp registers two sensors at run time
    cg.reserve_runtime_entities("sensor", 2)


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # core (esphome/core/config.py) must run its to_code during C++ test builds
    # because it bootstraps the fundamental application infrastructure that all
    # components depend on (component registration, event loop, etc.).
    # USE_LINE_READER keeps line_reader.cpp in the build for its tests.
    add_defines_to_code(manifest, "USE_LINE_READER", after=_reserve_test_sensors)
