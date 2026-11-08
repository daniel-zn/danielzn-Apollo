# Cursor State

On Windows, Apollo tells a client when a game has captured the mouse cursor, so the client can hold its
own pointer in the stream while a game uses the mouse to look around, and let it leave the window the rest of
the time. [Artemis for Mac](https://github.com/daniel-zn/artemis-macos) uses it for its
**Switch mouse mode automatically for games** setting (on by default): relative mouse while captured,
absolute ("remote desktop") mouse while free. Ctrl+Alt+Shift+M on the client makes it manual again.

Stock Moonlight clients and stock hosts are unaffected: the messages are only sent to a client that asks for them.

## When the cursor counts as captured

Apollo samples the cursor every 100 ms while a client that asked for it is connected:

- **Hidden:** `GetCursorInfo` reports no `CURSOR_SHOWING` (`CURSOR_SUPPRESSED`, i.e. touch or pen, counts as shown), or
- **Clipped:** `GetClipCursor` returns a rectangle smaller than the monitor it is on.

Windows hides the pointer while you type ("Hide pointer while typing"), so a cursor that disappears right after
keyboard input, without mouse input, counts as free until the next mouse input. A new state counts once it
has held for 250 ms. When Apollo can't read the cursor (the secure desktop: UAC, lock screen), it counts as free.

## Protocol

1. The host advertises `a=x-apollo-cursor:1` in its RTSP DESCRIBE response.
2. A client that wants the messages adds `a=x-apollo-cursor.enable:1` to its ANNOUNCE.
3. The host sends control stream message type `0x3080` (encrypted like other control messages) once when the
   client connects and again on every change. Its payload is 2 bytes:

| Bytes | Field |
|-------|-------|
| 1 | version (1) |
| 1 | flags: bit 0 = captured |

Clients ignore unknown control message types, and hosts ignore unknown ANNOUNCE attributes, so either side can
be stock.

The detection and message encoding are in `src/cursor_state.cpp`, tested by `tests/unit/test_cursor_state.cpp`.
