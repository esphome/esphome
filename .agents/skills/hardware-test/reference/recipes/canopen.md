# CANopen SDO client on two ESP32-C3 nodes (#22410)

Two ESP32-C3 devkits with SN65HVD230 transceivers on one bus (wiring in `reference/boards.md`), files in
`scripts/canopen/`; `c3-can.yaml` holds the shared C3 board, logger and bus setup. Check the bus first with
`buscheck.yaml` on both boards (`me: "1"` and `"2"`): each sends a counter every 500 ms and logs TEC/REC every 5 s;
expect 0 errors both ways.

- Server (node B, `nodeb.yaml`): the author's branch https://github.com/mrk-its/esphome/tree/canopen-full,
  `node_id: 0x20`, objects added in an `on_boot` lambda at priority 900 with
  `id(hub).get_od().add_var(index, OD_READ_WRITE [| ODA_STR], ptr, len)` (before the hub's setup, which builds the
  object dictionary). A writable string needs `ODA_STR`; `read_od()` from a lambda shows what the client wrote.
  `slow_object.hpp` makes 0x2003 writes and 0x2004 reads answer after 450 ms, past the client's 300 ms timeout.
- Client harness (`cotest.hpp`): a pool of requests and one callback that checks result, abort code, value or bytes
  and the completion order; the soak logs latency per request kind. Phases are inline functions called from the
  shared `phases` script in `actions2.yaml`, then each suite's own tail; every line starts with `PASS`, `FAIL`,
  `PHASE`, `SUMMARY` or `LAT`. The queue phase makes 17 calls in one lambda (16 accepted, the 17th rejected).
- `nodea.yaml`: the full suite, including NMT stop and start with a raw `0x000` frame and an offline window.
  `nodea-late.yaml`: the action paths in `actions2.yaml` (`on_success`, `on_error`, `on_value`, lambda node ids, an
  absent node) and five rounds of late responses. `pico.yaml`: a Pico 2 W with an MCP2515/TJA1050 module (8 MHz
  crystal, 3V3, SPI0 18/19/16, CS GP17, 2 MHz) as a third node running the same phases. The TJA1050 is rated for
  4.75 to 5.25 V: on 3V3 it is out of spec (it worked on the short bench bus); an SN65HVD230 is a 3.3 V part.
- `drive.py PORT_A PORT_B OUT SECONDS` (150 or more for `nodea.yaml`) captures both boards, resets them, and holds B
  in reset (RTS on, DTR off) 8 s into A's offline phase for 15 s.

Findings:

- Server off while the client sends a request every 250 ms: 3 TIMEOUT (frames still in the TWAI queue), then
  TRANSPORT_ERROR per request, because nobody ACKs the frames. With the Pico on the bus it ACKs everything, so the
  same test gives pure TIMEOUT. NMT stop gives pure TIMEOUT 0x05040000 (frames ACKed, no SDO answer).
- Latency is one main loop pass per frame round trip (about 16 ms expedited, 16 ms per segment).
- Only one SDO client may talk to node 0x20 at a time (the COB-IDs 0x620/0x5A0 are shared).
- Late responses: before e24ea6ce21 the next request on the same object aborted with 0x05040001 every time.
- In a header shared with Arduino builds, call `micros()` and `millis()` after `using esphome::micros;`, never as
  `esphome::micros()`: the Arduino macro turns that into `esphome::esphome::micros()`.
