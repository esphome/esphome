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

# Synthetic diagnostic entity, not tied to any OpenTherm data-id -- deliberately NOT given the
# sensor_and_informational_data_* prefix used throughout this file, since that prefix names a real
# spec chapter (§5.3.4 Class 4) this entity has nothing to do with. How long the most recent full
# sweep (every configured id attempted at least once) actually took -- see hub.h's
# sweep_length_passes_ comment. Unconditionally available regardless of what else is configured
# (no update_every field of its own -- it's a live measurement the hub publishes on its own
# schedule, not something a user cadence-configures).
CONF_SWEEP_DURATION = "sweep_duration"

# Same nature as the sweep duration entity above, but for a single pass (one full scan of every
# configured id, however many of them happened to be due on it) rather than a whole sweep.
CONF_PASS_DURATION = "pass_duration"

# Synthetic diagnostic entity, not tied to any OpenTherm data-id: true if any conversation during
# the most recently completed sweep was rejected or failed at the datalink level -- the kind of
# thing that otherwise only ever shows up as a log line. Same lifecycle as the sweep duration
# entity above (unconditionally available, no update_every field, published once per sweep).
CONF_SWEEP_HAD_ERRORS = "sweep_had_errors"
