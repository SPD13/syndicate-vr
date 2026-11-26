# VR viewer: configuration options

The VR viewer (`vr/viewer`, built by `vr/viewer/build.sh` into
`build/vr-viewer/syndwars-vr.exe`) runs the game core in its own process
and shows the mission as a miniature city on a table, in a headset through
OpenXR or in a desktop window. This document lists every option, its
default and what it changes. See `docs/swview.md` for how the viewer reads
the world.

## Starting the viewer

```
vr\run_vr.cmd [campaign,mission] [options...]
```

`run_vr.cmd` passes the mission and the game data folder and hands every
other option on to the viewer, e.g.:

```
vr\run_vr.cmd 0,1
vr\run_vr.cmd 1,2 --look upgraded --fog-scale 1.5
vr\run_vr.cmd 3,61 --desktop --no-fog
```

Units used below:

- **Map units**: the game's coordinates. A map is 32768 × 32768 map units,
  a tile is 256, a person is about 200 high; 110 map units are taken as
  one metre of the game world.
- **Table**: the city is drawn on a table in the room (VR) or in front of
  an orbit camera (desktop).

## Game

| Option | Default | Impact |
|---|---|---|
| `--mission C,M` | `0,1` | Campaign and mission the game core plays. `run_vr.cmd` takes it as its first argument. |
| `--data DIR` | `$SWARS_GAMEDATA`; `run_vr.cmd`: `gamedata` next to the repository | Game data folder (levels, textures, sprites, sounds). The viewer does not start without it. |
| `--turn-ms MS` | `50` (20 turns a second, the game's speed) | Real time per game turn. Lower values play faster. The game itself is the same: it does not depend on time, only on turns. |

## Look

| Option | Default | Impact |
|---|---|---|
| `--look classic\|upgraded` | `classic` | Starting look. `classic`: the game's own sprites for people, items and effects. `upgraded`: the 3D models, animations and particle effects of the assets repository; anything without a model is still drawn as its sprite. Switch at runtime with L (desktop) or B on the right controller. |
| `--assets DIR` | `$SWARS_ASSETS3D`, else `assets3d` next to the repository | Assets repository of the upgraded look (`manifest.json`, `models/`, `effects/`). It is only read. Without it, the upgraded look draws nothing different. |

## World shown

| Option | Default | Impact |
|---|---|---|
| (whole world) `--whole-world` | on | The whole map is drawn, around and beyond the table. It costs nothing extra: the whole map is on the GPU anyway. |
| `--table-only` | off | Only the square of the map on the table is drawn, as before. Switch at runtime with C (desktop) or a click of the right stick. Pointing and commands follow what is drawn. |
| `--zoom HALF` | `5120` | Map units from the table's centre to its edge at the start (1024 to 17000). Zoom at runtime with the right stick (up/down) or `+`/`-`. |
| `--center X Z` | off: the table follows the game camera | Map point at the table's centre at the start. Following stops; A on the right controller or F switches it back on. |

## Fog of war

The fog of war keeps the original game's limited view in VR, where the
whole city would otherwise be in sight. Code: `vr/viewer/fog.h`.

| Option | Default | Impact |
|---|---|---|
| (fog on) `--fog` | on | The player sees the world around their agents only (below). Switch at runtime with G (desktop) or the right grip. |
| `--no-fog` | off | Everything is visible, as without the feature. |
| `--fog-scale F` | `1.0` | Multiplies the default radius of the agents' circles. `1.5` lets agents see 50 % further, `0.5` half as far. |
| `--fog-radius R` | `0` (unused) | Sets the radius in map units, replacing the default and `--fog-scale`. For example 2560 is 10 tiles, about 23 m of the game world. |

**Default radius.** Half the width of the desktop game's window at its
furthest zoom: 640 pixels at a scale of 304 (the longest-range weapon's
zoom with `ZoomMin = 145` of `conf/rules.ini`) gives 2156 map units,
8.4 tiles, about 20 m. So an agent sees to each side as far as the
most-zoomed-out desktop view does. The value comes from the game
(`SwvHeader.view_scale_min`), so a different `ZoomMin` changes it too.

**What shows where:**

| Area | Ground and buildings | Scenery (trees, lamps, bins) | People, vehicles, effects, lights, bars |
|---|---|---|---|
| Inside an agent's circle | normal | normal | shown |
| Seen before, no agent near | grey, under a slowly drifting smoke | grey | hidden |
| Never seen | dark ground, no buildings | hidden | hidden |

- The circles are around every living agent of the player. Where agents
  are close together the circles join; the white dashed line on the ground
  is the outer edge of the joined shape.
- People and vehicles outside the circles cannot be pointed at or
  targeted, and their markers (health, damage numbers, lock) are hidden.
  Agents can still be sent anywhere on the ground.
- The viewer records what the agents have seen even while the fog is off,
  so switching it on mid-mission shows the real history. The record is
  kept for the session only, in cells of 128 × 128 map units.
- A vehicle partly inside a circle is cut at the circle's edge.
- With the fog off, the white dashes outline the most the desktop game's
  window can show (the same furthest zoom), around the game camera.

## Display and testing

| Option | Default | Impact |
|---|---|---|
| `--desktop` | off | Desktop window with an orbit camera instead of the headset. The viewer also falls back to it when no headset or OpenXR runtime is available. |
| `--orbit YAW PITCH DIST` | `0.6 0.75 1.6` | Desktop camera: angle around the table (radians), height angle (radians), distance (metres). The mouse drag and wheel change it at runtime. |
| `--screenshot FILE SECONDS` | off | Desktop: saves the window as a PPM image after that many seconds, then quits. |
| `--click X Y SECONDS BUTTON` | off | Desktop: simulates a mouse click at window pixel X, Y (button 1 left, 2 right). Can be repeated. |
| `--press X Y SECONDS BUTTON HOLD` | off | Desktop: as `--click`, with the button held for HOLD seconds (charging weapons). |
| `--key KEY SECONDS` | off | Desktop: simulates a key press (an SDL key name, e.g. `L`). |
| `--dump-block TURN FILE` | off | Writes the world export of that turn to FILE and quits, to compare with `swcore_coreonly --view-dump`. |

## Environment variables

| Variable | Impact |
|---|---|
| `SWARS_GAMEDATA` | Game data folder when `--data` is not given. |
| `SWARS_ASSETS3D` | Assets repository when `--assets` is not given. |

## Controls

VR (Valve Index, Oculus Touch):

| Control | Action |
|---|---|
| Right trigger | Point at an agent to select it, at the ground to walk there, at a vehicle to get in, at a row of the wrist panel to pick an agent or weapon |
| Left trigger | Shoot at what the left hand points at; held, weapons fire on and charging weapons charge |
| Left A / B (Touch: X / Y) | Previous / next weapon |
| Left stick | Move the map (it slides the way the stick is pushed) |
| Left stick click | Place the table in front of you |
| Right stick | Zoom (up/down), turn the table (left/right) |
| Right stick click | Whole world / table only |
| Right A | Follow the game camera on/off |
| Right B | Classic / upgraded look |
| Right grip | Fog of war on/off |

Desktop:

| Control | Action |
|---|---|
| Left click | As the right trigger |
| Right button (held) | As the left trigger |
| `[` / `]` | Previous / next weapon |
| Mouse drag, wheel | Orbit the camera, its distance |
| Arrows | Move the map |
| `+` / `-` | Zoom |
| Q / E | Turn the table |
| F | Follow the game camera on/off |
| L | Classic / upgraded look |
| C | Whole world / table only |
| G | Fog of war on/off |
| Enter | Confirm the end of a decided mission |
| Esc | Quit |
