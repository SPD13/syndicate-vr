# VR viewer: configuration options

The VR viewer (`vr/viewer`, built by `vr/viewer/build.sh`) shows the
mission as a miniature city on a table, in a headset through OpenXR. This
document lists every option, its default and what it changes. See
`docs/swview.md` for how the viewer reads the world.

## The two programs

`vr/viewer/build.sh` builds two programs into `build/vr-viewer`:

| Program | Game | Desktop window |
|---|---|---|
| `syndwars-vr.exe` | The whole desktop game, as `syndwarsfx`, on a thread of its own | The original game: its layout, panels, scanner, sound and mouse and keyboard play |
| `syndwars-vr-table.exe` | The game core alone (no renderer, no sound) | The 3D table, with an orbit camera, or a copy of the headset's left eye |

`syndwars-vr.exe` is the one to start (`run_vr.cmd` does). With a
headset, the mission can be played from the desktop window and watched
in VR, played in VR and watched on the desktop, or played from both at
once: the headset's commands go to the same game. It runs the table
program instead, with the same options, for `--desktop`, for
`--companion table`, and when no headset can be used.

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
| (mission end) | | When the mission is over (the desktop game leaves it, or its outcome is confirmed), the viewer quits 3 s later. Menus in VR are for later. |
| `--mission C,M` | `0,1` | Campaign and mission the game core plays. `run_vr.cmd` takes it as its first argument. |
| `--data DIR` | `$SWARS_GAMEDATA`; `run_vr.cmd`: `gamedata` next to the repository | Game data folder (levels, textures, sprites, sounds). The viewer does not start without it. |
| `--turn-ms MS` | `50` (20 turns a second) | Table program only: real time per game turn of the game core. Lower values play faster. The game itself is the same: it does not depend on time, only on turns. The desktop game keeps its own pace. |

## Look

| Option | Default | Impact |
|---|---|---|
| `--look classic\|upgraded` | `classic` | Starting look. `classic`: the game's own sprites for people, items and effects. `upgraded`: the 3D models, animations and particle effects of the assets repository; anything without a model is still drawn as its sprite. Switch at runtime with L (desktop) or B on the right controller. |
| (HUD font) | follows the look | The text of the wrist panel, objective board and world markers. Classic look: the game's own panel font (`data/fontc0-1`), its pixels shaded as in the game. Upgraded look: Share Tech Mono (`vr/viewer/fonts`, SIL Open Font License), drawn from a signed distance field so it stays sharp at any distance and angle. If neither can be loaded, a small built-in bitmap font. |
| `--assets DIR` | `$SWARS_ASSETS3D`, else `assets3d` next to the repository | Assets repository of the upgraded look (`manifest.json`, `models/`, `effects/`). It is only read. Without it, the upgraded look draws nothing different. |

## World shown

| Option | Default | Impact |
|---|---|---|
| (whole world) `--whole-world` | on | The whole map is drawn, around and beyond the table. It costs nothing extra: the whole map is on the GPU anyway. |
| `--table-only` | off | Only the square of the map on the table is drawn, as before. Switch at runtime with C (desktop) or a click of the right stick. Pointing and commands follow what is drawn. |
| `--zoom HALF` | `5120` | Map units from the table's centre to its edge at the start (1024 to 17000). Zoom at runtime with the right stick (up/down) or `+`/`-`, about the point in front of you. |
| (linked views) | on | The headset and the desktop look at the same place (below). A on the right controller or F switches the link off and on. |
| `--center X Z` | off: the views are linked | Map point at the table's centre at the start; the link is off. |

**Linked views.** The desktop's centre of view is its camera point, the
middle of its window. The headset's is the point of the table in front of
you: along your heading, where a gaze 40 degrees down meets the table top
(about 0.6 m ahead at a usual height), kept on the table and following
your heading slowly, so a glance does not move the map. Without a headset
(the table program's window) it is the table's centre.

- At the start, and whenever the table is placed again (left stick
  click), the desktop's camera point is put in front of you and the table
  is turned as the desktop's view: what is at the top of the desktop
  window is away from you.
- While the desktop's camera moves (the player scrolls, the camera follows
  an agent), the table slides so that its point stays in front of you.
- While you move the table (left stick, zoom and turn with the right
  stick), the desktop's camera is sent to the map point in front of you
  (`swcore_camera_look_at()`, as the desktop's scrolling) and glides
  there; the table follows the desktop again 1.2 s after you stop. Zooming
  and turning keep the point in front of you where it is.

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
| `--companion game\|table` | `game` | With a headset: what the desktop window shows. `game`: the original desktop game (`syndwars-vr.exe`). `table`: the 3D table program (`syndwars-vr-table.exe`) mirroring the headset's left eye, with the game core alone. |
| `--desktop` | off | No headset: the 3D table in a desktop window with an orbit camera (`syndwars-vr-table.exe`). Also used when no headset or OpenXR runtime is available. |
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
| `SWARS_VR_NO_HEADSET` | Set to 1: `syndwars-vr.exe` runs the desktop game even without a headset, for tests (instead of the table program). |

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
| Right A | Link the headset's and the desktop's views on/off |
| Right B | Classic / upgraded look |
| Right grip | Fog of war on/off |

Desktop window of `syndwars-vr.exe`: the original game's own controls.

Desktop window of `syndwars-vr-table.exe`:

| Control | Action |
|---|---|
| Left click | As the right trigger |
| Right button (held) | As the left trigger |
| `[` / `]` | Previous / next weapon |
| Mouse drag, wheel | Orbit the camera, its distance |
| Arrows | Move the map |
| `+` / `-` | Zoom |
| Q / E | Turn the table |
| F | Link the views on/off |
| L | Classic / upgraded look |
| C | Whole world / table only |
| G | Fog of war on/off |
| Enter | Confirm the end of a decided mission |
| Esc | Quit |
