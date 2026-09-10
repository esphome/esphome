"""Constants for integration tests."""

# Network constants
DEFAULT_API_PORT = 6053
LOCALHOST = "127.0.0.1"

# Timeout constants
API_CONNECTION_TIMEOUT = 30.0  # seconds
PORT_WAIT_TIMEOUT = 30.0  # seconds
PORT_POLL_INTERVAL = 0.1  # seconds

# The well-known all-zeros provisioning PSK, a key to provision over it, and
# the time the device takes to activate a newly saved key (100 ms timer plus
# margin)
ZERO_PSK = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
PROVISIONING_PSK = b"bm5ubm5ubm5ubm5ubm5ubm5ubm5ubm5ubm5ubm5ubm4="
KEY_ACTIVATION_DELAY = 0.5  # seconds

# Process shutdown timeouts
SIGINT_TIMEOUT = 5.0  # seconds
SIGTERM_TIMEOUT = 2.0  # seconds
