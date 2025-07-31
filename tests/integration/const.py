"""Constants for integration tests."""

# Network constants
DEFAULT_API_PORT = 6053
LOCALHOST = "127.0.0.1"

# Timeout constants
API_CONNECTION_TIMEOUT = 30.0  # seconds
PORT_WAIT_TIMEOUT = 30.0  # seconds
PORT_POLL_INTERVAL = 0.1  # seconds

# Process shutdown timeouts
SIGINT_TIMEOUT = 5.0  # seconds
SIGTERM_TIMEOUT = 2.0  # seconds
