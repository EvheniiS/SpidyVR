# Fork changes (EvheniiS/SpidyVR, branch `dev`)

What this fork adds on top of IlyaMez/SpidyVR, kept in one place so it can be
offered upstream. `docs/CHANGELOG.md` stays exactly upstream's; this file is
the fork's changelog. **Newest work first; add each new fork change here.**

- **Base:** upstream `main` at `b5be0e3` (Release Spidy 0.3.4). The fork's
  `main` mirrors upstream; `dev` is rebased on top of it. Synced: 0.2.12
  (`b1597ad`, Oct 11), 0.2.13 (`4dc4908`, Oct 11), 0.3.1 (`33af06c`, Oct 12),
  0.3.2 (`c6b3ab0`, Oct 13), 0.3.4 (`b5be0e3`, Oct 13; adds 0.3.3's Steam Frame
  controller layout and RT reflections off in VR; one conflict, the XR config
  version in `run_game_vr.py`, resolved by keeping the fork's version).
- **See the code:** `git diff upstream/main dev`, or on GitHub
  <https://github.com/IlyaMez/SpidyVR/compare/main...EvheniiS:SpidyVR:dev>.
- Everything is opt-in or off by default except the dive (right stick click,
  a new input), webs leaving from the wrist, and Aim = Hand (see below).

## Offering it upstream (CONTRIBUTING.md, since 0.3.1)

Spidy is GPL-3.0 since 0.3.1 and has a contributor guide. What it means here:

- **Ask first** in #spidy-dev (Discord) or a GitHub issue for anything beyond a
  small fix. Ilya decides; **stability fixes are the most welcome** right now.
- **Changes to how Spidy plays need a yes before the work,** and usually go in
  as a setting that **leaves the default as it is** (as flips did).
- **One feature or fix per pull request**, rebased on the latest `main`;
  **commit subjects written for players** (they become release-note lines);
  say what it changes, how to try it, and **how it was tested** (which
  headset, how it connects; "not played in a headset" is fine). **Leave the
  version number alone.** Large reworks: split into steps that work alone.
- **Shared structs:** change both sides in the same pull request, raise the
  version, run the Python protocol tests.
- **This fork as a custom build:** its source is public (this repo) under the
  GPL-3.0 with the notices kept, and it is clearly modified. If it is ever
  shared as a build: give it its own name (not "Spidy"), support it yourself,
  and on Spidy's Discord post only in #spidy-dev, as a GitHub link (no zips).

Suggested pull requests, smallest and most welcome first:

| # | Pull request | From | Fits as is? |
|---|---|---|---|
| 1 | Never save a crashed start's all-zero VR settings | `fe566dc` | yes: a stability fix |
| 2 | Name the window that pauses the game (foreground log) | `2248215` | yes: diagnostic for a freeze |
| 3 | Dive on the right stick click in the air | `50f98ac` | ask first; new input, on by default |
| 4 | Webs leave from the wrist shooter | `7acc038` | ask first; changes the default look |
| 5 | Aim webs along the hand (Aim: Hand / Controller) | `7af81fb` `4718401` | ask first; as a PR, with **Controller** as the default |
| 6 | Swing model 2.0 (setting, Classic default) | `91afe65` `28740bc` `6070651` `c345f36` `933748e` `645b85d` `830aa3f` | ask first; large: split (elastic web, pump, jump off, boost + momentum; ledger and bench as tooling) |

Each PR branch is cut from `upstream/main` and cherry-picks only its commits;
the protocol versions are renumbered against upstream at that time.

## At a glance

| Change | Commit | Default | Tried in the headset |
|---|---|---|---|
| Dive, as the game has it | `50f98ac` | on (R-stick click in the air) | yes, Oct 10: works |
| Webs leave from the palm-side wrist shooter | `7acc038` | on | built; looks unchecked |
| Aim webs along the hand's line (Aim: Hand / Controller) | `7af81fb` `4718401` | Hand | yes: his everyday aim since Oct 11, works |
| Swing model 2.0 + swing ledger | `91afe65` | Classic (2.0 opt-in) | yes, Oct 11: "felt pretty good" |
| Swing model 2.0, round 2: speed boost, momentum meter | `28740bc` | Classic (2.0 opt-in) | no |
| Swing model 2.0, closer to the game's code (boost, gravity, catch, jump off) | `6070651` `c345f36` `933748e` `645b85d` | Classic (2.0 opt-in) | yes, Oct 13 02:51: feels better |
| Swing model 2.0: reeling in draws you to the web without launching you | `830aa3f` | Classic (2.0 opt-in) | not tried in the headset |
| Don't save a crashed session's all-zero settings | `fe566dc` | — (fix) | — |
| Smoothness probe: new-pair gap percentiles in the run report | `f40cd5b` | — (diagnostic) | no |
| Log which window takes the foreground | `2248215` | — (diagnostic) | yes: caught `msrdc.exe` (WSLg) |

## Each change

**Smoothness probe** (`f40cd5b`, diagnostic). `XrTimingData` v2 adds 0.5 ms histograms of
the gap between consecutive new scene pairs (menus and the first 30 frames
excluded) and of the XR loop's frame time, gaps over 1.5x and 2x the display
period, and the longest run of reused images. `run_game_vr.py` prints p50 / p95
/ p99 / p99.9 / max at the end and stores it under `timing.cadence` in the
report. Read it instead of PyMax / CapFrameX, which watch the game's `Present`,
not the headset submissions.

**Dive** (`50f98ac`). Right stick click in free flight (no web attached;
since 0.2.13 the same click elsewhere stays upstream's LB + RB web pull, and
a click that can start or end a dive no longer also sends LB + RB):
the game's own free fall (26 m/s², terminal 42 m/s rising 8 m/s per second
to 65, the movement module's limit). A web that catches any fall carries it
into the swing by the game's table (fall 6/16/42 m/s → swing limit x1 to
x5/3); extra speed bleeds off at 4 m/s per second once free. Numbers come from
the game's config assets (`docs/REFERENCE.md`, "The game's dive";
`tools/game_configs.py` reads any config asset offline). New
`include/spidy/dive_toggle.hpp`; swing command version 4 (`dive` in the spare
word).

