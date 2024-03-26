# PULSE Filter

The PULSE filter filters noisy pulses by ensuring that the pulse is in a steady state for at least `filter_length` before allowing the state change to occur.
It counts the pulse time from the rising edge that stayed high for at least `filter_length`, so noise before this won't be considered the start of a pulse.
It then must see a low pulse that is at least `filter_length` long before a subsequent rising edge is considered a new pulse start.

It's operation should be the same as delayed_on_off from the Binary Sensor component.

There are three moving parts in the algorithm that are used to determine the state of the filter.

1. The time between interrupts, measured from the last interrupt, this is compared to filter_length to determine if the pulse has been in a steady state for long enough.
2. A latch variable that is set true when a high pulse is long enough to be considered a valid pulse and is reset when a low pulse is long enough to allow for another pulse to begin.
3. The previous and current state of the pin used to determine if the pulse is rising or falling.

## Ghost interrupts

Observations from the devices show that even though the interrupt should trigger on every rising or falling edge, sometimes interrupts show the same state twice in a row.
The current theory is an interprets occurs, but then the pin changing back faster than the ISR can be called and read the value, meaning it sees the same state twice in a row.
The algorithm interprets these when it sees them as two edges in a row, so will potentially reset a pulse if

## Pulse Filter Truth table

The following is all of the possible states of the filter along with the new inputs.
It also shows a diagram of a possible series of interrupts that would cause the filter to enter that state.
It then has the action that the filter should take and a description of what is happening.

Diagram legend

- `/` rising edge
- `\` falling edge
- `‾` high steady state of at least `filter_length`
- `_` low steady state of at least `filter_length`
- `¦` ghost interrupt

| Length | Latch | From | To  | Diagram | Action             | Description                                                                                          |
| ------ | ----- | ---- | --- | ------- | ------------------ | ---------------------------------------------------------------------------------------------------- |
| T      | 1     | 0    | 0   | `‾\_¦ ` | Reset              | `filter_length` low, reset the latch                                                                 |
| T      | 1     | 0    | 1   | `‾\_/ ` | Reset, Rising Edge | `filter_length` low, reset the latch, rising edge could be a new pulse                               |
| T      | 1     | 1    | 0   | `‾\/‾\` | -                  | Already latched from a previous `filter_length` high                                                 |
| T      | 1     | 1    | 1   | `‾\/‾¦` | -                  | Already latched from a previous `filter_length` high                                                 |
| T      | 0     | 1    | 1   | `_/‾¦`  | Set and Publish    | `filter_length` high, set the latch and publish the pulse                                            |
| T      | 0     | 1    | 0   | `_/‾\ ` | Set and Publish    | `filter_length` high, set the latch and publish the pulse                                            |
| T      | 0     | 0    | 1   | `_/\_/` | Rising Edge        | Already unlatched from a previous `filter_length` low                                                |
| T      | 0     | 0    | 0   | `_/\_¦` | -                  | Already unlatched from a previous `filter_length` low                                                |
| F      | 1     | 0    | 0   | `‾\¦  ` | -                  | Low was not long enough to reset the latch                                                           |
| F      | 1     | 0    | 1   | `‾\/  ` | -                  | Low was not long enough to reset the latch                                                           |
| F      | 1     | 1    | 0   | `‾\/\ ` | -                  | Low was not long enough to reset the latch                                                           |
| F      | 1     | 1    | 1   | `‾¦   ` | -                  | Ghost of 0 length definitely was not long was not long enough to reset the latch                     |
| F      | 0     | 1    | 1   | `_/¦  ` | Rising Edge        | High was not long enough to set the latch, but the second half of the ghost can be a new rising edge |
| F      | 0     | 1    | 0   | `_/\  ` | -                  | High was not long enough to set the latch                                                            |
| F      | 0     | 0    | 1   | `_/\/ ` | Rising Edge        | High was not long enough to set the latch, but this may be a rising edge                             |
| F      | 0     | 0    | 0   | `_¦ `   | -                  | Ghost of 0 length definitely was not long was not long enough to set the latch                       |

## Startup

On startup the filter should not consider whatever state it is in as valid so it does not count a strange pulse.
There are two possible starting configurations, either the pin is high or the pin is low.
If the pin is high, the subsequent falling edge should not count as a pulse as we never saw the rising edge.
Therefore we start in the latched state.
If the pin is low, the subsequent rising edge can be counted as the first pulse.
Therefore we start in the unlatched state.
