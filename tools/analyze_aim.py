"""Where the player meant each web to go, against where the hand aimed it.

At every web attach in a session report, the head's forward (a stand-in for the target the player meant) and the web's
direction, as angles in that hand's controller-aim frame: yaw toward the palm's side is + on a left hand and - on a right
hand (the frame's +x is right on both), pitch + is up. A gaze median far from the web's shows a miss.
Samples carry no clock, so each attach's ray sample is matched to the frame sample whose hands stood nearest.

Usage: python tools/analyze_aim.py reports/game-vr-<time>.json"""
import json, math, sys
import numpy as np

d = json.load(open(sys.argv[1]))
S, W, R = d['samples'], d['swing_samples'], d['ray_samples']

def rows(m):
    m = np.array(m, dtype=float).reshape(4, 4)
    return m[0, :3], m[1, :3], m[2, :3], m[3, :3]  # x, down(-y), forward(-z), position

# Ray samples: per hand origin/direction, with qpc.
rays = []
for r in R:
    per = {}
    for h in r.get('hits', [])[: r.get('count', 0)]:
        per[h['tag']] = (np.array(h['origin']), np.array(h['direction']))
    rays.append((r['qpc'], per))
rq = np.array([q for q, _ in rays])

# Sample hand positions (grip point) to align samples with ray samples.
samp = []
for s in S:
    if s.get('presentation') != 'immersive':
        continue
    hx = [rows(h) for h in s['hands']]
    hd = rows(s['head'])
    samp.append((hx, hd))
spos = np.array([[h[3] for h in hx] for hx, _ in samp])  # n x 2 x 3

def sample_for(per):
    best, bi = 1e9, -1
    for i, p in enumerate(spos):
        dist = sum(np.linalg.norm(p[t] - per[t][0]) for t in per)
        if dist < best:
            best, bi = dist, i
    return bi, best

def deg(v):
    return math.degrees(v)

prev = [False, False]
print('hand  match_m  gaze_yaw  gaze_pitch  web_yaw  web_pitch  (degrees, controller-aim frame)')
stats = {0: [], 1: []}
for w in W:
    for hand in (0, 1):
        att = bool(w['webs'][hand]['attached'])
        if att and not prev[hand]:
            k = int(np.searchsorted(rq, w['qpc']))
            k = min(max(k, 0), len(rays) - 1)
            per = rays[k][1]
            if hand not in per:
                continue
            i, dist = sample_for(per)
            hx, hd = samp[i]
            x, down, fwd, pos = hx[hand]
            up = -down
            gaze = hd[2] / np.linalg.norm(hd[2])
            palm = per[hand][1]
            def ang(v):
                return deg(math.atan2(np.dot(v, x), np.dot(v, fwd))), deg(math.asin(max(-1, min(1, np.dot(v, up)))))
            gy, gp = ang(gaze)
            py, pp = ang(palm)
            stats[hand].append((gy, gp, py, pp))
            print(f"{'LR'[hand]}     {dist:6.2f}   {gy:7.1f}   {gp:8.1f}   {py:7.1f}   {pp:8.1f}")
        prev[hand] = att
for hand in (0, 1):
    a = np.array(stats[hand])
    if len(a):
        print('LR'[hand], 'median gaze yaw/pitch', np.median(a[:, 0]).round(1), np.median(a[:, 1]).round(1),
              ' web yaw/pitch', np.median(a[:, 2]).round(1), np.median(a[:, 3]).round(1), ' n', len(a))
