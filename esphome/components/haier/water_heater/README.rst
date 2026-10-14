Haier hOn water heater controller
================================

This platform provides local control of Haier hOn water heaters.
Hardware testing has only been performed with a VH3W. Compatibility with other
models is unverified; related devices may use different status layouts or encoding.
Disconnect the original Wi-Fi module and connect UART RX and TX to the appliance.
Existing Haier climate code is unchanged; SmartAir2 is not supported.

Configuration
-------------

.. code-block:: yaml

   uart:
     rx_pin: GPIO44
     tx_pin: GPIO43
     baud_rate: 9600
   water_heater:
     - platform: haier
       name: Water Heater
       primary_temperature:
         name: Primary temperature zone
       secondary_temperature:
         name: Secondary temperature zone
       estimated_heating_time:
         name: Heater Estimated Heating Time
       target_temperature:
         name: Heater Target Temperature
       power:
         name: Heater Power
       heating:
         name: Heater Heating
       mode:
         name: Heater Mode
       available_water:
         name: Heater Available Water

Use pins appropriate to your board. Both RX and TX are required. The entity is
exposed by default. Initialization, polling, acknowledgements and controls are
always active.

Controls
--------

Electric maps to Normal, Eco to ECO, and Performance to BPS. Manual targets are
whole degrees from 35 to 75 C in Normal mode. BPS reports its own 80 C target.
Select Electric before changing a target in ECO/BPS. Selecting a mode powers on
an off heater; power-on alone preserves the selected mode. ECO/BPS transitions
pass through Normal using captured commands.

Commands require a status received within 15 seconds. A bounded queue sends one
command at a time and waits for a STATUS response reflecting the requested value.
Timeouts after five seconds or rejections discard the remaining commands. Off cancels unsent writes and waits for the current exchange to finish or time
out before taking priority. It ignores accompanying target temperatures and
does not require a recent status. Other requests while busy are rejected. There are no automatic write retries, optimistic state
updates or startup state restoration. Last reported state is retained if traffic
stops; controls reject stale state.

Communication
-------------

Every five seconds the controller sends device version (61 / 00 07), device ID
(70), or status (01 / 4D 01). Version and ID are revisited once per minute; polling
continues even if initialization is unanswered. Reports (06) receive CONFIRM (05).
Transmissions use clear text with flag 00.
Power, mode and temperature writes have been verified on a VH3W.

The receive parser supports FF FF framing, FF 55 escaping, checksum validation
and optional CRC-16/ARC. Unsupported frame flags are rejected. Partial frames
expire after 300 ms. Only valid 52-byte status payloads beginning 6D 01 update
sensors.
Packet dumps are at VERBOSE level; acknowledgements and control confirmations
are DEBUG. Timeouts and rejected frames remain warnings.

Network indication
------------------

F7 reports 07 when Wi-Fi or Ethernet is connected with an IP address, otherwise
05. This represents local network readiness, not internet, Haier cloud or Home
Assistant connectivity. Each interface is checked independently every 500 ms.
Updates occur on startup, changes, every 30 seconds and in response to F0.
Network messages wait for other exchanges and await CONFIRM or a two-second
timeout. The signal-strength field remains fixed at 100 percent. No manual
network-status selector is provided.

Status fields
-------------

Offsets are zero-based within the decoded payload, including 6D 01.

* Current/display temperature: offset 2, degrees Celsius.
* Secondary temperature: offset 15, degrees Celsius; physical location unknown.
* Target temperature: offset 3 plus 30 C. ECO's adaptive behaviour is unresolved.
* Estimated heating time: offset 7 directly, assumed minutes, not independently
  confirmed. This may remain nonzero while off; it is not an ESPHome countdown.
* Power: offset 39, bit 0. Heating activity: offset 41, bit 0.
* Normal: offsets 38 and 39, bit 4. ECO: offset 41, bit 2. BPS: offset 40, bit 2.
* Available water: offset 45, raw segments 0-3, published as 0%, 33%, 66%,
  and 100%. This estimates usable hot water, not tank fill level. Values above
  3 publish unknown.

Validation
----------

Schema and native captured-frame tests are under tests/component_tests/haier.
The native tests use the Zig C++ compiler pinned in requirements_test.txt, which
CI installs. Missing compilers, compile errors and assertion failures fail the
tests rather than skipping them. The controller tests use simulated UART, clock
and entity interfaces; they do not replace firmware builds or hardware testing.
Install the test dependencies and run:

.. code-block:: console

   python -m pip install -r requirements_test.txt
   python -m pytest tests/component_tests/haier


Communication health
--------------------

The optional communication binary sensor uses the connectivity device class.
It starts disconnected, becomes connected after a valid appliance status, and
returns to disconnected after communication_timeout (default 20s) without one.
The timeout can be configured even without the optional sensor. Values below
20s may cause false disconnects during the periodic device-information polls.
Component warnings and
transition logs report loss and recovery. Other readings retain their last values.
This measures received heater status independently of Wi-Fi/Ethernet connectivity;
an acknowledgement alone does not establish communication health.

.. code-block:: yaml

   communication_timeout: 20s
   communication:
     name: Heater Communication

Poll requests expire after five seconds. Pending network updates take precedence
over the next poll after an exchange finishes or times out.

For an unknown operating mode, the mode text sensor reports Unknown. While powered
on, the main entity retains the last recognized active mode across power-off.
Before any recognized mode is received it uses Electric as a display fallback,
because the base entity has no Unknown operation mode. A warning logs this case;
the fallback does not send a command or enable Normal-mode temperature writes.