**Webs from the wrist** (`7acc038`). The shot, aim rays and markers, web grab,
web balls and the drawn rope start at `webShooterPoint` (grip pose, 7.5 cm
toward the forearm, 2 cm to the palm side). Punches keep the knuckle point
via `HandInput::fist` (swing command version 5). Adds
`docs/GAME-SWING-RESEARCH.md`, the game's swing parameters.

**Aim along the hand** (`7af81fb`; the launcher passing Aim: Controller on `4718401`). A straight, natural
hand aims a web the way the thwip does: from the wrist straight through the
fist (the grip pose's -y, `handLine`). The controller's pointer leaves its
top by the thumb, which never matched the hand; aiming straight out of the
palm (90°) or turning the pointer toward the palm by 15-45° were tried in the
headset and dropped. Launcher **Aim: Hand** (default) / **Controller** (the
old pointer). `tools/analyze_aim.py` compares gaze with web direction at each
attach.

**Swing model 2.0** (`91afe65`, `28740bc`). Launcher **Swing model: Classic**
(default, the 0.2.11 swing unchanged) / **2.0**, for A/B. 2.0 ports parts of
the game's own swing: an elastic web instead of a rigid snap (the classic
swing loses most speed to snaps, also on webs shot ahead that go slack and
pull taut), a pump (taut-web gravity x1.25 down, x0.6 up), A to jump off a
swing, the game's swing speed boost and a 0-5 momentum meter. The solver keeps
a per-swing **ledger** (what snaps, rope, elastic, pump, yank, reel, jump,
boost and the limit did to the speed); reports carry it as `swings`, and
`tools/analyze_swings.py <report>` prints it, and last the falls with no web
(Spidy's and the game's own: count, length, height, fastest going down, how
many hit the speed limit). `spidy_swing_bench` (new) runs canonical swings
headless in both models.

**Swing model 2.0, closer to the game's code** (`6070651`, `c345f36`,
`933748e`, `645b85d`; Oct 13; tried Oct 13 02:51: feels better). He found that 2.0 "throws me up higher than it feels like I
should". `docs/GAME-SWING-RESEARCH.md` "Oct 13 2026 dig" read the game's real
swing from `Spider-Man.exe`; 2.0 now follows it instead of the first round's
guesses. Only 2.0 changes; Classic and upstream's physical webs are as they
were (`spidy_swing_bench` prints the same Classic lines). The game's numbers
are kept as the game's (m/s under its 26 m/s² free fall, `gameFall`) and
scaled to the player's gravity for equal heights: speeds x sqrt(g / 26),
0.61 at weight 100. `spidy_swing_bench` now prints **apex** (top of the swing
and flight above where it began) and **throw** (rise after letting go).

