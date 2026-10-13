# The game's swing, and what Spidy's swing lacks

Researched October 10 2026 from the game's config assets (build 23986256), read offline with
`python tools/game_configs.py dump <path>`. Field names and values are the game's; what each one *does*
is inferred from its name and its neighbours (the formulas are compiled into the exe and were not read).
Inferences are marked (inf.). Problem being solved: "swinging feels less rewarding than zipping; they feel too separate."

Assets (all under `configs/`):

| Asset | Holds |
|---|---|
| `hero/hero_swingconfig.config` | `MomentumConfig` (the cross-move momentum meter), `InputBoost`, `JumpBankZones`, `JumpRedirectZones`, release angles, and Standard/Ground/Indoor/Fast copies of the motion/release/search params |
| `hero/hero_swingsetup_standard.config` (also `_openarena`, `_ground`, `_indoor`, ...) | per-context `MotionParams` (Gravity/Speed/Turn), `ReleaseParams`, `SearchParams` |
| `herotraversalconfigsunsetoverdrive/herotraversalconfigsunsetoverdrive.config` | free fall, `WebZipAirOutdoor` (zip + re-zip chain), `PointLaunchConfig`, `SlingDataAir`, `JumpConfigs` (`SwingJumpConfig`, drag profiles) |
| `hero/hero_webzipsetup_outdoor.config`, `_ground`, `_swingfail` | zip setups |
| `hero/test/test_hero_swingconfig.config` | an older tuning; the only place `MomentumConfig/MaxMomentum` is set (1) |

Correction to REFERENCE.md "The game's dive": the momentum `FallData` fields ARE set by an asset
(`hero_swingconfig.config` `MomentumConfig/FallData`), see section 5.

## 1. Swing gravity is asymmetric (the game pumps energy every swing)

`hero_swingsetup_standard.config` `MotionParams/GravityParams` (m/s², negative = down):

| Phase | Fields | Value |
|---|---|---|
| Downswing | `GravityFallInit`, `GravityFallTurn`, `GravityFallHold` | -24, -24, -15 |
| Upswing, stick held (inf.) | `RiseGravityLowMin/Max`, `RiseGravityHighMin/Max`, `...Tilt` | -2/-4, -6/-11 (-14 in `hero_swingconfig` Standard), tilt -12/-20 |
| Upswing, no stick (inf. "Zero") | `RiseGravityLowMinZero/MaxZero`, `RiseGravityHighMinZero/MaxZero` | -28/-40, -30/-45 |
| Blend inputs | `RiseLowSpeedMin/Max`, `RiseHighSpeedMin/Max`, `RisePitchAngleMin/Max`, `RiseTiltAngleMin/Max`, `RiseLineLengthMin/Max`, `RiseLineLengthScale` | 16-40 m/s, 12-30 m/s, 30-70°, 25-60°, 28-55 m, 0.5 |

Open arena makes the rise gravity *positive* (+10/+20, +0/+10): the swing lifts you.
Indoor/Fast: fall -40, rise -12..-30.

Effect (25 m rope, attached 45° ahead, rise gravity 6, entry speed x 0.83 `InSpeedCarryover`):

| Entry speed | Spidy bottom / exit (g 9.81, energy conserved) | Game bottom / exit |
|---|---|---|
| 0 m/s | 12.0 / 0 | 18.7 / 16.2 |
| 10 m/s | 15.6 / 10 | 20.5 / 18.2 |
| 20 m/s | 23.3 / 20 | 25.0 / 23.2 |

Spidy's swing can never leave faster than it entered (minus the 4 m/s² stick); the game's always does,
before any boost (sections 2-3). Spidy's yank adds up to 18 m/s in one gesture (`physicsConfig`:
`maxZipImpulse` 18, `zipMultiplier` 4.5), the same as a 16.5 m drop at g 9.81: one yank is worth more than a whole swing.

## 2. Swing speed boost and terminal speeds

`MotionParams/SpeedParams` (standard setup; `hero_swingconfig` Standard in brackets):

