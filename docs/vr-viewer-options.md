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
| (mission end) | | The whole game (no `--mission`): the game goes on to its debriefing and menus, on the screen panel. With `--mission`: when the mission is over (the desktop game leaves it, or its outcome is confirmed), the viewer quits 3 s later. |
| `--mission C,M` | none: the whole game | `syndwars-vr.exe`: without it the whole game runs, its intro and menus first (below); with it the game starts straight in that mission. The table program always plays a mission (`0,1` by default). `run_vr.cmd` takes it as its first argument. |
| `--screen-size W` | `1.7` | `syndwars-vr.exe`, whole game: the width in metres (0.5 to 5) of the flat screen showing the game's intro, menus and briefings. |
| `--data DIR` | `$SWARS_GAMEDATA`; `run_vr.cmd`: `gamedata` next to the repository | Game data folder (levels, textures, sprites, sounds). The viewer does not start without it. |
| `--turn-ms MS` | `50` (20 turns a second) | Table program only: real time per game turn of the game core. Lower values play faster. The game itself is the same: it does not depend on time, only on turns. The desktop game keeps its own pace. |

### The whole game

Without `--mission`, `syndwars-vr.exe` runs the game as `syndwarsfx` does:
the intro, the menus, the briefings, the missions the player starts. Outside
a mission the headset shows the game's own screen on a flat panel standing
above the far half of the table's place, facing you (1.7 m wide by default,
`--screen-size`; fixed to the table's place: it does not follow the head),
in a black void: the table and the map are not drawn meanwhile. One hand
points at it as the mouse, the active one, shown by a blue beam and a round
dot on the screen: the right hand at first; a trigger of the left hand makes
it the active one (that press is not a click), and the other way round. The
active hand's bottom trigger (Steam Frame: R2 or L2) is the left button, its
top one (R1 or L1) the right one. In missions both hands keep their beams
and roles (the left one shoots). The keyboard
of the desktop works as usual. When a mission starts the panel goes and the
table shows it; at its end the panel comes back.

