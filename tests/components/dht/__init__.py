from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # dht is a sensor platform; its header needs the sensor component
    manifest.dependencies = manifest.dependencies + ["sensor"]