| Field | Value | Meaning (inf.) |
|---|---|---|
| `SpeedBoostTarget` | 10 [8]; open arena 20 | speed added over a swing |
| `SpeedBoostAccel`, `SpeedBoostAccelAccel` | 20 m/s², 120 m/s³ | how fast it is added (ramped) |
| `SpeedBoostBonusBase`, `SpeedBoostBonusTurn` | [6, 6] | extra target, extra on turning swings |
| `SpeedBoostAltMin` | [12 m] | no boost below 12 m above ground |
| `InSpeedCarryover` | 0.83 [0.85] | share of the incoming speed the swing starts with |
| `FallSpeedThresholdMin/Mid/Max` → `TerminalVelocityHorzMin/Mid/Max` | 6/16/42 → 24/27.25/40 [8/18/42 → 24/27/44] | horizontal cap from the caught fall (already ported: `fallCarry`) |
| `TerminalVelocityMinFloor`, `...MaxFloor`, `...PureMax` | 18, 20, 80 [60] | cap floors and absolute cap |

`hero_swingconfig` `InputBoost` (the stick during a swing): `ActiveAcceleration` 8 m/s², `AccelerationTime` 0.4 s,
`MaxSpeedAcceleration` 14 m/s² between `MaxSpeedAccelMinSpeed` 24 and `MaxSpeedAccelMaxSpeed` 32 m/s,
`GravityScaleMax` 0.3, `GravityFactorSpeed` 5. Spidy: `airAcceleration` 4 m/s², any direction.

## 3. Release by swing phase

`ReleaseParams` (standard) by the velocity pitch at release: Low band `ReleaseAngleLowStart/Final` -10..7°,
High band `ReleaseAngleHighStart/Final` 45..70°, `GravityAngleMid` 35°:

| Band | `VertFloorBase` (let go) | `VertFloorJump` (jump off) | `JumpBoost` | `TimeToPeakStandard/Jump` | Gravity after: release / jump |
|---|---|---|---|---|---|
| Low | -30 | 12 | 8 | 0.4 / 0.5 s | 13 / 20 |
| Middle | 0 | 22 | 20 | 0.55 / 0.75 s | 23 / 38 |
| High | 8 | 30 | 24 | 0.75 / 1.05 s | 26 / 45 |
| Fall (released while falling) | -30 | 12 | 8 | 0.4 / 0.5 s | |

Units m/s (floors: the vertical speed is raised to at least this) and m/s². Also: `JumpBoostXZ` 1.5,
`ReleaseBoostXZ` 1, `ReleaseBoostAltMin` 6 m, `JumpBoostPitchLow` 10-30°, `JumpBoostPitchHigh` 35-55°.
`JumpBankZones`: rope angle 75-85°, `MaxJumpDelay` 0.15 s (a jump pressed early is held for the sweet spot).
`JumpRedirectZones`: rope angle 100-110 / 120-135 / 137-145° → `ExitBias` 0.6 / 1.5 / 0.8.
After the jump: `JumpConfigs/SwingJumpConfig` (height 1.5 m, peak 0.25 s, drag `TraversalJumpDrag`:
forward drag 2→10 m/s² from 28→36 m/s, no-input drag 5, turn drag 12).

Spidy: letting go keeps the velocity; the jump button does nothing in the air with a web attached.

## 4. Entry: the sling

`SlingDataAir/StandardData`: `Duration` 0.25 s, `CarryFallSpeedMin/Max` 8-35 m/s → `CarryFallDeltMaxMin/Max`
0.25-6 m, `MaxCarryFall` 1, `MaxCarryRise` 12; `FallDataReZip` allows 12-35 m. (inf.) For 0.25 s after a web catches
a fall, the rope may stretch/give so the fall's speed turns into the swing instead of being cut.

Spidy: `shot()` sets the rope to the current distance, so it is taut at once and the tension-only constraint deletes the
outward radial velocity on the first step. Falling straight down onto a web anchored 45° ahead keeps only 71%
(20 → 14.1 m/s). `fallCarry` raises the limit but cannot restore speed already cut.

## 5. The momentum meter (what makes chaining build speed)

`hero_swingconfig` `MomentumConfig`. A 0-5 meter fed by every traversal move, spent as swing stages:

