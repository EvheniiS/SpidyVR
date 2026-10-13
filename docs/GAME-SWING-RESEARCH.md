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
