// Swings without a headset: the same solver the game runs, in an open world
// whose webs hold on a ceiling 60 m up, under each swing model. Prints what
// each swing did (the swing ledger), so a change to the swing's physics can be
// measured before anyone puts the headset on.
//
//   spidy_swing_bench [gravity m/s^2, default 9.81] [speed limit m/s, default 40]
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
void print(const char* name, const SwingLedger& l, float g) {
    // Energies as metres of height (over gravity), as analyze_swings.py prints them.
    std::printf("  %-28s in %4.1f out %4.1f peak %4.1f drop %5.1f %4.1fs | rope %5.1f snap %4.1f(%u) "
                "elastic %5.1f sling %5.1f pump %5.1f yank %5.1f jump %5.1f cap %5.1f | catches %u stretch %.1f m\n",
                name, l.entry, l.exit, l.peak, l.drop, l.seconds, l.rope / g, l.snapLoss / g, l.snaps, l.elastic / g,
                l.sling / g, l.pump / g, l.yank / g, l.jump / g, l.cap / g, l.catches, l.deepest);
}
// One swing: a web shot `pitch` degrees above the way the body goes, held
// until the body rises at `release` degrees (or `most` seconds), then let go
// (or, with jump, jumped off).
SwingLedger swing(Swing& s, const OpenWorld& world, Body start, float pitch, float release, bool jump, float most = 9) {
    s.reset(start);
    const Vec3 way = ahead(start.velocity, pitch);
    bool rose = false;
    for (float t = 0; t < most; t += frame) {
        Input in = aimed(s.body().position, way, true);
        const Vec3 v = s.body().velocity;
        rose |= v.y > 1;
        const bool done = t > .2f && rose && pitchOf(v) >= release;
        if (done) {
            in.hands[0].grip = jump ? 1.f : 0.f;
            in.jump = jump;
            s.update(frame, in, world);
            break;
        }
        s.update(frame, in, world);
        if (!s.webs()[0].attached && t > 0)
            break;
    }
    if (s.webs()[0].attached) {
        Input in = aimed(s.body().position, way, false);
        s.update(frame, in, world);
    }
    return s.lastSwing();
}
// Swings one after another for 20 s: each web shot 45 degrees up ahead (65
// below 20 m) after a short free flight, held until the body rises at 30
// degrees.
void chain(SwingConfig config, float g, bool jump) {
    Swing s(config);
    OpenWorld world;
    s.reset({{0, 35, 0}, {12, 0, 0}, false});
    float t = 0, sum = 0, flight = 0;
    int samples = 0, swings = 0;
    bool holding = false, rose = false, lowest = false;
    Vec3 way{};
    float minY = 1e9f;
    while (t < 20) {
        Input in;
        const Vec3 v = s.body().velocity;
        if (!holding) {
            flight += frame;
            in = aimed(s.body().position, way, false);
            if (flight > .35f) {
                // As a player does: steeper when low, to keep the height.
                way = ahead(v, s.body().position.y < 20 ? 65.f : 45.f);
                holding = true;
                rose = false;
                in = aimed(s.body().position, way, true);
            }
        } else {
            in = aimed(s.body().position, way, true);
            rose |= v.y > 1;
            if ((rose && pitchOf(v) >= 30) || !s.webs()[0].attached) {
                in.hands[0].grip = jump ? 1.f : 0.f;
                in.jump = jump && s.webs()[0].attached;
                holding = false;
                flight = 0;
                ++swings;
            }
        }
        s.update(frame, in, world);
        sum += length(s.body().velocity);
        minY = std::min(minY, s.body().position.y);
        lowest |= s.body().grounded;
        ++samples;
        t += frame;
    }
    std::printf("  20 s of swings%s: %d swings, mean speed %.1f, end speed %.1f m/s, end height %.0f m, lowest %.0f m%s\n",
                jump ? " (jumping off)" : "", swings, sum / samples, length(s.body().velocity), s.body().position.y,
                minY, lowest ? " (touched the ground)" : "");
    (void)g;
}
void yankCatch(SwingConfig config, float g) {
    // A yank toward an anchor ahead, then the rope pulling taut again on the
    // far side: what that second pull takes.
    Swing s(config);
    OpenWorld world;
    s.reset({{0, 35, 0}, {10, 0, 0}, false});
    const Vec3 way = ahead({1, 0, 0}, 45);
    Vec3 hand{};
    for (int f = 0; f < 270; ++f) {
        if (f >= 5 && f < 15)
            hand = hand + way * (2.5f * frame); // the hand pulls toward the anchor, 2.5 m/s
        Input in = aimed(s.body().position, way, true, hand);
        s.update(frame, in, world);
    }
    s.update(frame, aimed(s.body().position, way, false), world);
    print("yank, then the rope again", s.lastSwing(), g);
}
} // namespace
int main(int argc, char** argv) {
    const float g = argc > 1 ? std::strtof(argv[1], nullptr) : 9.81f;
    const float limit = argc > 2 ? std::strtof(argv[2], nullptr) : 40;
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
        std::printf("%s swing (gravity %.2f, limit %.0f m/s)\n", model == SwingModel::v2 ? "2.0" : "Classic", g,
                    limit);
        Swing s(config);
        for (float v0 : {0.f, 10.f, 20.f}) {
            const std::string name = "level at " + std::to_string(static_cast<int>(v0)) + " m/s";
            print(name.c_str(), swing(s, world, {{0, 35, 0}, {v0, 0, 0}, false}, 45, 30, false), g);
        }
        for (float fall : {10.f, 20.f, 30.f}) {
            const std::string name = "falling at " + std::to_string(static_cast<int>(fall)) + " m/s";
            print(name.c_str(), swing(s, world, {{0, 40, 0}, {3, -fall, 0}, false}, 45, 30, false), g);
        }
        print("level at 10, jumping off", swing(s, world, {{0, 35, 0}, {10, 0, 0}, false}, 45, 30, true), g);
        print("level at 10, off at 50 deg", swing(s, world, {{0, 35, 0}, {10, 0, 0}, false}, 45, 50, true), g);
        yankCatch(config, g);
        chain(config, g, false);
        chain(config, g, true);
    }
}