| Source | Fields | Value |
|---|---|---|
| Swinging | `SwingData/RateList` (rate by rope angle), `MinTime`/`MaxTime`, `BoostCap` | 0.125 @20°, 0.2 @90°, (@120°); 0.15/0.33 s; cap 4 |
| Let go | `SwingReleaseData/ReleaseData` `Boost`, `BoostCap`, `RetriggerDelay` | +0.25, cap 3, 1 s |
| Jump off | `SwingReleaseData/JumpData` | +1, cap 5, 1 s |
| Release angle scale | `SwingReleaseData/RateScaleList` (Angle→Scale) | 90°→1, 100°→0, 120°→(default), 160°→0 |
| Zip / web zip | `ZipData`, `WebZipData/FullBoostData` | cap 5, retrigger 2 s |
| Dive | `FallData` `DiveTimeMin/Max/Bias`, `DiveGainMin/Max`, `DiveBonusMax` | 0.5-2 s, bias 1.25, +0.5..1.25, bonus 2 |
| Fall | `FallData` `FallTimeMin/Max`, `FallGainMin`, `FallMomentumCapMax` | 1-4 s, +0.33, cap 5 |
| Sprint jump, tunnel | `SprintJumpData`, `TunnelData` | +1.5 (cap 1.5), +2 (cap 4) |
| Back swing | `BackSwingData/MomentumCap` | meter held to 0.5 |
| Decay in air | `DecayDataAir[0..2]` (from momentum, delay, rate/s) | 0: 1 s, 0.25/s; 3.5: 0.75 s, 0.66/s; 5: 0.5 s, 1/s |
| Decay stopped / ground | `DecayDataStop`, `DecayDataGround` | 2 s then 2/s; 2 s |

Stages (`MomentumStages`): 0 (below 1) pivot 0.95→0.75; **1 (≥1)**: horizontal cap floor 24, gravity x1;
**2 (≥4)**: horizontal cap floor 28 (+1.8/3.6/5.5 with skill `SkillOffsetForHorzFloor`), **gravity x0.5**,
rope +4 m (`LineLengthTweak`), angle +3°, pivot 0.9→0.6 (`TruePivotFactorStart/Trough/Final`; inf. the swing
pivots about a point 60-95% up the rope, a tighter, faster trough at higher momentum).

## 6. Zip and its chain

`WebZipAirOutdoor` (traversal config):

| Field | Value |
|---|---|
| `OutSpeed`, `OutSpeedMin`, `OutSpeedFloor` (`Forward`, `Left`) | 30, 22, 20 (22, 20) m/s |
| `SpeedMin/Max` (`Forward`, `Left`) | 18/25 (22/27) |
| `SpeedBoostBase`, `Forward` (2-14), `Left`, `WallRun`, `ZipJump` | 12, 15, 16, 20, 30 |
| `BoostSpeedCap`, `SpeedInMax` | 30, 28 |
| `GravityFall` after a zip, `TimeToPeak`, `OutHeight` | 18 m/s², 0.5 s (0.6-0.75), 1.25 m (3-5) |
| `ReZipOne/Two/Three` `BoostScale` | 0.9 / 0.85 / 0.8 |
| ... `BoostCap`, `OutSpeedFloor` | 25/24/23, 20/19/18 |
| ... `FallGravity`, `DropSpeedKept`, `ZipLockoutTime` | 22/24/26, 0.15/0.35/0.5, 1.1/1.2/1.25 s |
| `ReZipTime`, `CooldownOne/Two/Three`, `FalloffOne/Two/Three` | 0.66 s, 0.66/0.9/1.2 s, 0.75/0.6/0.45 |
| `WebZipSwingFail` (zip when no swing point) | `OutSpeedFloor` 30, `BoostSpeedCap` 33 |

The game makes each chained zip weaker and lock out longer, so zipping alone stalls at ~18-23 m/s while
swinging builds momentum. Spidy: every web allows one full yank (18 m/s), no chain penalty, no cooldown.

`PointLaunchConfig`: straight (`ExitDataZero`) 24 m/s +3 timed bonus, height 6 m +6, peak 0.65 s;
full (`ExitDataFull`) 26 +6, height 6 +3; back 12 +2, height 10 +16; bonus window `BoostWindowMin/Max` 0.2-0.5 s.
Spidy: horizontal = min(zip speed, limit), vertical 9 m/s (`jumpSpeed` x1.5), window 0.25 s.

Free fall (already ported): 26 m/s², terminal 42 rising 8/s to 100.

