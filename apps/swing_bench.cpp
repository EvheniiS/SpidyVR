// Swings without a headset: the same solver the game runs, in an open world
// whose webs hold on a ceiling 60 m up, under each swing model. Prints what
// each swing did (the swing ledger) and how high it threw the body (apex: the
// top of the swing and of the flight after it, above where the swing began;
// throw: how far the body rose after letting go), so a change to the swing's
// physics can be measured before anyone puts the headset on.
//
//   spidy_swing_bench [gravity m/s^2, default 9.81] [speed limit m/s, default 40] [game]
//
// With `game`, each frame runs as the game runs it (game_swing.cpp): the solver
// predicts from the body the game has, and the game moves the body by the
// prediction's average velocity, capped at the speed limit.
#include "spidy/swing.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace spidy;
namespace {
constexpr float ceiling = 60, frame = 1.f / 90;
class OpenWorld final : public World {
  public:
    std::optional<RayHit> raycast(Vec3 o, Vec3 d, float range) const override {
        if (d.y > 1e-4f) { // the ceiling, seen from below
            const float t = (ceiling - o.y) / d.y;
            if (t > 0 && t <= range)
                return RayHit{o + d * t, {0, -1, 0}, 1, true};
        }
        if (d.y < -1e-4f && o.y > 0) { // the ground
            const float t = -o.y / d.y;
            if (t <= range)
                return RayHit{o + d * t, {0, 1, 0}, 2, true};
        }
        return {};
    }
    MoveResult sweep(Vec3, Vec3 to, float radius) const override {
        if (to.y < radius)
            return {{to.x, radius, to.z}, {0, 1, 0}, true};
        return {to, {}, false};
    }
    bool exists(std::uint64_t id) const override {
        return id == 1 || id == 2;
    }
};
bool native = false;
Body gameBody{};
void start(Swing& s, Body body) {
    s.reset(body);
    gameBody = body;
}
const Body& now(const Swing& s) {
    return native ? gameBody : s.body();
}
// One frame: the solver's own clock, or as the game runs it.
void advance(Swing& s, const Input& in, const World& world) {
    if (!native) {
        s.update(frame, in, world);
        return;
    }
    const auto p = s.predictNativeStep(frame, in, world, gameBody, frame);
    if (!p.valid) {
        gameBody.velocity.y -= 9.81f * frame;
        gameBody.position = gameBody.position + gameBody.velocity * frame;
        return;
    }
    const Vec3 average = (p.target - gameBody.position) / frame;
    const Vec3 requested = limited(average, s.speedLimit() * (1 - 1e-6f));
    gameBody.position = gameBody.position + requested * frame;
    gameBody.velocity = limited(p.velocity + requested - average, s.speedLimit());
    gameBody.grounded = gameBody.position.y < .5f;
    if (gameBody.grounded) {
        gameBody.position.y = .5f;
        gameBody.velocity.y = std::max(0.f, gameBody.velocity.y);
    }
}
// The right hand aimed along `way` from the body, the grip as given.
Input aimed(Vec3 from, Vec3 way, bool grip, Vec3 hand = {}) {
    Input in;
    in.hands[0] = {{from, arc({0, 0, -1}, normalized(way))}, hand, true, 0, grip ? 1.f : 0.f};
    return in;
}
// Up ahead along the horizontal velocity at `pitch` degrees above level.
Vec3 ahead(Vec3 velocity, float pitch) {
    Vec3 h{velocity.x, 0, velocity.z};
    h = length(h) < .5f ? Vec3{1, 0, 0} : normalized(h);
    const float r = pitch * 3.14159265f / 180;
    return h * std::cos(r) + Vec3{0, std::sin(r), 0};
}
float pitchOf(Vec3 v) {
    const float s = length(v);
    return s < .1f ? 0 : std::asin(std::clamp(v.y / s, -1.f, 1.f)) * 57.29578f;
}
void print(const char* name, const SwingLedger& l, float g, float apex = 0, float thrown = 0) {
    // Energies as metres of height (over gravity), as analyze_swings.py prints them.
    std::printf(
        "  %-28s in %4.1f out %4.1f peak %4.1f drop %5.1f apex %+5.1f throw %4.1f %4.1fs | "
        "rope %5.1f snap %4.1f(%u) elastic %5.1f sling %5.1f pump %5.1f boost %4.1f yank %5.1f jump %5.1f "
        "cap %5.1f | catches %u stretch %.1f m momentum %.1f\n",
        name, l.entry, l.exit, l.peak, l.drop, apex, thrown, l.seconds, l.rope / g, l.snapLoss / g, l.snaps,
        l.elastic / g, l.sling / g, l.pump / g, l.boost / g, l.yank / g, l.jump / g, l.cap / g, l.catches,
        l.deepest, l.momentum);
}
struct Thrown {
    SwingLedger ledger;
    float apex;   // the top of the swing and of the flight after it, above where the swing began
    float thrown; // how far the body rose after letting go
};
// One swing: a web shot `pitch` degrees above the way the body goes, held
// until the body rises at `release` degrees (or `most` seconds), then let go
// (or, with jump, jumped off), and flown on to the top of its flight.
Thrown swing(Swing& s, const OpenWorld& world, Body start, float pitch, float release, bool jump,
             float most = 9) {
    ::start(s, start);
    const Vec3 way = ahead(start.velocity, pitch);
    bool rose = false;
    float top = start.position.y;
    for (float t = 0; t < most; t += frame) {
        Input in = aimed(now(s).position, way, true);
        const Vec3 v = now(s).velocity;
        rose |= v.y > 1;
        // release 90: held to the top of the swing, where it stops rising.
        const bool done = t > .2f && rose && (release >= 90 ? v.y <= 0 : pitchOf(v) >= release);
        if (done) {
            in.hands[0].grip = jump ? 1.f : 0.f;
            in.jump = jump;
            advance(s, in, world);
            break;
        }
        advance(s, in, world);
        top = std::max(top, now(s).position.y);
        if (!s.webs()[0].attached && t > 0)
            break;
    }
    if (s.webs()[0].attached) {
        Input in = aimed(now(s).position, way, false);
        advance(s, in, world);
    }
    const SwingLedger ledger = s.lastSwing();
    const float released = now(s).position.y;
    float flown = released;
    for (float t = 0; t < 6 && !now(s).grounded; t += frame) {
        top = std::max(top, now(s).position.y);
        flown = std::max(flown, now(s).position.y);
        if (now(s).velocity.y <= 0)
            break;
        advance(s, aimed(now(s).position, way, false), world);
    }
    return {ledger, top - start.position.y, flown - released};
}
// Swings one after another for 20 s: each web shot 45 degrees up ahead (65
// below 20 m) after a short free flight, held until the body rises at 30
// degrees.
void chain(SwingConfig config, float g, bool jump) {
    Swing s(config);
    OpenWorld world;
    start(s, {{0, 35, 0}, {12, 0, 0}, false});
    float t = 0, sum = 0, flight = 0;
    int samples = 0, swings = 0;
    bool holding = false, rose = false, lowest = false;
    Vec3 way{};
    float minY = 1e9f, maxY = 0;
    while (t < 20) {
        Input in;
        const Vec3 v = now(s).velocity;
        if (!holding) {
            flight += frame;
            in = aimed(now(s).position, way, false);
            if (flight > .35f) {
                // As a player does: steeper when low, to keep the height.
                way = ahead(v, now(s).position.y < 20 ? 65.f : 45.f);
                holding = true;
                rose = false;
                in = aimed(now(s).position, way, true);
            }
        } else {
            in = aimed(now(s).position, way, true);
            rose |= v.y > 1;
            if ((rose && pitchOf(v) >= 30) || !s.webs()[0].attached) {
                in.hands[0].grip = jump ? 1.f : 0.f;
                in.jump = jump && s.webs()[0].attached;
                holding = false;
                flight = 0;
                ++swings;
            }
        }
        advance(s, in, world);
        sum += length(now(s).velocity);
        minY = std::min(minY, now(s).position.y);
        maxY = std::max(maxY, now(s).position.y);
        lowest |= now(s).grounded;
        ++samples;
        t += frame;
    }
    std::printf("  20 s of swings%s: %d swings, mean speed %.1f, end speed %.1f m/s, end height %.0f m, "
                "lowest %.0f m%s, highest %.0f m, momentum %.1f (high stage %u times)\n",
                jump ? " (jumping off)" : "", swings, sum / samples, length(now(s).velocity),
                now(s).position.y, minY, lowest ? " (touched the ground)" : "", maxY, s.momentum(),
                s.surges());
    (void)g;
}
void yankCatch(SwingConfig config, float g) {
    // A yank toward an anchor ahead, then the rope pulling taut again on the
    // far side: what that second pull takes.
    Swing s(config);
    OpenWorld world;
    start(s, {{0, 35, 0}, {10, 0, 0}, false});
    const Vec3 way = ahead({1, 0, 0}, 45);
    Vec3 hand{};
    for (int f = 0; f < 270; ++f) {
        if (f >= 5 && f < 15)
            hand = hand + way * (2.5f * frame); // the hand pulls toward the anchor, 2.5 m/s
        Input in = aimed(now(s).position, way, true, hand);
        advance(s, in, world);
    }
    advance(s, aimed(now(s).position, way, false), world);
    print("yank, then the rope again", s.lastSwing(), g);
}
} // namespace
int main(int argc, char** argv) {
    const float g = argc > 1 ? std::strtof(argv[1], nullptr) : 9.81f;
    const float limit = argc > 2 ? std::strtof(argv[2], nullptr) : 40;
    native = argc > 3 && std::string(argv[3]) == "game";
    OpenWorld world;
    for (const auto model : {SwingModel::classic, SwingModel::v2}) {
        SwingConfig config;
        // The game's physics (game_swing::physicsConfig).
        config.gravity = g;
        config.maxSpeed = limit;
        config.reelSpeed = 16;
        config.airAcceleration = 4;
        config.maxZipImpulse = 18;
        config.zipMultiplier = 4.5f;
        applySwingModel(config, model);
        std::printf("%s swing (gravity %.2f, limit %.0f m/s%s)\n", model == SwingModel::v2 ? "2.0" : "Classic", g,
                    limit, native ? ", frames as the game runs them" : "");
        Swing s(config);
        const auto show = [&](const std::string& name, Body body, float release, bool jump) {
            const auto r = swing(s, world, body, 45, release, jump);
            print(name.c_str(), r.ledger, g, r.apex, r.thrown);
        };
        for (float v0 : {0.f, 10.f, 20.f})
            show("level at " + std::to_string(static_cast<int>(v0)) + " m/s", {{0, 35, 0}, {v0, 0, 0}, false},
                 30, false);
        for (float fall : {10.f, 20.f, 30.f})
            show("falling at " + std::to_string(static_cast<int>(fall)) + " m/s",
                 {{0, 40, 0}, {3, -fall, 0}, false}, 30, false);
        show("level at 10, jumping off", {{0, 35, 0}, {10, 0, 0}, false}, 30, true);
        show("level at 10, off at 50 deg", {{0, 35, 0}, {10, 0, 0}, false}, 50, true);
        show("level at 10, held to the top", {{0, 35, 0}, {10, 0, 0}, false}, 90, false);
        show("level at 20, held to the top", {{0, 35, 0}, {20, 0, 0}, false}, 90, false);
        show("fall at 20, held to the top", {{0, 40, 0}, {3, -20, 0}, false}, 90, false);
        yankCatch(config, g);
        chain(config, g, false);
        chain(config, g, true);
    }
}
