from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    # USE_UART_DEBUGGER compiles the virtual UART's debugger path for its tests.
    add_defines_to_code(manifest, "USE_UART_DEBUGGER")