## Recommendations (in order of payoff)

1. **Asymmetric swing gravity.** While a taut web holds the body: falling (velocity.y < 0) gravity = 24 (`GravityFallInit`),
   15 while the stick is held (`GravityFallHold`); rising with stick forward 2-14 by speed/pitch (`RiseGravity*`), rising with no
   stick 28-45 (`*Zero`). To keep WEIGHT meaningful, scale by weight/100 x the game's values, or use ratios to the WEIGHT gravity
   (down x2.45, rise x0.6). Free fall after release stays as it is. Single biggest fix: every swing now pays out
   (table in section 1).
2. **Sling on attach** (`SlingDataAir`): on attach, rotate the body's velocity onto the swing's tangent keeping its magnitude
   (or give the rope 0.25 s of give, up to 6 m at 35 m/s), instead of deleting the outward part. Fixes the up-to-29% speed cut when
   a web catches a fall or a dive.
3. **Swing speed boost** (`SpeedBoostTarget` 8-10 (+6 turn bonus), `SpeedBoostAccel` 20, jerk 120, only ≥12 m above ground):
   during the downswing/trough add tangential speed until entry speed + target.
4. **Release by phase + jump-off.** Jump pressed with a web attached (today ignored in the air) = jump off: release all, raise
   vertical speed to `VertFloorJump` 12/22/30 and add `JumpBoost` 8/20/24 by pitch band (Low/Mid/High above), buffer the press
   0.15 s (`JumpBankZones`), only ≥6 m up (`ReleaseBoostAltMin`). Plain release: `VertFloorBase` 0/8 in the Mid/High bands
   (a small free lift at the top of the arc). In VR the jump could also be a release with a fast hand flick.
5. **Momentum meter linking every move** (section 5): +1 per jump-off, +0.25 per release, +0.125-0.2/s while swinging, zips and
   dives feed it (caps 5); decays 0.25-1/s in the air after 0.5-1 s. At ≥1: swing cap floor 24 m/s; at ≥4: floor 28, **swing
   gravity x0.5**, rope +4 m. This is the game's answer to "swing and zip feel separate": a zip charges the next swing.
6. **Zip chain falloff** (`ReZipOne/Two/Three`): yank boost x0.9/0.85/0.8, cap 25/24/23 m/s, within 0.66 s re-zip window /
   1.1-1.25 s lockout; zip out speed floor 20-22, cap 30 rather than a flat +18 per web. Makes swinging the way to go fast.
7. **Stick boost** (`InputBoost`): forward stick 8 m/s² (ramp-in 0.4 s), 14 m/s² between 24 and 32 m/s, and up to 30% less gravity;
   replaces the flat 4 m/s².
8. **Point launch** (`PointLaunchConfig`): 24 m/s out (+3 if timed within 0.2-0.5 s), 6 m high (peak 0.65 s), instead of the
   zip's speed and 9 m/s up.
9. **Post-release drag** (`TraversalJumpDrag`): forward drag 2→10 m/s² from 28→36 m/s, so speed above ~30 bleeds naturally after
   a jump-off (Spidy: 4 m/s per s only above the limit).

Not to port alone: `InSpeedCarryover` 0.83 (the game cuts entry speed 17% because 1-3 refill it; with Spidy's conserved swing it
would only make things worse).

## Swing gravity, sling and assist (Oct 13 2026 dig)

Read from `Spider-Man.exe` 4.630.0.0 statically (capstone; no game run) plus the config assets. The exe keeps a
reflection table per config type (name, offset, kind and a **tooltip string** per field; read with a small script over
the descriptors in `.data`), so every field below was checked against the type the game actually loads. RVAs are
image-relative. CONF = read from code or asset; INF = inferred. Hero swing state = class `Hero::HeroStateSwing`
(vtable `0x38ce690`); its live params are a blended copy in a swing component: `[state+0x380]` SpeedParams,
`[state+0x388]` GravityParams, `[state+0x378]` TurnParams (set at `0xabd5cf`).

### 0. Corrections to sections 1-6 above (dead fields: in the asset, not in the type, ignored by the game)

