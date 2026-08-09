from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The component reaches climate_ir through AUTO_LOAD, which the unit test build does not
    # follow, and climate_ir itself doesn't declare `climate` even though ClimateIR derives from
    # climate::Climate. Pull both in explicitly so the test can include the component header.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]
