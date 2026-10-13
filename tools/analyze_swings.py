"""Swings in a game-vr report, one line each, and what they did to the player's speed.

    python tools/analyze_swings.py reports/game-vr-<time>.json [--all]

A swing: from the first sample with a web attached to the first with none. For each it prints
the speed coming in, what the catch kept (the first sample after the web attached), the most
the swing reached, the speed going out, the height it lost, the yanks in it and its energy
(speed^2/2 + g*height, as metres of height: what the swing added or lost). Samples are the
runner's (about 20 a second), so the catch is measured over one sample (about 50 ms of gravity).

Reports from builds with the swing ledger (Data version 5, `swings` in the report) also carry each
swing's exact budget from the solver, printed after the sampled table: in metres of height, what the
ropes cut (snaps: a rigid rope pulled taut), what elastic catches took and kept along the swing (sling),
the swing's pump, the yanks, the reel, the jump off, the stick and the speed limit. `rest` is what the
game changed between the solver's predictions (collisions, the game's own motion).

Last, the falls with no web (airborne, off walls, going down faster than 3 m/s): Spidy's (after a web let
go) and the game's own (no web at all), each with its length, height and fastest speed going down.
"""
import argparse
import json
import math
import pathlib
import statistics


def speed(v):
    return math.sqrt(sum(x * x for x in v))


def energy(sample, g):
    """Speed squared over 2g plus height: metres of height the player's motion is worth."""
    return speed(sample['velocity']) ** 2 / (2 * g) + sample['position'][1]


def attached(sample):
    return any(w['attached'] for w in sample['webs'])


def swings(samples):
    """(before, during, after): the sample before each swing, its samples, the one after."""
    out, run, before = [], [], None
    for index, s in enumerate(samples):
        if s.get('status') not in (1, 2):
            run, before = [], None
            continue
        if attached(s):
            if not run and index and samples[index - 1].get('status') in (1, 2):
                before = samples[index - 1]
            run.append(s)
        elif run:
            out.append((before, run, s))
            run, before = [], None
    return out