| Earlier claim | Truth (CONF, reflection table) |
|---|---|
| `hero_swingconfig` `MotionParamsStandard/Fast/Indoor/Ground`, `ReleaseParams*`, `SearchParams*` (the bracketed values) | Not fields of `HeroSwingConfig` (only `SwingSetupStandard/Ground/Indoor` refs). Live values: `hero_swingsetup_standard.config` only. So RiseGravityHighMax is -11 (not -14), SpeedBoostTarget **10**, InSpeedCarryover 0.83, TerminalVelocity 6/16/42 -> 24/27.25/40, PureMax 80. |
| `SpeedBoostBonusBase/Turn`, `SpeedBoostAltMin` 12 m | No such fields. Spidy's `boostAltitude` 12 copies a dead field. |
| `TimeToPeakStandard/Jump` | No such fields (`SwingReleaseParamSet` = JumpBoost, VertFloorBase, VertFloorJump). |
| `ReleaseBoostAltMin` = "no boost below 6 m" | Tooltip: "Minimum XZ speed allowed on a line release" (6 m/s). |
| `InSpeedCarryover` = share of incoming speed the swing starts with | Tooltip: "Percent of speed over baseline max that is kept on a new swing". The section-1 table that cut entry speed by 17% is wrong. |
| `SpeedBoostTarget` = speed added over a swing | Tooltip "Speed that character is boosted to before boost stops"; code: absolute target (see 3). |
| `JumpBoost` = speed along the way | Added to **vertical** speed (see 4). Horizontal is `JumpBoostXZ` 1.5. |
| `SlingDataAir` `CarryFallDelt*`, `CarryFallSpeed*`, `MaxCarryFall`, `MaxCarryRise` | Dead. Live `HeroSlingData`: `CarryRiseMax` (unset), `FallDataBase` (InSpeed 8->35 m/s => MaxFallDelta 0.25->6 m "maximum fall distance"), `FallDataReZip`, `Duration` 0.25. |
| Section 6 `WebZipAirOutdoor` (traversal config) | Not a field of `HeroTraversalConfig`; dead. Live zip = `hero_webzipsetup_outdoor.config`: ReZipTime 1.25 s (0.66 with skill), ReZipTwo/Three only (ReZipBonus = Quick Glide skill), OutSpeedFloor 20, SpeedBoostForward 2-14, Left 16, SpeedInMax 28, height 3-5 m. |
| `FallData` DiveGain +0.5..1.25, `DiveBonusMax` 2 | DiveGain/FallGain are **momentum per second** (tooltip); `DiveBonusMax` dead; `DiveMomentumCapMin/Max`, `FallGainMax`, `FallSpeedMin/Max`, `MaxMomentum` unset = compiled defaults (not found). |
| `SwingData` MinTime/MaxTime 0.15/0.33 s | Earn-rate ramp at swing start: `StartScale` (unset) until MinTime, full rate from MaxTime. |

### 1. Swing gravity (CONF, `0xac0b60`, called every substep from the swing integrator `0xabdb30`)

The swing has 3 modes (`[state+0x574]`, set at `0xabc5d0`): **0** attach -> trough; **1** from arc angle >= 90
(0 = rope horizontal below, 90 = trough, 180 = horizontal above); **2** when falling again with arc > 70 (the swing-back).
Gravity g (negative = down) per substep:

- **Falling, mode 0/1 (downswing):** `g = lerp(-12, G_fall, clamp((t-0.25)*4))`, t = seconds since attach,
  G_fall = lerp(GravityFallInit -24, GravityFallTurn -24, line tilt 30->60 deg); -12 = min(0.33*G_fall, -12).
  So **half gravity for the first 0.25 s of every swing, full 24 by 0.5 s.** Past arc 145 deg it blends to -50
  (`g -= (g+50)*clamp((arc-145)/35)`).
- **Rising (any mode):** with I = stick input (forward 0..1, back < 0; starts at -1 on attach), speed s, velocity pitch P:
  `low  = lerp(lerp(LowMinZero -28, LowMaxZero -40, tL), lerp(LowMin -2, LowMax -4, tL), I)`, tL = norm(s, 16, 40);
  `high = lerp(lerp(HighMinZero -30, HighMaxZero -45, tH), lerp(HighMin -6, HighMax -11, tH), I)`, tH = norm(s, 12, 30);
  each then blends to `RiseGravityLowTilt` -12 / `HighTilt` -20 by line tilt norm(25, 60 deg);
  `g = lerp(low, high, norm(P, 30, 70 deg))`. Line length L: 8-16 m -> g blends to -35 (`(L-8)/8`); L >= 16:
  `g *= lerp(1, RiseLineLengthScale 0.5, norm(L, 28, 55))` (long ropes rise lighter).
