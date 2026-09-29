"""Constants used by zephyr_pm."""

CONF_ENABLE_LIGHT_SLEEP = "enable_light_sleep"
CONF_POLICY = "policy"
CONF_POWER_DOWN_DEVICE = "power_down_device"
CONF_STATS = "stats"

# CPU_FREQ_POLICY_TIMING_NOISE also exists upstream but is a security mitigation,
# not a power policy, so it's deliberately excluded.
POLICY_ON_DEMAND = "on_demand"
POLICY_PRESSURE = "pressure"
POLICIES = (POLICY_ON_DEMAND, POLICY_PRESSURE)
