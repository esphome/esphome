# Config keys shared by opentherm42's platform files (switch/number/binary_sensor/sensor). Named
# after the OpenTherm Protocol Specification v4.2 section + field hierarchy, with "HB"/"LB" stripped
# and the spec's term for the boiler side of the conversation replaced by "boiler" (see CLAUDE.md's
# C++ enumerator-naming rule for why that term is avoided project-wide).

# Every platform file's schema uses this to reference the opentherm42: hub.
CONF_OPENTHERM42_ID = "opentherm42_id"

# Per-entity cadence field shared by number/sensor/text_sensor (every 1:1 id) -- "due once every N
# passes" (see hub.h's Entry), not a wall-clock duration. esphome.const's own CONF_UPDATE_INTERVAL
# can't be reused: its established meaning across the rest of ESPHome is a time period, and this
# option's unit is deliberately not one -- see hub.h's Entry/sweep_length_passes_ comments for why.
CONF_UPDATE_EVERY = "update_every"

# §5.3.1 Class 1, IDs 1/8/71: numeric setpoints the master writes.
CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT = (
    "control_and_status_information_control_setpoint"
)