- **Support floor:** `g = m + (g-m)*f`, m = min(g, -10): gravity weaker than 10 is allowed only in proportion to f
  (`[state+0x510]`, see 2). f = 1 on ordinary forward swings.
- **Back stick while rising (mode != 0):** target -25..-50 (`-25-15|I|` at low pitch, `-35-15|I|` high), blended by |I|, never lighter.
- **Mode 1:** `g *= GravityScale` of the current momentum stage (`[state+0x468]`, 1 below 4, **0.5 at >= 4**) and x0.5 more
  if the swing point's flag bit (`[state+0x5f9]`, byte 0x6c bit 6 of the attach request; meaning unknown) is set.
  So momentum lightens **only the upswing**.
- **Mode 2 (swing-back):** constant **-14 falling, -22 rising**; plus speed drag ramping 1%/s -> 15%/s over 10 s (`0xabc785`).

Ratios (the game's free fall is 26, `StandardFallGravity`; Spidy's is 9.81 x WEIGHT, so compare ratios):

| Phase | g (m/s^2) | / free fall 26 | / downswing 24 |
|---|---|---|---|
| downswing, first 0.25 s | 12 | 0.46 | 0.5 |
| downswing | 24 | 0.92 | 1 |
| upswing, stick forward, pitch < 30 | 2-4 (speed 16-40) | 0.08-0.15 | 0.08-0.17 |
| upswing, stick forward, pitch > 70 | 6-11 (speed 12-30) | 0.23-0.42 | 0.25-0.46 |
| same, at momentum >= 4 (mode 1) | 1-5.5 | 0.04-0.21 | 0.04-0.23 |
| upswing, sideways line (tilt 60) | 12 / 20 | 0.46 / 0.77 | 0.5 / 0.83 |
| **upswing, no stick** | 28-40 / 30-45 | **1.08-1.73** | **1.17-1.88** |
| swing-back (mode 2) | 14 down / 22 up | 0.54 / 0.85 | |
| after release (Low/Mid/High band) | 13 / 23 / 26 | 0.5 / 0.88 / 1.0 | |
| after jump-off (Low/Mid/High) | 20 / 38 / 45 | 0.77 / 1.46 / 1.73 | |

The pump is entirely the light upswing **with the stick held forward**; the downswing is not heavier than free fall.
Let go of the stick and the upswing is 1.2-1.9x the downswing: the swing dies instead of throwing you.
Integrator (CONF): 4 substeps per frame; acceleration = g*y + f*(-g*u_y)*u + (|v|^2/L)*u (u = unit vector to the anchor),
i.e. tension cancels gravity's rope component (scaled by f) plus centripetal. Then the 3D/horizontal caps (section 4).

### 2. Web give on the catch (no sling, no bounce)

`SlingDataAir` is not part of the swing: it sits with `WebZipSetup*`/`SwingFailZipData` in `HeroTraversalConfig` and
its `FallDataReZip` is zip-specific (INF: the zip/sling-to-point states). Nothing in the swing state reads it (INF).
The swing's own catch softening (CONF unless marked):
- half gravity for 0.25 s, ramping to full by 0.5 s (above);
- **rope length not enforced for the first 0.33 s**: per substep the overshoot past the rope length is scaled by
  k = 1 - 0.72*clamp((t-0.33)/0.67), so it is kept entirely until 0.33 s and pulled in hard (72%/substep) from 1.0 s
  (`0xabdfba`, `0xabea8c`; CONF for the timing, INF that this is position overshoot);
- **support factor f** at attach (`0xabd610`): f = 1 - t_side * (0.35 + 0.65*(1 - norm(horiz. distance, 8, 16 m))) * t_pitch,
  t_side = norm(angle between anchor direction and heading, 30 -> 55 deg), t_pitch = 1 unless the reference
  vector points > 10 deg down (INF which vectors). After 0.2 s f climbs back at 1.25/s. f < 1 = the rope holds only part
  of gravity (the hero sags) and the upswing cannot be lighter than 10. A close side anchor => no float.