While a text box of the game has the focus (the login name, a save slot's
name: the game's `edit_flag`) a keyboard lies over the near half of the
table, tilted towards you (`vr/viewer/vkeyboard.h`): digits, letters, `-`,
`.`, space, backspace and enter. Point at a key with the right hand, its
the active hand's bottom trigger types it; the key goes into bflib's keyboard as a real key press
(`keyboardControl()`, held a tenth of a second, as the game reads
backspace while held).

The frame comes from bflib's screen swap (`lbScreenSwapHook`, with the
pointer drawn), the pointer's moves and clicks go in through
`LbMouseVirtualEvent()` from an idle handler of bflib's event loop
(registered at the first frame, bflib's set up clears them), so they work in
the intro's and the menus' own loops too. They are absolute places on the
game's screen, not moves of the OS mouse (`MouseToScreen()` takes those as
relative). The trigger skips the intro too (before the game's mouse is set
up only the button's state is set, which the videos check).

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
| `--no-desktop-frame` | off: the frame is shown | Hides the yellow outline of what the desktop's window shows now (`--desktop-frame` shows it). The outline follows the desktop's camera, zoom, window size and projection (perspective or isometric), so with perspective it is wider at its far side. |

**Linked views.** The desktop's centre of view is its camera point, the
middle of its window. The headset's is the point of the table in front of
you: along your heading, where a gaze 40 degrees down meets the table top
(about 0.6 m ahead at a usual height), kept on the table. It and the
direction are taken once, when the table is placed in front of you: at
the start and after SteamVR's reset view along the play area's forward,
with a left stick click along the way you face. The table's far edge is
the top of the desktop window, and the views are lined up again (even
when unlinked). After that, turning your head or body changes neither
the desktop's view nor its yellow outline on the table.
Without a headset (the table program's window) it is the table's centre.

- The desktop's camera looks the way you did when the table was placed:
  the top of the desktop window lies along that heading (from your eyes
  to the point in front of you), and the desktop's camera point is put in
  front of you.
- The view turns only with the turn controls (right stick left/right, Q/E
  on the desktop) or when the desktop's camera turns.
- While the desktop's camera moves (the player scrolls, the camera follows
  an agent) or turns, the table slides and turns with it, so that its
  point stays in front of you and the top of the desktop window away from
  you.
- While you move the table (left stick, zoom and turn with the right
  stick), the desktop's camera is sent to the map point in front of you
  (`swcore_camera_look_at()`, as the desktop's scrolling) and glides
  there, and turned as the table (`swcore_camera_set_angle()`); the table
  follows the desktop again 1.2 s after you stop. Zooming and turning keep
  the point in front of you where it is.
- The zooms are linked too. When the views are lined up, the viewer
  notes the ratio of the table's size to the ground the desktop's window
  covers; zooming the table (right stick up/down) then zooms the desktop
  in the same ratio (`swcore_camera_set_zoom()`), within what the game
  allows at that moment: from 256 (closest) out to the selected agent's
  weapon's limit (a short-range weapon, or none, keeps the desktop fully
  zoomed in; the furthest is about 150). The table zooms on beyond that
  range. When the desktop zooms (its own keys, or a weapon change), the
  table follows in the same ratio. The yellow outline shows the desktop's
  zoom as it is.
- The desktop draws in perspective (an option of the game): lines running
  up its window converge, so a street straight up the map looks slanted
  there and straight on the table. The yellow outline shows the area
  exactly.

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

## The wrist panels

The left wrist (beside the table in the table program's window) shows the
desktop game's own agent panels: the same sprites (`data/pop1-1`) and
layout (`conf/pop1-1.ini`), the four agents' panels stacked top down, the
energy column and grouping pane to their left (`vr/viewer/hud_panels.h`).
Point at them with the right hand. The click is the right hand's main
trigger (Steam Frame: R2, the bottom one); the right click is R1 (the top
one) on the Steam Frame, the
left trigger on other controllers while the right hand points at the
panels, the right mouse button in the table program. They do what the
desktop's do:

| Element | Click | Right click |
|---|---|---|
| Badge, health bar, energy mini bar | Select the agent | |
| Mood bar (red: faster, blue: slower) | Set the agent's mood where pointed; hold and move to change it | The same for the whole group |
| Weapon button | Open the agent's weapon list; release over a weapon to draw it, over the button to draw or put away the shown one | The list for the whole group; release on a weapon |
| Weapon list, while open | Press the other button over a weapon: drop it | |
| Medikit | Use it (when carried) | |
| Energy column | Supershield on/off (when possible) | |
| Grouping pane | One more agent protects the selected one; its thermal box: thermal view on/off | All agents protect it, or none |
| Scanner (minimap, right of the agents) | The selected agent walks to the point clicked | |

The scanner is the desktop's: its map (`SCANNER_data`: floor, roads, water,
building outlines), centred on the desktop's camera point, turned with its
view, at its zoom; people as dots in its colours (the player's agents
pulsing, persuaded people yellow), vehicles, and the objective's signals:
a blinking mark on each place and a circle starting there every 32 turns
and sweeping across the whole scanner, darkening as it grows, in the
game's colours (red for a target). Its map, and the signals' turn and
colours, come from the desktop game; with the game core alone (the table
program) its background is plain and the circles are red on the viewer's
own clock.

If the sprites or layout cannot be loaded, the panel falls back to rows
of text.

## Environment variables

| Variable | Impact |
|---|---|
| `SWARS_GAMEDATA` | Game data folder when `--data` is not given. |
| `SWARS_ASSETS3D` | Assets repository when `--assets` is not given. |
| `SWARS_VR_MOUSE_TEST` | A file: `syndwars-vr.exe` (whole game) presses the headset's mouse button at 12 s (skipping the intro), clicks LOGIN at 17 s, types V and R with the headset's keys at 19 s, moves its pointer to a quarter of the screen at 20 s and saves the game's screen into the file (PPM) at 23 s, to check the pointer without a headset. |
| `SWARS_VR_NO_HEADSET` | Set to 1: `syndwars-vr.exe` runs the desktop game even without a headset, for tests (instead of the table program). |
| `SWARS_VR_DUMP_PANELS` | A folder: the wrist panels are drawn flat once (turn 100 of a mission), a unit per pixel of the 640x480 layout, into `verts.bin` (triangles: x, y, u, v, r, g, b, kind, 0 as floats) and `page.rgba` (the sprite page, 1024 square), to compare with a capture of the desktop. |

## Controls

VR (Valve Index, Oculus Touch):

| Control | Action |
|---|---|
| Right trigger (Steam Frame: R2, the bottom one) | Point at an agent to select it, at the ground to walk there, at a vehicle to get in, at a row of the wrist panel to pick an agent or weapon |
| Left trigger | Shoot at what the left hand points at; held, weapons fire on and charging weapons charge |
| Left A / B (Touch: X / Y) | Previous / next weapon |
| Left stick | Move the view the way the stick is pushed, as you see it (up: on towards what is away from you; the map slides towards you), whatever the table's turn |
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