- **Speed boost:** the game's is an absolute target (SpeedBoostTarget 10 m/s
  = 6.1 m/s here), only from a web's attaching to the bottom of its arc and
  only while slower than that; 8 → 20 m/s² at 120 m/s³. It lifts a slow
  start and leaves swinging speeds alone. The first round added 8 m/s over
  the attach speed on every web. The 12 m height rule is gone: it copied
  `SpeedBoostAltMin`, a field no type the game loads has.
  Bench (9.81, limit 40), throw after letting go at 30° up: swing at 20 m/s
  10.8 → 6.7 m, caught 30 m/s fall 20.1 → 13.2 m (Classic 3.0 / 7.8).
- **Swing gravity** (the game's `0xac0b60`, as shares of its 26 m/s² free
  fall, `swingCurve`): way down 24/26 = 0.92 g (was 1.25 g), half that for
  the first 0.25 s after a web attaches, full by 0.5 s (a soft catch); way
  up the game's curve by velocity pitch (to 30° / from 70°), speed (game
  16-40 / 12-30 m/s, scaled) and rope length (under 16 m heavier, toward 35;
  28-55 m down to half); swinging back from the top 14 / 22 of 26 (0.54 /
  0.85 g). High momentum (meter 4+) halves only the way up past the bottom of
  the arc, not the way down. The game's upswing is light with the stick held
  forward along the way (2-11 of 26) and heavy without (28-45); **no stick
  touches the swing here** (the left stick flips; you steer with webs), so a
  fixed `swingIntent` stands in for it. Bench, swing held to its top, height
  above where it began (level at 20 m/s / falling at 20 m/s; Classic +6.9 /
  +13.0; 2.0 before +29.8 / +33.4): intent 0 +16.5 / +11.4, 0.3 +21.7 /
  +17.2, **0.4 +24.2 / +20.0**, 0.5 +26.4 / +22.4, 0.7 +30.6 / +27.6, 1
  +37.5 / +36.6. 0.4 is about a pendulum's own height (pump +4.6 / -1.0 m):
  no lift out of nowhere, and still far above Classic, which loses its speed
  to snaps. Swings let go at 30° barely change with it (throw at 20 m/s
  6.5-6.9 m for any intent): their height comes from the speed kept, the
  sling. Left out: the sideways-rope (line tilt) blend to 12 / 20, the
  support floor for close side anchors, the swing-back drag, the past-145°
  blend to 50 and the swing point's flag bit (unknown meaning).
- **Catch:** no bounce (`catchBounce` 0.4 → 0). The game's rope never
  springs the body back toward its anchor; 2.0's flung it in at 40% of the
  caught speed (8.7 m/s after a 20 m/s fall, now 0.5 m/s as the web reaches
  its length). The sling stays: the caught speed still goes on along the
  swing (`catchKeep` 1), as the game keeps it (its rope only bends the
  velocity). Unbounced, the spring is nearly critically damped and stiffer,
  so `catchFrequency` 1 → 0.6 Hz keeps the give it had (1.6 m deepest at a
  30 m/s catch, reached at 0.27 s, near the game's 0.33 s of rope it does
  not hold to its length; 1 Hz would give 0.9 m at 0.16 s). The web comes
  back over about a second, carrying the body in at up to 3 m/s on the way.
  Bench: no snaps; a 10 m/s swing held to its top now really swings (+11.5 m;
  the bounce used to stop it on the catch).