- No spring, no rebound toward the anchor anywhere. No speed is "kept along the swing" by a separate rule: the
  centripetal term just bends the velocity; the caught speed is limited only by the speed caps.

### 3. Assists (speed boost, limits, input)

- **Speed boost** (`0xabc460`, inline at `0xabe0d6`; CONF): only in **mode 0** (attach -> trough), while
  speed < SpeedBoostTarget (**absolute 10 m/s** standard, 20 open arena; speed = horizontal when rising) and back-stick
  brake `[0x57c]` <= 0.2. Accel starts at max(0.15*SpeedBoostAccel, 8) = 8 m/s^2, rises 120 m/s^3 to 20; adds
  min(target - speed, a*dt) along the velocity. It lifts a slow start to 10 m/s; at normal swing speeds it does nothing.
- **Horizontal speed limit TV** at attach (`0xab38e0`, CONF): a "virtual fall speed" (fall speed pushed toward
  FallSpeedThresholdMid by k = 0.75*((1 - norm(line pitch, 10, 35 deg)) + norm(L, 22, 44 m)): shallow or long ropes count
  as faster falls) -> TerminalVelocityHorz 24/27.25/40 at 6/16/42; TV = max(that, current horizontal speed); x0.85 if
  `[state+0x5f7]`; >= momentum stage floor 24 (>=1) / 28 (>=4, +1.8/3.6/5.5 by skill).
- **Input-scaled limit** (`0xabf580`, CONF): limit = lerp(TVmin, TV, I) with TVmin = max(TV*TerminalVelocityMinPercent,
  MinFloor 18), approached at InputDecelZero; back stick lowers TV at |I|*InputDecelBack (floor MaxFloor 20). 3D cap
  lerp(TV, 3TV, I) (mode 0 also limitH/cos(line pitch)), times clamp(x/14) of a length argument (INF rope), <= PureMax 80.
  MinPercent/DecelZero/DecelBack are unset in the asset (compiled defaults not found).
- **Jump redirect** (`0xac1840`, CONF): within `JumpRedirectZones` line angles (100-110, 120-135, 137-145) the jump's pitch is
  re-aimed to lerp(ExitAngleStart, End, t^ExitBias) (ExitBias is an exponent).
- `InputBoost` (GravityScaleMax 0.3, ActiveAcceleration 8, ...): reader not found in the swing code (open).

### 4. Release and jump-off (`0xac17aa`, CONF)

Band by velocity pitch: **Fall < -10 deg, Low -10..7, Mid 7..45, High > 45** (`ReleaseAngleLowStart/LowFinal/HighStart`;
HighFinal 70 not used here). In order:
1. Fall band: horizontal x(1 + (max(h*0.66, 8)/h - 1)*norm(|pitch|, 20, 55)) (`JumpFromFallHorzDamp/Min`).
2. Jump: **v.y += JumpBoost** (Fall 8, Low 8, Mid 20, High 24; +6 more in High if flag 0x5f9).
3. (skill-gated INF) v.y += 3-6 / 4-10 / 6-20 by a 0-1 value of the swing component.
4. Rising release: XZ += ReleaseBoostXZ (unset) * sin(pitch) * (1 - 0.5*norm(h, 29.3, 40)) (TerminalVelocityHorzMin + 0.33 of the span to Max),
   clamped so XZ >= **6 m/s** and <= TV + 1. Jump adds JumpBoostXZ **1.5** x pitch ramp (0 at 10, full 30-35, 0 at 55 deg).
5. **v.y = max(v.y, floor)**: release floors Fall/Low none (-30), Mid 0, High 8; jump floors 12/12/22/30.
6. Gravity after: release 13/23/26, jump 20/38/45, lerp by pitch around GravityAngleMid 35 (Low/High angles unset).

Momentum (CONF fields): swing 0.125/s at arc 20 -> 0.2/s at 90 (cap 4); let go +0.25 (cap 3, x RateScaleList by
angle: 90 -> 1, 100 -> 0), jump +1 (cap 5); dive/fall gains are per second; stages step at 1 and 4 (INF step, not blend).

### Game vs Spidy 2.0

