# Per-client Display Scale

Apollo remembers the Windows display scale (100%, 125%, 150%, …) each client used on its virtual display and
sets it again whenever that client connects or resumes. A phone can keep 200% while a desktop client keeps
100%, without any change to the clients. Windows only.

## How to use it

1. Connect from a client with a virtual display.
2. On the host, open Settings > System > Display, select the virtual display and pick a scale.
3. Disconnect (or quit the app). Apollo stores the scale for that client.

Next time that client connects, Apollo sets its stored scale right after the display mode. The Clients list in
the web UI (PIN page) shows each client's **Display Scale** and lets you change it; **Auto** leaves the scale at
Windows' recommended value for the resolution.

## Behaviour

- **Stored value.** `display_scale` in the client's `named_devices` entry of `sunshine_state.json`: `0` (Auto) or
  a percentage from Windows' list (100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500). Other values read
  from the file or the web UI become the nearest step; a missing key is Auto.
- **When it is saved.** For the client whose mode the virtual display currently has (the "owner"): when one of
  its stream sessions ends, before another client's resume takes the display over, and before the virtual
  display is removed. The scale is saved as Auto when it equals Windows' recommendation at that moment, so a
  client that never changed it keeps following Windows' default at any resolution.
- **When it is applied.** After the virtual display is created at launch, and after the mode is checked or
  changed on a resume (`apply_client_display_mode`). Auto sets the recommended scale, so a client never
  inherits the previous client's scale.
- **Several clients.** The client that connected or resumed most recently owns the display and its scale
  (the same rule as the display mode). The log names the owner.
- **Not allowed at this resolution.** Windows limits the scale range by resolution. A stored scale outside the
  range is clamped to the nearest allowed step and logged; the stored value stays as it was, so a larger
  resolution later gets the full value.
- **Only the virtual display.** The scale is read and set on the virtual display's source only (looked up by
  its GDI name); other monitors are never touched. Nothing happens when no virtual display is in use.

## How the scale is set

Windows has no public API to set a monitor's scale. Apollo uses the undocumented `DisplayConfigGetDeviceInfo` /
`DisplayConfigSetDeviceInfo` types `-3` (get) and `-4` (set) that Settings uses, as in the open-source SetDpi
and windows-DPI-scaling-sample projects. They work in steps relative to the recommended scale: get returns the
minimum, current and maximum step relative to the recommendation; set takes one relative step. When Apollo runs
as SYSTEM, the calls are made while impersonating the signed-in user, so the setting lands in that user's
desktop like a change made in Settings.

## Client-side preference

Not part of this change. Moonlight clients have no launch parameter for a desktop scale: the existing
`scaleFactor` launch parameter (and the app's `scale-factor`) scales the virtual display's **resolution**, not the
Windows UI scale. A client setting would need a new launch parameter in both the client and Apollo.