- **Jump off** (A with a web attached; the game's release `0xac17aa`): bands
  by velocity pitch at 7° and 45° (were 0° / 25° / 50° with a blend); speed
  up = max(v.y + JumpBoost 8 / 20 / 24, VertFloorJump 12 / 22 / 30) in game
  m/s, then set to rise as high here as it does under the game's heavier
  gravity after a jump (20 / 38 / 45 m/s²): about +5.6 / +10.2 / +11.2 m/s
  added, floors 8.4 / 11.2 / 14.0 m/s. Along the level JumpBoostXZ 1.5 (0.9
  here), ramped in from 10° to 30° and out from 35° to 55° (was +4 / 9 / 11
  m/s). The stick no longer picks the way of a jump going straight up. The
  gains need the web held 0.5 s (`jumpOffSwing`, the soft catch's end); a
  jump off a web just shot lifts only to the floor: added on any web, grip
  and A together climbed 559 m in the bench's 20 s. Bench: a jump off a
  10 m/s swing at 30° now throws 13.7 m (was 5.6), at 50° 15.7 m (was 8.5):
  the game's jump is that strong; `jumpOffUp` is the knob if it is too much.
  20 s of jumping off: highest 74 m (was 86), mean speed 20.5 m/s (was 32.6).
  Left out: the Fall band's horizontal damping, the skill-gated extra rise,
  the release (no jump) floors and the jump redirect zones.

**Swing model 2.0: a gentler reel** (`830aa3f`; Oct 13; not tried in
the headset). Oct 13 02:51 (limit 40, weight 100): the trigger "pulls you
through the web upward... too aggressive". 196 reels, each +23 m of height
(speed and height) median, +52 m at p90, ending at +12 m/s up (p90 +31). A
winch shortening a taut rope spins the body up round its anchor as a skater
pulling her arms in: by speed² / rope x 16 m/s, 130 m of height a second at
40 m/s on 20 m. Estimated from the samples, that spin was 30 m a reel median
(64 at p90); the rest is the winch's own 16 m/s toward the anchor (up to 13 m
a reel) and the lift against gravity. The ledger's `reel` was the winch: the
hand pulling on its rope (physical webs) was at most ~1000 m of its 17962 m
(an upper bound, any hand motion counted as a pull).

2.0 now limits only the spin (`reelSpin` 0.7 g = 6.9 m/s² at weight 100; the
spin measured each step as the last step's draw turned along the swing): the
winch still winds in at 16 m/s, draws the body in at that speed and lifts it,
a reel from a hang or below 0.43 x rope m/s of swing (8.6 m/s on 20 m) is as
before, letting go keeps the speed, and the spin adds nothing above the speed
limit. The hand's pull is not limited. The ledger's `reel` is net of the
limit (no protocol change). On the session's reels this would leave ~11 m of
spin median (24 at p90). Classic is unchanged (same bench lines).
Bench (9.81, limit 40), reel held through the bottom until rising at 30°,
throw after letting go, before → after (unreeled): 20 m/s 14.7 → 8.8 m
(5.4), out 33.6 → 26.3 m/s; 30 m/s 22.7 → 15.7 m (11.1), out 40 → 34.5;
40 m/s 21.8 → 21.9 (17.3, at the speed limit throughout); from rest 5.4 →
5.4. Limit 32: 20 m/s 14.2 → 8.8 m. Test next: does a reel at speed still
feel like a pull (not a brake), and is it "less steep"; if not, the
remaining lift is the winch's 16 m/s toward the anchor (a 2.0 reel speed
would be the knob), or `reelSpin` lower (0.5 g: ~8 m a reel).

**All-zero settings guard** (`fe566dc`). A game that closed before VR read its
settings reported zeros; the launcher clamped them to every minimum (swing
speed 10, weight 40, snap turn and haptics off) and saved them. The launcher
now refuses such a line.

**Foreground log** (`2248215`). The game pauses itself when another window is
in front (camera, player and pad reads all stop; VR falls to the flat
screen). The log now names the window (`Focus: <exe> '<title>'`) and the
report counts `focus_changes`.

## Protocol and settings changes (matter when merging)

- **XR config version 26, 672 bytes.** Upstream 0.2.13 is version 23,
  664 bytes (`upscale`, `monitorRate`, then `screenDepth`, `spare`). The fork
  appends `controllerAim` and `swingModel` after them. Option bits are
  upstream's unchanged (up to `0x80000`, no physical webs); the fork uses none.
- **Swing command version 6, 224 bytes.** Upstream 0.3.2 is version 4,
  200 bytes (`leap`, `look`). Fork version 5 = upstream's layout with `dive`
  in its `reserved` word; version 6 appends `fists[2]`. Versions 2-5 still
  work (2-4 with no dive, 2-5 punching at the aim's point).
- **Swing settings version 6, 44 bytes.** Upstream 0.3.2 is version 5,
  36 bytes (`physical`). The fork appends `model`, `reserved`.
- **Swing data version 6** (392 bytes: ledger, momentum); upstream's is
  unchanged at version 4, 272 bytes. The probes' older versions still work.
- **launcher.ini:** `controller_aim`, `swing_model`.
  **run_game_vr.py:** `--controller-aim`, `--swing-model`.
- New files: `apps/swing_bench.cpp`, `include/spidy/dive_toggle.hpp`,
  `docs/GAME-SWING-RESEARCH.md`, `tools/analyze_aim.py`,
  `tools/analyze_swings.py`, `tools/game_configs.py`.
- Rebasing onto 0.2.12 collided twice in the XR config (both sides grew it
  648 → 656 and both used the option bit `0x8000`); resolved by appending the
  fork's fields after upstream's and moving the fork off option bits.
- Rebasing onto 0.2.13: the XR config again (upstream 656 → 664 with
  `screenDepth`; the fork's words now follow it), and the **right stick
  click**, which 0.2.13 made the game's LB + RB (web pull) and the fork uses
  for the dive. `game_xr.cpp` `diveOwnsStick`: in free flight with no web, or
  during a dive, the click is the dive's; everywhere else LB + RB.
- Rebasing onto 0.3.1: no XR config change upstream. 0.3.1 put heal and
  LT + RT on the right stick **pushed** down / up; the dive stays on its
  **click**, so they don't collide (the VR help line names all three). 0.3.0's
  combat rewrote `game_punch.cpp`; punches still take the knuckle point
  (`fistPoint`), and a web strike keeps its own speed while every other
  movement command is capped at the dive-aware `solver.speedLimit()`.
- Rebasing onto 0.3.2 (physical webs, Charge Jump, one enemy at a time):
  upstream took swing command version 4 and swing settings version 5, both
  of which the fork had used; the fork's moved up past them (above). Its
  `motionSpeedLimit` (65) duplicated the fork's; one is kept. **Physical webs
  (on by default) measure a rope to the hand (`Swing::hold`)**, so swing
  2.0's elastic stretch, pump (`hung`), sling and reel-follow now measure to
  `hold(i)` too; with the anchor they would have read the arm's reach as
  stretch. A web's attach and reel keep both sides (upstream's length to the
  hand, 2.0's stretch reset), and a rope's pull adds 2.0's `give` to the
  hand's own rate. Option bit `0x80000` is now upstream's (no physical
  webs). `spidy_swing_bench` gives the same numbers as before (it runs
  without physical webs); **swing 2.0 with physical webs is unplayed.**

## Full build notes (moved from the fork's CHANGELOG entries)

**Build (October 11, branch `dev`): swing model 2.0, second round:
the swing's speed boost and the momentum meter.** From the first headset
session on 2.0 (357 swings, limit 32, weight 80, `tools/analyze_swings.py`):
fall catches kept at least 78% of their speed (classic's worst: 30-39%), but
the pump gave a median 0.4 m of height a swing (swings lasted 0.66 s, 90% had
a yank) and 48% of swings left pinned at the speed limit. Only 2.0 changes;
Classic is the swing it was.

- **The swing's speed boost** (the game's SpeedBoost: 8 m/s at 20 m/s²,
  ramping in at 120 m/s³, not within 12 m of the ground). On a taut web's way
  down and through the bottom of its arc the swing speeds up, until it goes
  8 m/s faster than when that web attached. A swing without a yank now pays.
- **The momentum meter** (the game's MomentumConfig), 0 to 5: swinging
  raises it a little, letting go on the way up 0.25, a jump off 1, a yank 0.5
  (once in 2 s), a dive a web catches 0.5-1.25; it falls away in the air
  0.75-1.5 s after the last rise, and on the ground after 3 s. At 4
  (both hands feel a light pulse) the swing's speed limit is 7/6 of yours
  and a taut web's gravity half as strong: faster, floatier swings while you
  keep the chain going. Its clock runs 1.5 times slower than the game's, as
  real gravity's swings take that much longer.
- **Fixes.** Reeling while standing wound the rope in although the ground
  held you (9 m in that session), and the take-off pulled it all in at once
  at the speed limit; 2.0's winch now waits for the body. The elastic web
  stretches 3 m at most (it was 6-7 m at 65 m/s, a bungee): a faster catch
  is a stiffer one.
- **The swing ledger** books a winch's work as `reel` even while its rope
  stretches (the session's +9068 m of "elastic" was mostly reel), counts
  snaps only for rigid ropes (2.0's 134 were the spring at work), skips steps
  on the ground, and adds `boost` and the momentum as each swing ended.
  What the speed limit trims from a catch's sling is booked as `cap` (it
  showed as a negative sling), and a winch catching up with the body is no
  snap (it showed as snaps that took nothing). Swing
  data version 6 (392 bytes: `surges`, the meter now).

Measured by `spidy_swing_bench 9.81 40 game` (new `game`: frames as the game
runs them; it matches the solver's own clock): 20 s of swings averaged 13.0
m/s (Classic), 20.5 (2.0) and 34.0 (2.0 jumping off, the meter at 3.8 by the
end); a web shot ahead at 20 m/s left at 15.7 (Classic) and 27.9 m/s (2.0).
6 new core checks; 248 core checks, the launcher checks and 103 Python checks
pass.

**Build (October 10, branch `dev`): swing model 2.0, to try against the
classic swing.** The launcher's new **Swing model** (Classic by default, which
is the swing exactly as before) picks **2.0**, which adds three things the
game's own swing has (read from its config assets, see
[GAME-SWING-RESEARCH.md](GAME-SWING-RESEARCH.md)) and the classic swing lacks:

- **An elastic web.** A rope that pulls taut while you move away from its
  anchor (a web catching a fall, or a web shot ahead that went slack as you
  flew toward it and pulls taut past it) no longer cuts that speed in one
  step. It stretches like an elastic band, a quarter second to its deepest
  (the game's sling time; 1-2 m at 15-30 m/s), springs back to its length and
  flings you toward the anchor at 40% of the speed it caught. The rest of that
  speed goes on along the swing instead of being lost.
- **A swing that pays.** While a taut web holds you up, gravity is 1.25 times
  as strong on the way down and 0.6 times on the way up (the game: 24 m/s² down,
  2-14 up), so every swing leaves faster than it came in.
- **A jump off the swing.** A with a web attached in the air lets go of every
  web and leaps: up at 6, 10 or 13 m/s at least and 4, 9 or 11 m/s faster along
  your way, the higher up the arc the more (half the game's numbers; both
  hands feel it). In Classic, A still does nothing on a web.

Measured without a headset by the new `spidy_swing_bench` (the game's solver,
real gravity, a 40 m/s limit, webs 45° up ahead): a web shot ahead at 20 m/s
left the classic swing at 15.7 m/s, one snap taking 13.4 m of height's worth
of speed, and the 2.0 swing at 23.0 with no snap; a 30 m/s fall caught by a
web left at 24.8 (classic) and 32.3 m/s (2.0); 20 s of swings averaged 13.0
(classic), 18.8 (2.0) and 32.3 m/s (2.0, jumping off each). Your own last
three sessions (476 swings in the air, `tools/analyze_swings.py`): you yanked
on 90% of webs; swings without a yank lost 1-6 m of height's worth of speed,
and the worst catches cut 10-13 m/s at once.

New: **the swing ledger.** The solver keeps an exact budget of every swing (what
the ropes cut and in how many snaps, what elastic catches kept, the pump, the
yanks, the reel, the jump, the stick, the speed limit) and the session report
keeps one entry per swing (`swings`, `swing_model`); `tools/analyze_swings.py
<report>` prints them, and estimates the same from the swing samples of older
reports. Swing data version 5, swing settings version 5 (`model`; the probes'
versions 3 and 4 still work), XR config version 24 (`swingModel` in the spare
word after `palmAim`); `--swing-model 1`, `swing_model` in `launcher.ini`.
6 new core checks (classic is unchanged, a rigid snap against an elastic
catch, the stretch springs back, the ledger adds up, the jump off in each
model, the pump); 242 core checks, the launcher checks and 103 Python checks
pass. Not yet tried in the game.

**Build (October 10, branch `dive`): a dive, as the game has it.** Click
the **right thumbstick** in the air with no web attached and Spider-Man dives:
he falls under the game's own free-fall gravity, 26 m/s² (or your WEIGHT, if
heavier), and the speed limit lifts to the game's terminal velocity, 42 m/s,
then climbs 8 m/s each second to 65 m/s, the movement module's limit (the game
allows 100). A pulse in both hands marks the start. Another click, a web or
landing ends it. A web that catches the dive keeps its speed: as in the game,
a swing's speed limit rises with how fast you fall when the web attaches (the
game: 24 m/s horizontal from a 6 m/s fall, 27.25 from 16, 40 from 42 or
faster), so here SWING SPEED LIMIT x1 up to a 6 m/s fall and up to x5/3 from
42. This applies to every fall a web catches, dive or not. Once no web holds
you, the extra speed bleeds off at 4 m/s each second. Numbers are the game's
own, read from its config assets (`hero_swingsetup_standard.config`,
`herotraversalconfigsunsetoverdrive.config`; see
[REFERENCE.md](REFERENCE.md#the-games-dive)). Swing command version 4: the
spare word is now `dive` (version 3 and the probes' version 2 still work).
5 new core checks (the dive's gravity and rising limit, a web ending it and
carrying its fall, the carry table, the bleed, the toggle); 217 core checks,
the launcher checks and 94 Python checks pass. Not yet tried in the game.