| Game mechanism (standard setup) | Game value | Spidy field | Spidy 2.0 | Change |
|---|---|---|---|---|
| Downswing gravity | 24 = 0.92 x free fall; 12 (0.46) first 0.25 s | `swingFall` | 1.25 x g, whole time | **0.9-1.0**, and 0.5 for 0.25 s after attach |
| Upswing, stick forward | 0.08-0.42 x free fall by pitch/speed | `swingRise` | 0.6 x g, always | ~0.15 (pitch < 30) .. 0.4 (> 70), **only with forward input** |
| Upswing, no stick | 1.1-1.7 x free fall | (none) | 0.6 | blend `swingRise` by forward stick toward ~1.3 |
| Long / short rope | x1 -> x0.5 for 28-55 m; -> 35 for 8-16 m | (none) | - | optional |
| Momentum >= 4 | GravityScale 0.5, **upswing only** | `momentumGravity` | 0.5, both ways | apply to rise only |
| Speed boost | up to **10 m/s absolute**, mode 0 only, accel 8 -> 20 (120/s) | `boostTarget`, `boostAltitude` | **attach speed + 8**, down/trough; 12 m alt | absolute target ~6.5 (10 x sqrt(9.81/24)) or 0; `boostAltitude` 0 (dead field) |
| Catch | no spring/bounce; rope unenforced 0.33 s; half gravity 0.25 s; f < 1 for close side anchors | `catchFrequency`, `catchBounce`, `catchKeep`, `catchDepth` | 1 Hz, **0.4**, 1, 3 m | `catchBounce` -> ~0 (no fling to the anchor); keep 1 |
| Swing limit | TV 24/27.25/40 by fall 6/16/42, >= current; floor 24/28 by momentum | `fallCarry`, `momentumLimit` | ported; 28/24 | ok |
| Jump-off vertical | v.y = max(v.y + 8/8/20/24, 12/12/22/30) | `jumpOffUp` | max(v.y, 6/10/13) | add a scaled boost: up += JumpBoost x sqrt(g/g_after) ~ 5.5/10/11 |
| Jump-off horizontal | +1.5 (ramped) | `jumpOffBoost` | **+4/9/11** | ~1 |
| Jump bands | -10 / 7 / 45 deg | `jumpOffMid/High` | 0 / 25 / 50 | -10 / 7 / 45 |
| Release | XZ >= 6; v.y >= 8 above 45 deg; gravity 0.5/0.88/1.0 | (none) | normal g | optional |

Scale rule: same apex height needs v_spidy = v_game x sqrt(g_spidy / g_game); same timing needs x(g_spidy / g_game).
With g_spidy 9.81 vs 24-26 that is x0.62-0.64 (height) or x0.38-0.41 (time); absolute m/s copied unscaled are 2.5x too
high in height terms.

**Why 2.0 throws him up too high (in order):** (1) the boost adds 8 m/s on every web at any speed (20 -> 28 m/s is
~20 m more height at 9.81); the game only lifts a start below 10 m/s. (2) The light upswing is unconditional; in the
game it needs the stick held forward and is 1.2-1.9x the downswing without it. (3) `catchBounce` 0.4 flings the body
toward the anchor (up); the game has no rebound. (4) `swingFall` 1.25 makes the downswing heavier than free fall
(game 0.92, and 0.46 for the first 0.25 s), adding energy every swing. (5) momentum's 0.5 also lightens the downswing (minor).

### Open questions

- Compiled defaults of unset fields: TerminalVelocityMinPercent, InputDecelZero/Back, ReleaseBoostXZ, GravityAngleLow/High,
  MaxMomentum, DiveMomentumCap*, FallGainMax, SwingData StartScale, stage 0 GravityScale (assumed 1). No default-instance
  table found; the type constructors (`0x11ebd40`) only zero.
- Meaning of the swing-request flag bit (`[state+0x5f9]`: x0.5 upswing gravity, +6 m/s high jump) and `[state+0x5f7]` (TV x0.85).
- Whether momentum stages step or blend; where `InputBoost` and `TruePivotFactor*` are read.
- Whether the 0.33 s unenforced rope keeps the radial (outward) velocity or only the position overshoot.
- Which game value maps to Spidy's "input" in VR (stick forward along travel is the literal port).
