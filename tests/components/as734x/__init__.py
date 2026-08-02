from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # as734x is a sensor platform, so it declares what it needs in sensor.py rather than in the
    # component manifest. The C++ unit test build resolves dependencies from the manifest, so name
    # them here or the drivers fail to find i2c.h and sensor.h.
    manifest.dependencies = [*manifest.dependencies, "i2c", "sensor"]