def describe(before, run, after, g, start):
    first, last = run[0], run[-1]
    entry = speed(before['velocity']) if before else float('nan')
    fall = -before['velocity'][1] if before else float('nan')
    gap = (first['qpc'] - before['qpc']) / 1e7 if before else float('nan')
    # Free flight over the gap: what the speed would have been without the web.
    free = float('nan')
    if before:
        v = list(before['velocity'])
        v[1] -= g * gap
        free = speed(v)
    zips = last['zips'] - (before or first)['zips']
    lengths = [w['length'] for s in run for w in s['webs'] if w['attached']]
    seconds = (last['qpc'] - first['qpc']) / 1e7
    peak = max(speed(s['velocity']) for s in run)
    exit_ = speed(after['velocity'])
    grounded = any(s['grounded'] for s in run)
    row = dict(t=(first['qpc'] - start) / 1e7, seconds=seconds, entry=entry, fall=fall, free=free,
               caught=speed(first['velocity']), peak=peak, exit=exit_,
               drop=(before or first)['position'][1] - after['position'][1], zips=zips, grounded=grounded,
               rope=min(lengths) if lengths else float('nan'),
               gain=energy(after, g) - energy(first, g),
               catch=(energy(first, g) - energy(before, g)) if before else float('nan'),
               reeled=bool(lengths) and lengths[0] - min(lengths) > 1.5,
               ledger=last.get('swing'))
    return row


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('report', type=pathlib.Path)
    p.add_argument('--all', action='store_true', help='List every swing, not just a summary and the first 40')
    a = p.parse_args()
    r = json.loads(a.report.read_text())
    samples = r.get('swing_samples') or []
    weight = (r.get('vr_settings') or {}).get('weight') or 100
    g = 9.81 * weight / 100
    if not samples:
        print('No swing samples.')
        return
    start = samples[0]['qpc']
    rows = [describe(*s, g, start) for s in swings(samples)]
    settings = r.get('vr_settings') or {}
    print(f"{a.report.name}: {len(rows)} swings over {(samples[-1]['qpc'] - start) / 1e7:.0f} s, "
          f"limit {r.get('swing_speed')} m/s, weight {weight}% (g {g:.2f}), air webs "
          f"{'on' if settings.get('air_webs') else 'off'}")
    print('    t   s  in  fall caught(free) peak  out  drop rope zips  energy: catch swing (m)')
    for row in rows if a.all else rows[:40]:
        print(f"{row['t']:5.0f} {row['seconds']:4.1f} {row['entry']:4.1f} {row['fall']:5.1f} "
              f"{row['caught']:5.1f}({row['free']:4.1f}) {row['peak']:4.1f} {row['exit']:4.1f} "
              f"{row['drop']:5.1f} {row['rope']:4.0f} {row['zips']:3d}{' R' if row['reeled'] else '  '}"
              f"{' G' if row['grounded'] else '  '}  {row['catch']:6.1f} {row['gain']:6.1f}")
    if not a.all and len(rows) > 40:
        print(f'  ... {len(rows) - 40} more (--all)')

    def summary(name, chosen):
        if not chosen:
            return
        pick = lambda k: [x[k] for x in chosen if math.isfinite(x[k])]
        med = lambda k: statistics.median(pick(k)) if pick(k) else float('nan')
        print(f"{name}: {len(chosen)}, median in {med('entry'):.1f} -> out {med('exit'):.1f} m/s, "
              f"out-in {statistics.median([x['exit'] - x['entry'] for x in chosen if math.isfinite(x['entry'])] or [0]):+.1f}, "
              f"catch {med('catch'):+.1f} m, swing {med('gain'):+.1f} m, {med('seconds'):.1f} s")

    ledger = r.get('swings') or []
    if ledger:
        model = {0: 'classic', 1: '2.0'}.get(r.get('swing_model'), '?')
        print(f"\nSwing ledger ({model} swing model), metres of height:")
        print('   in  out peak drop    s | rope snap(n) elastic sling pump yank reel jump steer  cap  rest | '
              'catches stretch')
        for x in ledger if a.all else ledger[-40:]:
            terms = {k: x[k] / g for k in ('rope', 'snap_loss', 'elastic', 'sling', 'pump', 'yank', 'reel', 'jump',
                                           'steer', 'cap')}
            change = (x['exit'] ** 2 - x['entry'] ** 2) / (2 * g)
            rest = change - (x['drop'] + terms['yank'] + terms['reel'] - terms['rope'] + terms['elastic'] +
                             terms['sling'] + terms['pump'] + terms['steer'] - terms['cap'] + terms['jump'])
            print(f"{x['entry']:5.1f} {x['exit']:4.1f} {x['peak']:4.1f} {x['drop']:5.1f} {x['seconds']:4.1f} | "
                  f"{terms['rope']:4.1f} {terms['snap_loss']:4.1f}({x['snaps']}) {terms['elastic']:7.1f} "
                  f"{terms['sling']:5.1f} {terms['pump']:4.1f} {terms['yank']:4.1f} {terms['reel']:4.1f} "
                  f"{terms['jump']:4.1f} {terms['steer']:5.1f} {terms['cap']:4.1f} {rest:5.1f} | "
                  f"{x['catches']:3d} {x['deepest']:5.1f} m")
        total = lambda k: sum(x[k] for x in ledger) / g
        print(f"All {len(ledger)} swings: snaps {sum(x['snaps'] for x in ledger)} took {total('snap_loss'):.0f} m, "
              f"ropes {total('rope'):.0f} m; elastic catches {sum(x['catches'] for x in ledger)}, "
              f"net {total('elastic') + total('sling'):+.0f} m; pump {total('pump'):+.0f} m, yanks "
              f"{total('yank'):+.0f} m, jumps off {sum(x['jump_off'] for x in ledger)} {total('jump'):+.0f} m, "
              f"speed limit {-total('cap'):+.0f} m")
    air = [x for x in rows if not x['grounded']]
    summary('Swings with no yank or reel', [x for x in air if not x['zips'] and not x['reeled']])
    summary('Swings with a yank', [x for x in air if x['zips']])
    summary('Swings reeled in', [x for x in air if x['reeled'] and not x['zips']])
    falling = [x for x in air if math.isfinite(x['fall']) and x['fall'] > 8]
    if falling:
        kept = [x['caught'] / x['free'] for x in falling if x['free'] > 1]
        print(f"Webs that caught a fall over 8 m/s: {len(falling)}, the catch kept {statistics.median(kept):.0%} "
              f"of the speed free flight would have had (median), worst {min(kept):.0%}")
    falls_summary(samples, r.get('swing_speed') or 0)


def falls(samples):
    """Runs of samples falling with no web: in play, airborne, off walls, going down faster than 3 m/s."""
    out, run = [], []
    for s in samples:
        if (s.get('status') in (1, 2) and not attached(s) and not s['grounded'] and not s.get('wall')
                and s['velocity'][1] < -3):
            run.append(s)
        elif run:
            out.append(run)
            run = []
    if run:
        out.append(run)
    return out


def falls_summary(samples, limit):
    """Falls by who moved the player: Spidy (after a web let go) or the game (no web at all)."""
    for owner, name in ((1, 'Spidy'), (0, 'the game')):
        runs = [run for run in falls(samples) if run[0]['owned'] == owner]
        if not runs:
            continue
        seconds = [(run[-1]['qpc'] - run[0]['qpc']) / 1e7 for run in runs]
        down = sorted(max(-s['velocity'][1] for s in run) for run in runs)
        heights = [run[0]['position'][1] - run[-1]['position'][1] for run in runs]
        top = [max(speed(s['velocity']) for s in run) for run in runs]
        # Only Spidy's falls answer to the swing speed limit; past it is a dive or a caught dive's carry.
        capped = ''
        if owner and limit:
            capped = (f"; at the speed limit {sum(limit - .5 <= t <= limit + .5 for t in top)}, "
                      f"past it (dive, carry) {sum(t > limit + .5 for t in top)}")
        print(f"Falls with no web ({name}): {len(runs)}, {sum(seconds):.0f} s in all; each median "
              f"{statistics.median(seconds):.1f} s and {statistics.median(heights):.1f} m; fastest going down: "
              f"median {statistics.median(down):.1f}, 90% {down[int(.9 * (len(down) - 1))]:.1f}, "
              f"max {down[-1]:.1f} m/s{capped}")


if __name__ == '__main__':
    main()
