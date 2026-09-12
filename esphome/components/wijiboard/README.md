# `wijiboard`

Drives the two-armed planchette of a [WijiBoard](https://github.com/nerd-sniped/WijiBoard)
from ESPHome, so words sent from Home Assistant are spelled out letter by letter.

This is a port of the original WijiBoard firmware. The board's character map and the
inverse kinematics that turn a board coordinate into a pair of arm angles are carried over
unchanged; the WiFi access point, the web interface and the blocking motor loops are not,
because ESPHome already handles the network and the main loop must never block.

## Wiring

The board has two 28BYJ-48 stepper motors behind ULN2003 drivers. Declare them with the
standard `uln2003` stepper platform and hand the two IDs to `wijiboard`. The stock PCB
wires the second motor with its B and C pins swapped, which the example below reproduces.

## Configuration

```yaml
stepper:
  - platform: uln2003
    id: wiji_stepper_1
    pin_a: GPIO4
    pin_b: GPIO7
    pin_c: GPIO5
    pin_d: GPIO6
    max_speed: 600 steps/s
    acceleration: 100 steps/s^2
    deceleration: 100 steps/s^2
    sleep_when_done: true
  - platform: uln2003
    id: wiji_stepper_2
    pin_a: GPIO8
    pin_b: GPIO11
    pin_c: GPIO9
    pin_d: GPIO10
    max_speed: 600 steps/s
    acceleration: 100 steps/s^2
    deceleration: 100 steps/s^2
    sleep_when_done: true

wijiboard:
  - id: wiji
    stepper_1: wiji_stepper_1
    stepper_2: wiji_stepper_2

text:
  - platform: wijiboard
    wijiboard_id: wiji
    name: Message
```

### Options

| Option | Default | Meaning |
| --- | --- | --- |
| `stepper_1`, `stepper_2` | required | IDs of the two steppers that carry the arms. |
| `base_separation` | `25.8` | Distance in mm between the two shoulder joints. |
| `upper_arm_length` | `85.0` | Length in mm of the link from shoulder to elbow. |
| `forearm_length` | `110.0` | Length in mm of the link from elbow to planchette. |
| `steps_per_rotation` | `2048` | Steps in one full turn of a motor. |
| `rest_position` | `[-1024, 0]` | Step positions the planchette parks at between letters. |
| `homing_positions` | `[1024, 2048, -1050, -1300, 550, -530]` | Step targets of the three-part homing run against the mechanical stops. |
| `hold_time` | `500ms` | How long the planchette sits on a letter. |
| `letter_pause` | `200ms` | Pause between two letters. |
| `space_pause` | `1s` | Pause for a space in a word. |
| `return_home_between_letters` | `true` | Park at the rest position after every letter, so repeated letters read clearly. |
| `home_on_boot` | `true` | Run the homing sequence at start-up. Turn this off only if the planchette is already parked. |
| `max_word_length` | `64` | Longest word the board will queue; anything longer is truncated. |
| `use_default_letters` | `true` | Start from the stock board's character map. |
| `letters` | see below | Add to or override the character map. |

### Character map

Every entry is either a board coordinate in mm, measured from the middle of the board, or
a pair of arm angles in degrees for points the linkage cannot solve for:

```yaml
wijiboard:
  - id: wiji
    stepper_1: wiji_stepper_1
    stepper_2: wiji_stepper_2
    letters:
      "A":
        x: -143.0
        y: 99.0
      ",":
        theta1: 130.0
        theta2: 268.0
```

Keys are single characters and are matched case-insensitively. A space is always a pause,
so its length is set with `space_pause` rather than mapped here. Characters that are not in
the map are skipped with a log message.

### Triggers

`on_word_start` (the word as `x`), `on_letter` (the character code as `x`, a `uint8_t`),
`on_word_end`, and `on_home`, which fires when the homing sequence finishes.

## Actions

- `wijiboard.write_word` — spell a word. Starting a new word abandons whatever was in progress.
- `wijiboard.write_letter` — point at a single character.
- `wijiboard.goto_xy` — point at any board coordinate in mm.
- `wijiboard.goto_angles` — drive the arms to raw angles in degrees, skipping the inverse kinematics.
- `wijiboard.home` — re-run the homing sequence.
- `wijiboard.stop` — abandon the current word and park the planchette.

## Conditions

- `wijiboard.is_busy` — true while the board is homing, moving, holding or pausing.

See `examples/wijiboard.yaml` for a full device configuration, including Home Assistant
actions for sending a word and for sending custom coordinates.
