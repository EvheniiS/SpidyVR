#include "spidy/swing.hpp"
#include <stdexcept>

namespace spidy {
namespace {
// A body this close to the end of its rope is held by it. After an exact
// projection the distance equals the length only to float rounding, about a
// millimetre at game world coordinates; an exact comparison let rounding
// choose which of two ropes acted on the velocity in a step.
constexpr float nearlyTaut = .005f;
Vec3 projectBall(Vec3 p, Vec3 center, float radius) {
    return center + limited(p - center, radius);
}

// A web with its anchor at `hold`.
struct Rope {
    Vec3 anchor{};
    float length{};
};
Vec3 projectTwoWebs(Vec3 p, const Rope& a, const Rope& b) {
    // Exact projection onto the intersection of two rope-length spheres.
    // Alternating single-rope projections converge poorly near opposing anchors.
    const auto onA = projectBall(p, a.anchor, a.length);
    if (length(onA - b.anchor) <= b.length)
        return onA;
    const auto onB = projectBall(p, b.anchor, b.length);
    if (length(onB - a.anchor) <= a.length)
        return onB;
    const Vec3 span = b.anchor - a.anchor;
    const float distance = length(span);
    if (distance < 1e-5f)
        return projectBall(p, a.anchor, std::min(a.length, b.length));
    const Vec3 axis = span / distance;
    const float along = (a.length * a.length - b.length * b.length + distance * distance) / (2 * distance);
    const Vec3 center = a.anchor + axis * along;
    const float radius = std::sqrt(std::max(0.f, a.length * a.length - along * along));
    Vec3 radial = p - center - axis * dot(p - center, axis);
    if (length(radial) < 1e-5f)
        radial = cross(axis, std::abs(axis.y) > .95f ? Vec3{1, 0, 0} : Vec3{0, 1, 0});
    return center + normalized(radial) * radius;
}
} // namespace

Swing::Swing(SwingConfig c) : config_(c) {
    const float positive[] = {c.radius,    c.maxRange,     c.minRope,    c.maxSpeed,      c.yankSpeed,
                              c.yankDistance, c.yankWindow, c.fixedStep,  c.wallClearance, c.wallSlope,
                              c.bendOut,   c.bendClearance, c.bendRope,  c.maxHandSpeed};
    for (float v : positive)
        if (!std::isfinite(v) || v <= 0)
            throw std::invalid_argument("Invalid swing configuration");
    const float nonnegative[] = {
        c.gravity,          c.reelSpeed,       c.airAcceleration, c.groundAcceleration, c.zipMultiplier,
        c.maxZipImpulse,    c.jumpSpeed,       c.pointLaunchWindow, c.zipLandingWindow, c.obstructionTime,
        c.anchorClearance,  c.wallStep,        c.wallCarry,       c.wallWalkSpeed,   c.wallAcceleration,   c.wallBrake,
        c.wallJumpOut,      c.wallJumpUp,      c.wallJumpPause,   c.wallGrace,          c.crestSeconds,
        c.crestPush,        c.crestHop,        c.diveGravity,     c.diveSpeed,       c.diveSpeedRamp,
        c.diveSpeedMax,     c.overspeedDecay,  c.catchMin,        c.catchFrequency,  c.catchBounce,
        c.catchKeep,        c.swingFall,       c.swingRise,       c.jumpOffUp[0],    c.jumpOffUp[1],
        c.jumpOffUp[2],     c.jumpOffBoost[0], c.jumpOffBoost[1], c.jumpOffBoost[2], c.jumpOffMid,
        c.catchDepth,       c.boostTarget,     c.boostAccel,      c.boostJerk,       c.boostStart,
        c.momentumHigh,     c.momentumLimit,   c.momentumGravity, c.momentumTime};
    for (float v : nonnegative)
        if (!std::isfinite(v) || v < 0)
            throw std::invalid_argument("Invalid swing configuration");
    if (!(c.gameFall > 0) || !std::isfinite(c.gameFall))
        throw std::invalid_argument("Invalid game gravity");
    for (int i = 0; i < 3; ++i)
        if (!(c.jumpOffGravity[i] > 0) || !std::isfinite(c.jumpOffGravity[i]) || !(c.jumpOffFloor[i] >= 0) ||
            !std::isfinite(c.jumpOffFloor[i]))
            throw std::invalid_argument("Invalid jump off");
    if (!(c.jumpOffSwing >= 0) || !std::isfinite(c.jumpOffSwing))
        throw std::invalid_argument("Invalid jump off");
    if (!(c.reelSpin >= 0) || !std::isfinite(c.reelSpin))
        throw std::invalid_argument("Invalid winch spin");
    if (c.catchBounce >= 1 || c.catchKeep > 1 || c.catchFrequency > 10 || c.swingFall > 4 || c.swingRise > 4 ||
        !(c.jumpOffHigh > c.jumpOffMid) || c.jumpOffHigh > 90 || c.momentumHigh > 5 || c.momentumLimit < 1 ||
        c.momentumLimit > 2 || c.momentumGravity > 2 || !(c.momentumTime > 0) || c.momentumTime > 4)
        throw std::invalid_argument("Invalid elastic web, swing gravity, jump off or momentum");
    if (c.minRope > c.maxRange || c.fixedStep > .02f)
        throw std::invalid_argument("Invalid rope range or fixed timestep");
    if (!std::isfinite(c.wallReach) || c.wallReach < c.wallClearance || c.wallReach > 10 || c.wallSlope >= 1)
        throw std::invalid_argument("Invalid wall reach or slope");
    if (c.wallStep > 1)
        throw std::invalid_argument("Invalid wall step");
    if (!std::isfinite(c.mountReach) || c.mountReach < 0 || c.mountReach > 10 || !(c.mountSeconds >= 0) ||
        !(c.mountHold >= 0) || !std::isfinite(c.mountHeight))
        throw std::invalid_argument("Invalid wall mount");
    if (!std::isfinite(c.handHeight) || c.bendRope > c.minRope)
        throw std::invalid_argument("Invalid physical web");
    limit_ = c.maxSpeed;
}
void Swing::allowPhysical(bool allowed) {
    config_.physical = allowed;
}
Vec3 Swing::hold(int i) const {
    return config_.physical ? webs_[i].anchor - hands_[i].offset : webs_[i].anchor;
}
void Swing::payOut(Vec3 position, float slack) {
    if (!config_.physical || !finite(position) || !(slack >= 0))
        return;
    for (int i = 0; i < 2; ++i)
        if (auto& w = webs_[i]; w.attached)
            w.length = std::max(w.length, length(hold(i) - position) + slack);
}
bool Swing::wrap(int i, Vec3 before, const RayHit& first, const WorldQueries& world) {
    auto& w = webs_[i];
    const Vec3 pivot = w.anchor, now = body_.position;
    if (w.bent >= maxBends || !finite(before) || length(now - before) < 1e-4f)
        return false;
    const bool bend = w.bent > 0;
    const auto blocker = [&](Vec3 from) -> std::optional<RayHit> {
        const Vec3 d = pivot - from;
        const float dist = length(d);
        if (dist < .2f)
            return {};
        const auto hit = world.raycast(from, d / dist, std::max(0.f, dist - .1f));
        if (hit && finite(hit->point) && blocks(*hit, from, pivot, bend))
            return hit;
        return {};
    };
    // The line was clear a step ago: the edge it met lies in between. Halving
    // the step finds the line that just meets it, and where.
    if (blocker(before))
        return false;
    Vec3 clear = before;
    RayHit edge = first;
    float low = 0, high = 1;
    for (int n = 0; n < 5; ++n) {
        const float middle = (low + high) / 2;
        const Vec3 at = before + (now - before) * middle;
        if (const auto hit = blocker(at)) {
            high = middle;
            edge = *hit;
        } else {
            low = middle;
            clear = at;
        }
    }
    // Out of the edge: off the face the line met, and toward the line that
    // still passed it.
    const Vec3 face = finite(edge.normal) ? normalized(edge.normal) : Vec3{};
    const Vec3 line = clear - pivot;
    const float span2 = dot(line, line);
    const float along = span2 > 1e-6f ? std::clamp(dot(edge.point - pivot, line) / span2, 0.f, 1.f) : 0.f;
    Vec3 aside = pivot + line * along - edge.point;
    aside = length(aside) > 1e-4f ? normalized(aside) : Vec3{};
    Vec3 out = face + aside;
    if (length(out) < .1f)
        out = length(face) > .5f ? face : aside;
    if (length(out) < .1f)
        return false;
    out = normalized(out);
    const Vec3 point = edge.point + out * config_.bendOut;
    const float span = length(point - pivot);
    if (!finite(point) || span < config_.bendOut)
        return false;
    w.bends[w.bent++] = {pivot, out, span};
    w.anchor = point;
    // The rope beyond the bend: what is left of it, and no shorter than the
    // way to the hand is now (the bend lies a little off the straight line).
    w.length = std::max({config_.bendRope, w.length - span, length(hold(i) - now)});
    events_.push_back({EventKind::Wrap, i, .3f});
    return true;
}
void Swing::unwrap(int i, const WorldQueries& world) {
    auto& w = webs_[i];
    if (!w.bent)
        return;
    const Bend& last = w.bends[w.bent - 1];
    const Vec3 line = body_.position - last.from;
    const float span = length(line);
    if (span < .2f)
        return;
    // The straight rope from before the bend would pass the edge on its
    // outside: it lies free of it.
    const float along = std::clamp(dot(w.anchor - last.from, line) / (span * span), 0.f, 1.f);
    if (dot(last.from + line * along - w.anchor, last.out) < config_.bendOut * .5f)
        return;
    // And of everything else.
    const auto hit = world.raycast(body_.position, line * (-1 / span), std::max(0.f, span - .1f));
    if (hit && blocks(*hit, body_.position, last.from, w.bent > 1))
        return;
    w.anchor = last.from;
    w.length += last.span;
    --w.bent;
    events_.push_back({EventKind::Unwrap, i, .2f});
}
void Swing::limitSpeed(float maxSpeed) {
    if (!std::isfinite(maxSpeed) || maxSpeed <= 0)
        throw std::invalid_argument("Invalid swing speed limit");
    // Speed a dive left over the old limit bleeds off as before; otherwise
    // the new limit holds from the next step.
    const bool over = limit_ > config_.maxSpeed;
    config_.maxSpeed = maxSpeed;
    limit_ = over ? std::max(limit_, maxSpeed) : maxSpeed;
}
void Swing::setGravity(float gravity) {
    if (!std::isfinite(gravity) || gravity < 0)
        throw std::invalid_argument("Invalid swing gravity");
    config_.gravity = gravity;
}
void Swing::setModel(SwingModel model) {
    applySwingModel(config_, model);
}
void Swing::allowAirAnchors(bool allowed) {
    config_.airAnchors = allowed;
}
void Swing::allowWalls(bool allowed) {
    config_.walls = allowed;
    if (!allowed)
        leaveWall();
}
void Swing::leaveWall() {
    if (wall_.on)
        events_.push_back({EventKind::WallOff, -1, .2f});
    wall_ = {};
    wallLost_ = 0;
    wrapWay_ = {};
}
void Swing::joinWall(const RayHit& hit) {
    const Vec3 n = hit.normal;
    const float into = -dot(body_.velocity, n);
    Vec3 along = body_.velocity + n * into;
    // Part of the speed into the wall goes on along it, the more the body
    // already went that way: grazing the wall it keeps its speed, head on
    // it stops.
    if (const float speed = length(along), total = length(body_.velocity); into > 0 && speed > 1e-3f)
        along = along * std::min(total / speed, 1 + config_.wallCarry * into / total);
    // What still closes on the wall ends at its clearance (holdWall).
    body_.velocity = along - n * into;
    if (!wall_.on) {
        wall_.seconds = 0;
        events_.push_back({EventKind::WallOn, -1, std::clamp(into / 20, .2f, 1.f)});
    }
    wall_.on = true;
    wall_.normal = n;
    wall_.point = hit.point;
    wall_.distance = dot(body_.position - hit.point, n);
    wallLost_ = crest_ = 0;
    wrapWay_ = {};
}
void Swing::offerWall(const RayHit& wall) {
    const Vec3 n = normalized(wall.normal);
    if (!config_.walls || !wall.fixed || !finite(wall.point) || length(n) < .5f ||
        std::abs(n.y) > config_.wallSlope)
        return;
    mountWall_ = {wall.point, n, wall.surface, true};
    offered_ = true;
}
void Swing::senseWall(float dt, const Input& in, const WorldQueries& world) {
    sinceWallJump_ += dt;
    const bool offered = offered_;
    offered_ = mounting_ = false;
    if (!config_.walls) {
        leaveWall();
        crest_ = mount_ = mountLeft_ = 0;
        return;
    }
    const Vec3 at = body_.position;
    // The wall a ray meets from outside: fixed, steep, facing the ray. The
    // game's world takes unit directions only.
    const auto wallAlong = [&](Vec3 from, Vec3 direction, float reach) -> std::optional<RayHit> {
        direction = normalized(direction);
        if (!finite(from) || length(direction) < .9995f || !(reach >= .02f))
            return {};
        const auto hit = world.raycast(from, direction, std::min(reach, 100.f));
        if (!hit || !hit->fixed || !finite(hit->point) || !finite(hit->normal))
            return {};
        const Vec3 n = normalized(hit->normal);
        if (length(n) < .5f || std::abs(n.y) > config_.wallSlope || dot(n, direction) > -.2f)
            return {};
        return RayHit{hit->point, n, hit->surface, true};
    };
    // The wall the stick walks the body at, level: within the mount's reach,
    // and faced within `facing` (a cosine) of head on.
    const auto walkedAt = [&](float facing) -> std::optional<RayHit> {
        const Vec3 stick = finite(in.move) ? Vec3{in.move.x, 0, in.move.z} : Vec3{};
        // From the tilt at which the game walks the player.
        if (length(stick) < .35f)
            return {};
        const Vec3 way = normalized(stick);
        const auto hit = wallAlong(at, way, config_.mountReach);
        if (!hit || dot(hit->normal, way) > -facing)
            return {};
        return hit;
    };
    if (offered) {
        mountLeft_ = config_.mountHold;
        mountOffered_ = true;
    }
    if (body_.grounded) {
        leaveWall();
        crest_ = 0;
        if (offered) {
            mount_ = 0;
            mounting_ = true;
            return;
        }
        auto wall = walkedAt(.7f);
        if (wall) {
            const auto above =
                wallAlong(at + Vec3{0, config_.mountHeight, 0}, -wall->normal, config_.mountReach + .5f);
            if (!above || dot(above->normal, wall->normal) < .9f)
                wall.reset();
        }
        mount_ = wall ? mount_ + dt : 0;
        if (wall && mount_ >= config_.mountSeconds) {
            mountWall_ = *wall;
            mountLeft_ = config_.mountHold;
            mountOffered_ = false;
            mounting_ = true;
        } else {
            // A jump already asked for still ends on its wall.
            mountLeft_ = std::max(0.f, mountLeft_ - dt);
        }
        return;
    }
    mount_ = 0;
    if (mountLeft_ > 0) {
        // Off the ground by the mount's jump: onto its wall, up it at a walk
        // at most. Once more if the game stood the body again meanwhile.
        mountLeft_ -= dt;
        const Vec3 n = mountWall_.normal;
        if (const float distance = dot(at - mountWall_.point, n);
            !wall_.on && distance > -.2f && distance <= config_.mountReach + .5f) {
            body_.velocity = mountOffered_ ? Vec3{}
                                           : limited(body_.velocity - n * dot(body_.velocity, n),
                                                     config_.wallWalkSpeed);
            joinWall(mountWall_);
            return;
        }
    }
    if (wall_.on) {
        wall_.seconds += dt;
        const Vec3 n = wall_.normal;
        const Vec3 along = body_.velocity - n * dot(body_.velocity, n);
        const float speed = length(along);
        const bool rounding = length(wrapWay_) > .5f;
        // A wall ahead along this one (an inside corner): onto it.
        if (speed > 1 && !rounding)
            if (const auto next = wallAlong(at, along, speed * dt + config_.wallClearance + .2f);
                next && dot(next->normal, n) < .9f) {
                joinWall(*next);
                return;
            }
        const float closing = std::max(0.f, -dot(body_.velocity, n));
        // Start outside a small projecting sill, so the ray still finds its
        // front when the body's centre has already passed its edge. Look
        // ahead before the capsule hits it and below the harness until the
        // feet have cleared it. All samples must stay on this facade.
        const float step = config_.wallStep;
        const auto facade = [&](Vec3 from) -> std::optional<RayHit> {
            auto hit = wallAlong(from + n * step, -n,
                                 config_.wallReach + .3f + 2 * step + closing * dt);
            if (!hit)
                return {};
            const float distance = dot(at - hit->point, n);
            if (distance < -step || distance > config_.wallReach + step)
                return {};
            return hit;
        };
        auto hit = facade(at);
        if (!rounding && step > 0) {
            const Vec3 ahead = speed > .3f ? along / speed : Vec3{};
            const float look = config_.wallClearance + std::min(speed * .2f, 2.f);
            for (const Vec3 offset : {ahead * look, Vec3{0, -1.1f, 0}}) {
                const float span = length(offset);
                if (span < .01f)
                    continue;
                // An endpoint alone skips a thin sill. Trace along the
                // facade as well, then sample just past its lip to find its
                // outward face (the lip's own normal points up or down).
                const float depth = std::max(dot(at - wall_.point, n),
                                             hit ? dot(at - hit->point, n) : 0.f);
                const Vec3 start = at - n * (depth - .05f), way = offset / span;
                const auto edge = world.raycast(start, way, span);
                std::optional<RayHit> lip;
                if (edge && edge->fixed && finite(edge->point)) {
                    const float travel = dot(edge->point - start, way);
                    if (travel >= 0 && travel <= span)
                        lip = facade(at + way * (travel + .02f));
                }
                for (const auto& next : {facade(at + offset), lip}) {
                    if (!next || dot(next->normal, n) < .9f)
                        continue;
                    // A nearby parallel face may extend our contact footprint;
                    // a new building or a deep overhang must not pull us onto it.
                    const float rise = dot(next->point - wall_.point, n);
                    if (std::abs(rise) > step || (hit && dot(next->point - hit->point, n) <= 0))
                        continue;
                    hit = next;
                }
            }
        }
        if (hit) {
            // A facade's sills and pilasters turn the normal for a moment.
            wall_.normal = normalized(n + (hit->normal - n) * (1 - std::exp(-dt / .08f)));
            wall_.point = hit->point;
            wall_.distance = dot(at - hit->point, wall_.normal);
            wallLost_ = 0;
            wrapWay_ = {};
            // Carried off it (a web's pull): the wall lets go.
            const float reach = dot(body_.velocity, n) < .2f
                                    ? std::max(config_.wallReach, config_.wallClearance + step)
                                    : config_.wallReach;
            if (wall_.distance > reach && closing <= 0)
                leaveWall();
            return;
        }
        // Rounding a corner, the body is not in front of the next face yet.
        if (rounding && (wrapLeft_ -= dt) > 0)
            return;
        // At a walk, round the wall's end onto its next face. The body is
        // beside that face's edge, a clearance out along the old wall: it
        // goes on round the corner by itself (wallMotion) until the next
        // face is beside it.
        if (!rounding && speed > .3f && speed <= config_.wallWalkSpeed + 1) {
            const Vec3 ahead = along / speed;
            const Vec3 past = at + ahead * (speed * dt + .3f) - n * (wall_.distance + .4f);
            if (const auto next = wallAlong(past, -ahead, speed * dt + 1.5f)) {
                wall_.normal = next->normal;
                wall_.point = next->point;
                wall_.distance = dot(at - next->point, next->normal);
                wallLost_ = 0;
                wrapWay_ = normalized(-n - next->normal * dot(-n, next->normal));
                wrapLeft_ = .8f;
                body_.velocity = {};
                return;
            }
        }
        wrapWay_ = {};
        wallLost_ += dt;
        if (wallLost_ > config_.wallGrace) {
            // Over the wall's top edge: on over it, onto the roof.
            if (along.y > 1) {
                crest_ = config_.crestSeconds;
                crestNormal_ = n;
                body_.velocity.y = std::max(body_.velocity.y, config_.crestHop);
            }
            leaveWall();
        }
        return;
    }
    if (sinceWallJump_ < config_.wallJumpPause)
        return;
    // Ahead of the body and to both its sides: a wall it meets head on, and
    // one it grazes.
    const Vec3 level{body_.velocity.x, 0, body_.velocity.z};
    std::optional<RayHit> wall;
    if (length(level) >= .5f) {
        const Vec3 heading = normalized(level), side{-heading.z, 0, heading.x};
        const float reach = 1.5f * config_.wallClearance + length(body_.velocity) * dt + .2f;
        float nearest{};
        for (const Vec3 direction : {heading, side, -side}) {
            const auto hit = wallAlong(at, direction, reach);
            if (!hit)
                continue;
            // Coming into it, and within its clearance by the end of this step.
            const float distance = dot(at - hit->point, hit->normal),
                        closing = -dot(body_.velocity, hit->normal);
            if (closing > .5f && distance - closing * dt <= config_.wallClearance + .05f &&
                (!wall || distance < nearest)) {
                wall = hit;
                nearest = distance;
            }
        }
    }
    // Pushed at a wall beside the body, the stick takes it with no speed
    // into it, and none off it: a fall down a facade, a jump beside one.
    if (!wall && (wall = walkedAt(.5f)))
        body_.velocity -= wall->normal * std::max(0.f, dot(body_.velocity, wall->normal));
    if (wall)
        joinWall(*wall);
}
void Swing::wallMotion(float dt, const Input& in) {
    const Vec3 n = wall_.normal;
    const float out = dot(body_.velocity, n);
    Vec3 along = body_.velocity - n * out;
    if (length(wrapWay_) > .5f) {
        // Round a corner, slowly enough to clear its edge while the next
        // face pushes the body out to its clearance.
        body_.velocity = wrapWay_ * 3 + n * out;
        return;
    }
    // What of the stick points into the wall goes up it: pushed at a wall,
    // the body climbs.
    const Vec3 up = normalized(Vec3{0, 1, 0} - n * n.y);
    Vec3 move = finite(in.move) ? limited(in.move, 1) : Vec3{};
    const float into = -dot(move, n);
    move = limited(move + n * into + up * into, 1);
    if (const float tilt = length(move); tilt > .05f) {
        // The stick walks the wall; a faster run keeps what it has that way.
        const Vec3 way = move / tilt;
        const Vec3 wanted = way * std::max(dot(along, way), config_.wallWalkSpeed * tilt);
        along += limited(wanted - along, config_.wallAcceleration * dt);
    } else if (!webs_[0].attached && !webs_[1].attached) {
        if (const float speed = length(along); speed > 1e-4f) {
            const float slower = speed > config_.wallWalkSpeed ? speed - config_.wallBrake * dt
                                                               : speed * std::exp(-6 * dt) - dt;
            along = along * (std::max(0.f, slower) / speed);
        }
    }
    body_.velocity = limited(along + n * out, config_.maxSpeed);
}
void Swing::holdWall(float dt, float before, const World* collision) {
    const Vec3 n = wall_.normal;
    const float distance = dot(body_.position - wall_.point, n), out = dot(body_.velocity, n);
    // No nearer than the clearance; a body already inside it (the facade
    // stepped out, a corner) comes out at a walk. One the wall lets drift
    // off comes back slowly, unless something carries it away.
    const float nearest = std::min(config_.wallClearance, before + 4 * dt);
    float shift{};
    if (distance < nearest)
        shift = nearest - distance;
    else if (distance > config_.wallClearance + .1f && out < .2f)
        shift = -std::min(distance - config_.wallClearance, 2 * dt);
    if (shift != 0)
        move(body_.position + n * shift, collision);
    if (out < 0 && distance + shift <= config_.wallClearance + 1e-3f)
        body_.velocity -= n * out;
    wall_.distance = distance + shift;
}
void Swing::reset(Body b) {
    if (!finite(b.position) || !finite(b.velocity))
        throw std::invalid_argument("Invalid body");
    body_ = b;
    webs_ = {};
    hands_ = {};
    events_.clear();
    accumulator_ = 0;
    sinceZip_ = sinceLanding_ = 100;
    zipSpeed_ = 0;
    zipDirection_ = {};
    jumpHeld_ = jumpQueued_ = false;
    leapQueued_ = 0;
    leapHeld_ = false;
    wall_ = {};
    wallLost_ = crest_ = 0;
    wrapWay_ = {};
    sinceWallJump_ = 100;
    mount_ = mountLeft_ = 0;
    mounting_ = offered_ = mountOffered_ = false;
    limit_ = config_.maxSpeed;
    carry_ = diveTime_ = 0;
    diving_ = false;
    swing_ = lastSwing_ = {};
    swinging_ = false;
    boostRamp_ = 0;
    swingMode_ = 0;
    sinceAttach_ = 100;
    momentum_ = 0;
    sinceMomentum_ = sinceLetGo_ = sinceJumpOff_ = sinceYank_ = 100;
    surges_ = 0;
}
void Swing::release(int i, EventKind reason) {
    const bool was = webs_[i].attached;
    if (was)
        events_.push_back({reason, i, .2f});
    webs_[i] = {};
    auto& h = hands_[i];
    h.zipUsed = false;
    h.reeling = false;
    h.pullDistance = h.pullTime = h.obstructed = h.winch = 0;
    h.stretch = h.stretchSpeed = h.give = 0;
    h.drawn = {};
    if (was && !webs_[1 - i].attached)
        endSwing();
}
void Swing::beginSwing() {
    swing_ = {};
    swing_.swings = lastSwing_.swings + 1;
    swing_.entry = swing_.peak = length(body_.velocity);
    swing_.fall = -body_.velocity.y;
    swingStartY_ = body_.position.y;
    swinging_ = true;
}
void Swing::endSwing() {
    if (!swinging_)
        return;
    swing_.exit = length(body_.velocity);
    swing_.drop = swingStartY_ - body_.position.y;
    swing_.momentum = momentum_;
    lastSwing_ = swing_;
    swinging_ = false;
}
std::array<float, 2> Swing::stretchRopes(float dt) {
    std::array<float, 2> sling{};
    const float omega = 6.2831853f * config_.catchFrequency;
    // A damped spring that takes a speed at its length comes back to it at
    // exp(-zeta pi / sqrt(1 - zeta^2)) of that speed: catchBounce.
    const float bounce = std::max(config_.catchBounce, 1e-3f), lnBounce = std::log(bounce);
    const float zeta = -lnBounce / std::sqrt(9.8696044f + lnBounce * lnBounce);
    // The share of the speed a catch takes that the bounce does not give
    // back, kept along the swing: with all of it the catch keeps the speed.
    const float keep = config_.catchKeep * (1 - bounce * bounce);
    // Caught at a speed, a spring of omega stretches speed / omega times this at the deepest.
    const float lift = std::sqrt(1 - zeta * zeta);
    const float deepest = std::exp(-zeta * std::atan2(lift, zeta) / lift);
    for (int i = 0; i < 2; ++i) {
        auto& h = hands_[i];
        auto& w = webs_[i];
        h.give = 0;
        if (!w.attached || omega <= 0) {
            h.stretch = h.stretchSpeed = 0;
            continue;
        }
        // To what the rope holds the body to (the hand's point with physical webs).
        const Vec3 delta = body_.position - hold(i);
        const float dist = length(delta);
        if (dist < 1e-5f)
            continue;
        const Vec3 radial = delta / dist;
        if (h.stretch <= 0 && h.stretchSpeed == 0) {
            // The rope pulled taut this step, after slack or as it attached,
            // with the body going away from the rope's end faster than
            // catchMin (a reeled rope's end comes in at its winch). Going
            // round the anchor on a rope turns the velocity outward by
            // speed^2 dt / distance each step: that is the swing, not a catch.
            const Vec3 along = body_.velocity - radial * dot(body_.velocity, radial);
            const float out = dot(body_.velocity, radial) + h.winch - dot(along, along) * dt / dist;
            if (h.taut || dist < w.length - nearlyTaut || out <= config_.catchMin)
                continue;
            // A faster catch stiffens the spring to stay within catchDepth.
            h.omega = config_.catchDepth > 0 ? std::max(omega, out * deepest / config_.catchDepth) : omega;
            h.stretch = std::clamp(dist - w.length, 0.f, config_.catchDepth > 0 ? config_.catchDepth : dist);
            h.stretchSpeed = out;
            w.length += h.stretch;
            h.give = out; // the rope's end goes out with the body: nothing is cut
            if (swinging_)
                ++swing_.catches;
            events_.push_back({EventKind::Catch, i, std::clamp(out / 20, .2f, 1.f)});
            continue;
        }
        // Stretching, the spring slows the rope's end; springing back, it
        // carries the body in (Web::length follows it, the constraint the body).
        const float before = h.stretchSpeed, spring = h.omega > 0 ? h.omega : omega;
        h.stretchSpeed += (-spring * spring * h.stretch - 2 * zeta * spring * h.stretchSpeed) * dt;
        float next = h.stretch + h.stretchSpeed * dt;
        if (before > 0 && dist >= w.length - .05f) {
            const float after = std::max(0.f, h.stretchSpeed);
            sling[i] = keep * .5f * (before * before - after * after);
        }
        if (next <= 0) {
            next = 0; // back at its length: the catch is over
            h.stretchSpeed = 0;
        }
        const float change = next - h.stretch;
        h.stretch = next;
        w.length = std::max(config_.minRope, w.length + change);
        h.give = change / dt;
        if (swinging_)
            swing_.deepest = std::max(swing_.deepest, next);
    }
    return sling;
}
bool Swing::blocks(const RayHit& hit, Vec3 from, Vec3 anchor, bool bend) const {
    const float clearance =
        bend ? config_.bendClearance : std::max(config_.anchorClearance, .1f * length(anchor - from));
    return length(hit.point - anchor) > clearance && length(hit.point - from) > config_.radius;
}
std::array<float, 2> Swing::reeled(std::array<float, 2> rates, float seconds) const {
    std::array<float, 2> lengths{webs_[0].length, webs_[1].length}, taken{};
    for (int i = 0; i < 2; ++i)
        if (webs_[i].attached) {
            // A rope that bends round an edge may be shorter beyond it than a
            // web can be shot: no winch lengthens it.
            const float wanted =
                std::max(std::min(config_.minRope, lengths[i]), lengths[i] - rates[i] * seconds);
            taken[i] = std::max(0.f, lengths[i] - wanted);
            lengths[i] = wanted;
        }
    if (webs_[0].attached && webs_[1].attached) {
        // Opposing winches cannot shorten their combined length below
        // the span between anchors. Leave clearance for the body too.
        // Each gives back in proportion to what it took: when one hand's
        // winch kept the last of the rope, it kicked the body to its side.
        const float span = length(webs_[0].anchor - webs_[1].anchor) + config_.radius * 2;
        const float took = taken[0] + taken[1];
        const float back = std::min(span - lengths[0] - lengths[1], took);
        if (back > 0)
            for (int i = 0; i < 2; ++i)
                lengths[i] += back * taken[i] / took;
    }
    return lengths;
}
float Swing::follow(int i, Vec3 radial) const {
    const auto& other = webs_[1 - i];
    if (!other.attached)
        return 1;
    const Vec3 delta = body_.position - hold(1 - i);
    const float dist = length(delta);
    if (dist < other.length - nearlyTaut || dist < 1e-5f)
        return 1; // the other rope is slack
    // Two taut ropes at an angle carry the body faster than either shortens,
    // without bound as they come to oppose each other. Follow that exactly up
    // to a right angle between them; past it, hold the carried speed there.
    const float cosHalfAngle = std::sqrt(std::max(0.f, (1 + dot(radial, delta / dist)) / 2));
    return std::min(1.f, cosHalfAngle * 1.4142135f);
}
WebShot Swing::shot(Pose aim, Vec3 from, const WorldQueries& world) const {
    WebShot out;
    const Vec3 forward = aim.orientation.rotate({0, 0, -1});
    if (!finite(aim.position) || !finite(from) || !finite(forward) || length(forward) < .9f || length(forward) > 1.1f)
        return out;
    const Vec3 direction = normalized(forward);
    out.hit = world.raycast(aim.position, direction, config_.maxRange);
    const bool airAnchor = !out.hit && config_.airAnchors;
    if (!out.hit && !airAnchor)
        return out;
    const RayHit anchor = airAnchor ? RayHit{aim.position + direction * config_.maxRange, {}, 0, true} : *out.hit;
    // Confirm the body has a clear line to the same anchor. Hands cannot
    // shoot through a wall while the body remains on its other side.
    if (anchor.fixed && (airAnchor || world.exists(anchor.surface)) && finite(anchor.point)) {
        const Vec3 delta = anchor.point - from;
        const float dist = length(delta);
        const auto obstacle = world.raycast(from, normalized(delta), std::max(0.0f, dist - .08f));
        if (length(anchor.point - aim.position) <= config_.maxRange + .01f && dist >= config_.minRope &&
            !(obstacle && blocks(*obstacle, from, anchor.point)))
            out.web = Web{true, anchor.point, anchor.surface, dist, 0, airAnchor};
    }
    return out;
}
void Swing::releaseAll() {
    for (int i = 0; i < 2; ++i) {
        release(i);
        hands_[i].sample = false;
        hands_[i].held = true; // a grip already held is not a new shot
    }
    jumpQueued_ = false;
    accumulator_ = 0;
    sinceZip_ = sinceLanding_ = 100;
    zipSpeed_ = 0;
    zipDirection_ = {};
    diving_ = false;
    diveTime_ = 0;
}
void Swing::inputs(float dt, const Input& in, const WorldQueries& world) {
    jumpQueued_ |= in.jump && !jumpHeld_;
    const bool leap = std::isfinite(in.leap) && in.leap > 0;
    if (leap && !leapHeld_)
        leapQueued_ = std::min(in.leap, 1.f);
    leapHeld_ = leap;
    jumpHeld_ = in.jump;
    for (int i = 0; i < 2; ++i) {
        const auto& h = in.hands[i];
        auto& s = hands_[i];
        auto& w = webs_[i];
        const bool valid = h.tracked && finite(h.aim.position) && finite(h.gripRelativeToHead) &&
                           std::isfinite(in.trackingYaw) && std::isfinite(h.trigger) && std::isfinite(h.grip);
        const Vec3 forward = h.aim.orientation.rotate({0, 0, -1});
        if (!valid || !finite(forward) || length(forward) < .9f || length(forward) > 1.1f) {
            release(i, EventKind::TrackingLost);
            s.sample = false;
            s.held = true; // require a full release before the next shot
            s.rate = {};
            continue;
        }
        // The hand that holds the rope: where the grip is from the head,
        // turned into the world. It follows a jump of that turn (a snap turn)
        // no faster than a hand moves, and its rate is the arm's own motion.
        if (config_.physical) {
            const Quat turn = trackingTurn(in);
            const Vec3 offset = turn.rotate(h.gripRelativeToHead) + Vec3{0, config_.handHeight, 0};
            const float over = std::max({dt, stepSeconds_, 1e-3f});
            if (s.sample) {
                s.offset += limited(offset - s.offset, config_.maxHandSpeed * dt);
                s.rate = limited(turn.rotate(h.gripRelativeToHead - s.previous) / over, config_.maxHandSpeed);
            } else {
                s.offset = offset;
                s.rate = {};
            }
        } else {
            s.offset = s.rate = {};
        }
        // Squeezing the grip shoots this hand's web; holding it keeps the web.
        const bool held = s.held ? h.grip > .35f : h.grip > .65f;
        const bool shoot = held && !s.held;
        s.held = held;
        if (w.attached && (!held || (!w.airAnchor && !world.exists(w.surface)))) {
            // Letting go of the swing on its way up raises the momentum meter
            // (SwingReleaseData ReleaseData: 0.25, to 3, once a second).
            if (!held && !webs_[1 - i].attached && !body_.grounded && body_.velocity.y > 0)
                gainMomentum(.25f, 3, &sinceLetGo_, 1);
            release(i);
        }
        if (!w.attached && shoot) {
            if (const auto fired = shot(h.aim, body_.position, world); fired.web) {
                if (!webs_[1 - i].attached)
                    beginSwing();
                w = *fired.web;
                // To the hand that holds it.
                w.length = std::max(config_.minRope, length(hold(i) - body_.position));
                s.taut = false;
                s.stretch = s.stretchSpeed = s.give = 0;
                // The swing carries the fall it catches (fallCarry).
                carry_ = std::max(carry_, config_.maxSpeed * fallCarry(-body_.velocity.y));
                // A new swing, from the top of its arc (the game's mode 0).
                boostRamp_ = 0;
                swingMode_ = 0;
                sinceAttach_ = 0;
                s.triggerReleased = false;
                s.reeling = false;
                s.zipUsed = false;
                s.pullDistance = s.pullTime = s.obstructed = 0;
                events_.push_back({EventKind::Attach, i, .65f});
            } else {
                events_.push_back({EventKind::Miss, i, .15f});
            }
        }
        if (w.attached) {
            if (h.trigger < .35f)
                s.triggerReleased = true;
            const bool reel = s.triggerReleased && h.trigger > .65f;
            if (reel && !s.reeling) {
                // A yank/forward swing leaves slack. Starting a winch should
                // engage at the current distance, not spend seconds winding in
                // invisible slack before the player feels any response.
                w.length = std::min(w.length, std::max(std::min(config_.minRope, w.length),
                                                       length(hold(i) - body_.position)));
                s.stretch = s.stretchSpeed = 0; // the winch takes the rope as it is
            }
            s.reeling = reel;
            // Motion must be both fast and deliberate within a short window.
            // Head-relative samples remove virtual movement and room-scale translation.
            if (s.sample && !s.zipUsed) {
                const Vec3 delta = trackingTurn(in).rotate(h.gripRelativeToHead - s.previous);
                const float pull = -dot(delta, normalized(w.anchor - body_.position));
                if (pull > 0) {
                    s.pullDistance += pull;
                    s.pullTime += dt;
                    if (s.pullTime > config_.yankWindow) {
                        s.pullDistance = pull;
                        s.pullTime = dt;
                    }
                    const float speed = pull / dt;
                    if (s.pullDistance >= config_.yankDistance && speed >= config_.yankSpeed) {
                        const float impulse = std::min(speed * config_.zipMultiplier, config_.maxZipImpulse);
                        const Vec3 before = body_.velocity;
                        body_.velocity =
                            limited(body_.velocity + normalized(w.anchor - body_.position) * impulse, limit_);
                        if (swinging_) {
                            swing_.yank += gained(before, body_.velocity);
                            ++swing_.yanks;
                        }
                        // ZipData: to 5, once in 2 s (its rise is compiled in
                        // the game, not in the asset: 0.5 here).
                        gainMomentum(.5f, 5, &sinceYank_, 2);
                        sinceZip_ = 0;
                        zipSpeed_ = length(body_.velocity);
                        zipDirection_ = normalized(Vec3{body_.velocity.x, 0, body_.velocity.z});
                        s.zipUsed = true;
                        events_.push_back({EventKind::Zip, i, 1});
                    }
                } else {
                    s.pullDistance = s.pullTime = 0;
                }
            }
        }
        s.previous = h.gripRelativeToHead;
        s.sample = true;
    }
}
void Swing::move(Vec3 target, const World* collision) {
    if (!collision) {
        // Internal prediction only. No collision has been performed here.
        body_.position = target;
        return;
    }
    const auto result = collision->sweep(body_.position, target, config_.radius);
    if (!finite(result.position))
        return;
    body_.position = result.position;
    if (result.hit) {
        const Vec3 n = normalized(result.normal);
        const float vn = dot(body_.velocity, n);
        if (vn < 0)
            body_.velocity -= n * vn;
        if (n.y > .65f)
            body_.grounded = true;
    }
}
void Swing::steerLimit(float dt, const Input& in) {
    const bool attached = webs_[0].attached || webs_[1].attached;
    if (!attached)
        carry_ = 0;
    // A dive acts in free flight only: a web, the ground or a wall ends it.
    const bool dove = diving_;
    const float dived = diveTime_;
    diving_ = in.dive && !attached && !body_.grounded && !wall_.on;
    diveTime_ = diving_ ? diveTime_ + dt : 0;
    // A dive a web catches raises the momentum meter (FallData: 0.5 after
    // half a second of dive, up to 1.25 after 2 s; to 5).
    if (dove && !diving_ && attached && dived >= .5f)
        gainMomentum(.5f + .75f * std::min(1.f, (dived - .5f) / 1.5f), 5);
    float target = config_.maxSpeed;
    if (attached)
        target = std::max(target, carry_);
    // High momentum lets the swing go faster (momentumLimit).
    if (attached && config_.momentum && momentum_ >= config_.momentumHigh)
        target = std::max(target, config_.maxSpeed * config_.momentumLimit);
    if (diving_)
        target = std::max(target, std::min(config_.diveSpeedMax,
                                           config_.diveSpeed + config_.diveSpeedRamp * diveTime_));
    target = std::min(target, std::max(config_.maxSpeed, config_.diveSpeedMax));
    limit_ = target >= limit_ ? target : std::max(target, limit_ - config_.overspeedDecay * dt);
}
void Swing::step(float dt, const Input& in, const WorldQueries& world, const World* collision) {
    sinceZip_ += dt;
    sinceLanding_ += dt;
    sinceLetGo_ += dt;
    sinceJumpOff_ += dt;
    sinceYank_ += dt;
    sinceAttach_ += dt;
    decayMomentum(dt);
    const bool wasGrounded = body_.grounded;
    if (leapQueued_ > 0) {
        // A charged leap: up, and along the stick. From the ground only.
        if (body_.grounded && !wall_.on) {
            const Vec3 stick = finite(in.move) ? Vec3{in.move.x, 0, in.move.z} : Vec3{};
            const float tilt = std::min(length(stick), 1.f);
            const Vec3 along = tilt > .1f ? stick * (config_.leapForward * (.5f + .5f * leapQueued_) / length(stick) * tilt)
                                          : Vec3{};
            body_.velocity = limited(
                along + Vec3{0, config_.leapUp + (config_.leapUpFull - config_.leapUp) * leapQueued_, 0},
                config_.maxSpeed);
            body_.grounded = false;
            events_.push_back({EventKind::Leap, -1, leapQueued_});
            jumpQueued_ = false;
        }
        leapQueued_ = 0;
    }
    if (jumpQueued_) {
        if (wall_.on) {
            // Off the wall: out from it and up, with what the body had along it.
            const Vec3 n = wall_.normal;
            body_.velocity = limited(body_.velocity - n * dot(body_.velocity, n) + n * config_.wallJumpOut +
                                         Vec3{0, config_.wallJumpUp, 0},
                                     config_.maxSpeed);
            leaveWall();
            sinceWallJump_ = 0;
            mountLeft_ = 0;
            events_.push_back({EventKind::WallJump, -1, 1});
        } else if (body_.grounded) {
            body_.velocity.y = config_.jumpSpeed;
            if (pointLaunchReady()) {
                Vec3 horizontal = normalized(Vec3{body_.velocity.x, 0, body_.velocity.z});
                if (length(horizontal) < .1f)
                    horizontal = normalized(Vec3{in.move.x, 0, in.move.z});
                if (length(horizontal) < .1f)
                    horizontal = zipDirection_;
                const float speed = std::min(zipSpeed_, config_.maxSpeed);
                body_.velocity.x = horizontal.x * speed;
                body_.velocity.z = horizontal.z * speed;
                body_.velocity.y = config_.jumpSpeed * 1.5f;
                sinceZip_ = 100;
                events_.push_back({EventKind::PointLaunch, -1, 1});
            }
            body_.grounded = false;
        } else if (webs_[0].attached || webs_[1].attached) {
            jumpOff();
        }
        jumpQueued_ = false;
    }
    steerLimit(dt, in);
    const float wallBefore = wall_.on ? dot(body_.position - wall_.point, wall_.normal) : 0.f;
    const Vec3 before = body_.position;
    if (wall_.on) {
        // On a wall the stick walks it, and gravity does not pull along it.
        wallMotion(dt, in);
        body_.grounded = false;
    } else {
        Vec3 steering = finite(in.move) ? limited(Vec3{in.move.x, 0, in.move.z}, 1) : Vec3{};
        const Vec3 unsteered = body_.velocity;
        body_.velocity +=
            steering * ((body_.grounded ? config_.groundAcceleration : config_.airAcceleration) * dt);
        if (counting())
            swing_.steer += gained(unsteered, body_.velocity);
        if (body_.grounded) {
            const float damping = std::exp(-5 * dt);
            body_.velocity.x *= damping;
            body_.velocity.z *= damping;
        }
        // A native prediction has no sweep to cancel gravity at the floor. Keep
        // the measured support until a jump or rope pull actually lifts the body.
        const bool supported = !collision && body_.grounded && body_.velocity.y <= 0;
        if (supported) {
            body_.velocity.y = 0;
        } else {
            const float g = diving_ ? std::max(config_.gravity, config_.diveGravity) : config_.gravity;
            // The swing's pump: a taut web holding the body up changes
            // gravity on the way down and on the way up; high momentum
            // lightens it (momentumGravity; the game's curve: on the way up).
            float angle{}, rope{};
            const bool hanging = hung(&angle, &rope);
            // Carried up past the bottom of the arc (the game's mode 1), and
            // falling back from the top (its mode 2).
            if (hanging && swingMode_ == 0 && body_.velocity.y > 0)
                swingMode_ = 1;
            else if (hanging && swingMode_ == 1 && body_.velocity.y < 0)
                swingMode_ = 2;
            const float lighter =
                config_.momentum && momentum_ >= config_.momentumHigh ? config_.momentumGravity : 1.f;
            const float swung =
                !hanging ? g
                : config_.swingCurve
                    ? g * swingGravity(rope)
                    : g * (body_.velocity.y < 0 ? config_.swingFall : config_.swingRise) * lighter;
            if (counting())
                swing_.pump -= (swung - g) * body_.velocity.y * dt;
            body_.velocity.y -= swung * dt;
            // Swinging raises the momentum meter (SwingData: 0.125 a second
            // with the rope 20 degrees from straight down, 0.2 at 90; to 4).
            if (hanging)
                gainMomentum((.125f + .075f * std::clamp((angle - 20) / 70, 0.f, 1.f)) * dt / config_.momentumTime, 4);
            boostSwing(dt);
        }
        if (crest_ > 0) {
            // Over a wall's top edge: on over the roof behind it.
            crest_ -= dt;
            body_.velocity -= crestNormal_ * (config_.crestPush * dt);
        }
        const Vec3 uncapped = body_.velocity;
        body_.velocity = limited(body_.velocity, limit_);
        if (counting())
            swing_.cap -= gained(uncapped, body_.velocity);
        body_.grounded = supported;
    }
    move(body_.position + body_.velocity * dt, collision);
    for (int i = 0; i < 2; ++i) {
        auto& w = webs_[i];
        if (!w.attached)
            continue;
        if (!w.airAnchor && !world.exists(w.surface)) {
            release(i, EventKind::Obstructed);
            continue;
        }
        const Vec3 d = w.anchor - body_.position;
        const float dist = length(d);
        const auto hit = world.raycast(body_.position, normalized(d), std::max(0.0f, dist - .1f));
        // Swinging along a facade, the line grazes ledges and sills near the
        // anchor every few frames. Only a wall that stays in the way releases:
        // one a physical web cannot bend round (no edge found, too many).
        auto& obstructed = hands_[i].obstructed;
        bool blocked = hit && blocks(*hit, body_.position, w.anchor, w.bent > 0);
        if (config_.physical) {
            if (blocked)
                blocked = !wrap(i, before, *hit, world);
            else
                unwrap(i, world);
        }
        obstructed = blocked ? obstructed + dt : 0;
        if (obstructed > 0 && obstructed >= config_.obstructionTime) {
            release(i, EventKind::Obstructed);
            continue;
        }
    }
    std::array<float, 2> rates{};
    for (int i = 0; i < 2; ++i)
        if (webs_[i].attached && hands_[i].reeling)
            rates[i] = config_.reelSpeed;
    const auto lengths = reeled(rates, dt);
    for (int i = 0; i < 2; ++i) {
        hands_[i].winch = (webs_[i].length - lengths[i]) / dt;
        webs_[i].length = lengths[i];
    }
    const auto sling = stretchRopes(dt);
    // What each rope holds the body to: its anchor, or with physical webs
    // the point that keeps the hand on it.
    const Rope ropes[2] = {{hold(0), webs_[0].length}, {hold(1), webs_[1].length}};
    if (webs_[0].attached && webs_[1].attached)
        move(projectTwoWebs(body_.position, ropes[0], ropes[1]), collision);

    // Unilateral, tension-only constraints. Slack rope never pushes the player.
    // Alternate order to reduce left/right bias with two simultaneous ropes.
    float pull[2]{}; // speed each rope has taken out of the body in this step
    float cut[2]{}, cutEnergy[2]{}; // the body's own outward speed each cut, and what that took
    bool held[2]{};                 // whether each rope held the body in this step
    for (int pass = 0; pass < 8; ++pass)
        for (int j = 0; j < 2; ++j) {
            const int i = (pass % 2) ? 1 - j : j;
            auto& w = webs_[i];
            if (!w.attached)
                continue;
            const Vec3 delta = body_.position - ropes[i].anchor;
            const float dist = length(delta);
            if (dist < w.length - nearlyTaut || dist < 1e-5f)
                continue;
            const Vec3 radial = delta / dist;
            held[i] = true;
            if (dist > w.length)
                move(ropes[i].anchor + radial * w.length, collision);
            // A taut rope that is being reeled carries the body inward at the
            // winch rate. With the pull applied to position only, the velocity
            // never held it, and letting go mid-reel stopped the body dead.
            // A hand that pulls on its rope does the same (physical webs). An
            // elastic web's end goes out as it stretches (give) and in as it
            // springs back.
            const float own = dot(body_.velocity, radial);
            const float outward = own + (hands_[i].winch - hands_[i].give) * follow(i, radial) +
                                  dot(hands_[i].rate, radial);
            // Once the other rope has pulled too, this one may have taken more
            // than the two need together. It hands that back, never more than
            // it took: kept, the surplus pushed the body sideways every step.
            const float change = std::max(outward, -pull[i]);
            const Vec3 before = body_.velocity;
            if (counting()) {
                const float e = gained(before, before - radial * change);
                if (hands_[i].give != 0) {
                    // A stretched rope being wound in: the winch's share is
                    // the rope's pull times the speed it winds in at; the
                    // rest is the stretch's, taken or given back.
                    const float wound = change * hands_[i].winch * follow(i, radial);
                    swing_.reel += wound;
                    swing_.elastic += e - wound;
                } else if (e > 0) {
                    swing_.reel += e;
                } else {
                    swing_.rope -= e;
                }
                // Going round the anchor turns the velocity out by this much
                // a step: the swing, not a snap; nor is an elastic web's give.
                const float turn = std::max(0.f, dot(before, before) - own * own) * dt / dist;
                if (own > turn && change > 0 && hands_[i].give == 0) {
                    cut[i] += std::min(own - turn, change);
                    cutEnergy[i] -= std::min(e, 0.f);
                }
            }
            pull[i] += change;
            body_.velocity -= radial * change;
        }
    // A winch spins the body up round its anchor (reelSpin): the speed it
    // drew the body in at turns along the swing as the body goes round, by
    // v w / rope a second, the most in a fast swing. Only that spin is
    // limited; the body still comes in at the winch's rate, and is lifted as
    // before. Measured as it happens (the last step's draw, turned), not
    // estimated: the estimate ran 5% over and slowed a capped swing.
    for (int i = 0; i < 2 && config_.reelSpin > 0; ++i) {
        const Vec3 drawn = hands_[i].drawn;
        hands_[i].drawn = {};
        const float give = std::max(0.f, hands_[i].give); // a stretching rope's end goes out
        if (!held[i] || !webs_[i].attached)
            continue;
        const Vec3 delta = body_.position - ropes[i].anchor;
        const float dist = length(delta);
        if (dist < 1e-3f)
            continue;
        const Vec3 radial = delta / dist;
        if (hands_[i].winch > give)
            hands_[i].drawn = radial * -((hands_[i].winch - give) * follow(i, radial));
        const Vec3 along = body_.velocity - radial * dot(body_.velocity, radial);
        const float speed = length(along);
        const float spun = speed < 1e-3f ? 0.f : dot(drawn, along / speed);
        if (spun <= 0)
            continue;
        // Nothing above the speed limit: the room under it, with the speed
        // across the swing (toward the anchor) as it is.
        const float across = std::max(0.f, dot(body_.velocity, body_.velocity) - speed * speed);
        const float room = std::sqrt(std::max(0.f, limit_ * limit_ - across)) - (speed - spun);
        const float allowed = std::clamp(std::min(config_.reelSpin * config_.gravity * dt, room), 0.f, spun);
        const Vec3 unlimited = body_.velocity;
        body_.velocity -= along * (std::min(spun - allowed, speed) / speed);
        if (counting())
            swing_.reel += gained(unlimited, body_.velocity); // the reel's, net of the limit
    }
    // What an elastic catch took from the rope's way out goes on along the
    // swing: along the body's way round the anchor, or down it from rest.
    for (int i = 0; i < 2; ++i) {
        if (sling[i] <= 0 || !webs_[i].attached)
            continue;
        const Vec3 delta = body_.position - hold(i);
        if (length(delta) < 1e-3f)
            continue;
        const Vec3 radial = normalized(delta);
        Vec3 along = body_.velocity - radial * dot(body_.velocity, radial);
        if (length(along) < .5f)
            along = Vec3{0, -1, 0} + radial * radial.y;
        if (length(along) < .05f)
            continue;
        const Vec3 way = normalized(along), before = body_.velocity;
        const float speed = dot(body_.velocity, way);
        body_.velocity += way * (std::sqrt(std::max(0.f, speed * speed + 2 * sling[i])) - speed);
        const Vec3 slung = body_.velocity;
        body_.velocity = limited(body_.velocity, limit_);
        if (counting()) {
            // What the speed limit takes of it is the limit's, not the sling's.
            swing_.sling += gained(before, slung);
            swing_.cap -= gained(slung, body_.velocity);
        }
    }
    for (int i = 0; i < 2; ++i) {
        hands_[i].taut = held[i];
        // A rigid rope that pulled taut and cut the body's speed at once: a
        // snap. A winch catching up with the body gives more than it cuts: none.
        if (counting() && cut[i] > config_.catchMin && cutEnergy[i] > 0) {
            ++swing_.snaps;
            swing_.snapLoss += cutEnergy[i];
        }
    }
    if (swinging_) {
        swing_.seconds += dt;
        swing_.peak = std::max(swing_.peak, length(body_.velocity));
    }
    // A web may pull along a wall or off it, never through it.
    if (wall_.on)
        holdWall(dt, wallBefore, collision);
    for (int i = 0; i < 2; ++i)
        webs_[i].tension = webs_[i].attached ? pull[i] / dt : 0;
    if (body_.grounded && !wasGrounded)
        sinceLanding_ = 0;
}
void Swing::jumpOff() {
    const Vec3 v = body_.velocity;
    const float speed = length(v);
    // The band by the velocity's pitch: low below jumpOffMid degrees, middle
    // to jumpOffHigh, high from there.
    const float pitch = speed > .5f ? std::asin(std::clamp(v.y / speed, -1.f, 1.f)) * 57.29578f : 0.f;
    const int band = pitch < config_.jumpOffMid ? 0 : pitch < config_.jumpOffHigh ? 1 : 2;
    const float floor = config_.jumpOffFloor[band];
    if (config_.jumpOffUp[band] <= 0 && floor <= 0 && config_.jumpOffBoost[band] <= 0)
        return; // the classic swing: a jump does nothing on a web
    // The gains only off a swing (jumpOffSwing), not off a web just shot.
    const bool swung = sinceAttach_ >= config_.jumpOffSwing;
    const float up = swung ? config_.jumpOffUp[band] : 0, boost = swung ? config_.jumpOffBoost[band] : 0;
    // In the game's m/s: v.y = max(v.y + JumpBoost, VertFloorJump), then as
    // fast as rises as high at the player's gravity as that does at the
    // game's gravity after a jump.
    const float scale = gameScale();
    const float rise = std::max(v.y / scale + up, floor) * scale *
                       std::sqrt(config_.gameFall / config_.jumpOffGravity[band]);
    // Along the level: JumpBoostXZ, ramped in from 10 to 30 degrees up and
    // out from 35 to 55. Straight up there is no way to go: no stick picks one.
    const float ramp = std::clamp((pitch - 10) / 20, 0.f, 1.f) * std::clamp((55 - pitch) / 20, 0.f, 1.f);
    const Vec3 level{v.x, 0, v.z};
    const Vec3 way = length(level) < 1 ? Vec3{} : normalized(level);
    Vec3 next = v + way * (boost * ramp * scale);
    next.y = std::max(v.y, rise); // a jump never slows the way up
    next = limited(next, limit_);
    if (swinging_) {
        swing_.jump += gained(v, next);
        ++swing_.jumpOff;
    }
    body_.velocity = next;
    gainMomentum(1, 5, &sinceJumpOff_, 1); // SwingReleaseData JumpData: 1, to 5, once a second
    for (int i = 0; i < 2; ++i)
        release(i);
    events_.push_back({EventKind::JumpOff, -1, std::clamp(band / 2.f, .4f, 1.f)});
}
bool Swing::hung(float* angle, float* rope) const {
    bool any{};
    float upright = 180, span = 0;
    for (int i = 0; i < 2; ++i) {
        const auto& w = webs_[i];
        if (!w.attached || w.anchor.y <= body_.position.y)
            continue;
        // To what the rope holds the body to (the hand's point with physical webs).
        const Vec3 d = body_.position - hold(i);
        const float dist = length(d);
        if (dist < 1e-5f || dist < w.length - .15f)
            continue;
        any = true;
        const float from = std::acos(std::clamp(-d.y / dist, -1.f, 1.f)) * 57.29578f;
        if (from < upright) {
            upright = from;
            span = dist;
        }
    }
    if (angle)
        *angle = any ? upright : 0;
    if (rope)
        *rope = span;
    return any;
}
float Swing::swingGravity(float rope) const {
    // The game's swing gravity (Hero::HeroStateSwing, 0xac0b60), in m/s^2
    // under its free fall of gameFall: only its shares carry over.
    const float game = config_.gameFall;
    const auto ramp = [](float x, float from, float to) {
        return std::clamp((x - from) / (to - from), 0.f, 1.f);
    };
    const auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    const Vec3 v = body_.velocity;
    // Swinging back from the top of the arc (mode 2): 14 falling, 22 rising.
    if (swingMode_ == 2)
        return (v.y < 0 ? 14 : 22) / game;
    // The way down: 24, half that for 0.25 s after a web attaches, full by 0.5 s.
    if (v.y < 0)
        return config_.swingFall * mix(.5f, 1, ramp(sinceAttach_, .25f, .5f));
    // The way up: RiseGravityLow (velocity pitch to 30 degrees) and High (from
    // 70), each between its value with no stick (LowMinZero 28 at 16 m/s to
    // LowMaxZero 40 at 40; HighMinZero 30 at 12 to HighMaxZero 45 at 30) and
    // with the stick forward (LowMin 2 to LowMax 4; HighMin 6 to HighMax 11),
    // by the intent. The game's speeds, here at the player's gravity.
    const float speed = length(v) / gameScale(), intent = std::clamp(config_.swingIntent, 0.f, 1.f);
    const float pitch = std::asin(std::clamp(v.y / std::max(length(v), 1e-3f), -1.f, 1.f)) * 57.29578f;
    const float low = mix(mix(28, 40, ramp(speed, 16, 40)), mix(2, 4, ramp(speed, 16, 40)), intent);
    const float high = mix(mix(30, 45, ramp(speed, 12, 30)), mix(6, 11, ramp(speed, 12, 30)), intent);
    float rise = mix(low, high, ramp(pitch, 30, 70));
    // A short rope rises heavy (35 at 8 m and under); a long one lighter
    // (RiseLineLengthScale 0.5 from 55 m).
    if (rope < 16)
        rise = mix(35, rise, ramp(rope, 8, 16));
    else
        rise *= mix(1, .5f, ramp(rope, 28, 55));
    // High momentum lightens the way up past the bottom of the arc only
    // (mode 1; its stage's GravityScale).
    if (swingMode_ == 1 && config_.momentum && momentum_ >= config_.momentumHigh)
        rise *= config_.momentumGravity;
    return config_.swingRise * rise / game;
}
float Swing::gameScale() const {
    return std::sqrt(std::max(config_.gravity, .1f) / config_.gameFall);
}
void Swing::boostSwing(float dt) {
    const float scale = gameScale(), target = config_.boostTarget * scale;
    const Vec3 v = body_.velocity;
    // The game measures a rising body's speed along the level.
    const float speed = v.y > 0 ? length(Vec3{v.x, 0, v.z}) : length(v), total = length(v);
    // Only from the web's attaching to the bottom of its arc (the game's mode
    // 0), and only below the target: the boost lifts a slow start. The game
    // has no height rule (its SpeedBoostAltMin is in no type it loads).
    const bool on =
        target > 0 && !body_.grounded && swingMode_ == 0 && total > .5f && speed < target && hung();
    if (!on) {
        boostRamp_ = 0;
        return;
    }
    const float k = scale * scale, most = config_.boostAccel * k;
    boostRamp_ = boostRamp_ > 0 ? std::min(most, boostRamp_ + config_.boostJerk * k * scale * dt)
                                : std::min(most, config_.boostStart * k);
    body_.velocity = v + v * (std::min(target - speed, boostRamp_ * dt) / total);
    if (counting())
        swing_.boost += gained(v, body_.velocity);
}
void Swing::gainMomentum(float amount, float cap, float* since, float retrigger) {
    if (!config_.momentum || amount <= 0 || momentum_ >= cap || (since && *since < retrigger))
        return;
    const bool below = momentum_ < config_.momentumHigh;
    momentum_ = std::min(cap, momentum_ + amount);
    sinceMomentum_ = 0;
    if (since)
        *since = 0;
    if (below && momentum_ >= config_.momentumHigh)
        ++surges_;
}
void Swing::decayMomentum(float dt) {
    sinceMomentum_ += dt;
    if (!config_.momentum) {
        momentum_ = 0;
        return;
    }
    // DecayDataGround's wait, DecayDataStop's rate; in the air DecayDataAir:
    // with the meter at 0, 1 s then 0.25 a second; at 3.5, 0.75 s then 0.66;
    // at 5, 0.5 s then 1. All on the meter's slower clock (momentumTime).
    float delay = 2, rate = 2;
    if (!body_.grounded) {
        const float m = momentum_;
        const float t = m < 3.5f ? m / 3.5f : (m - 3.5f) / 1.5f;
        delay = m < 3.5f ? 1 - .25f * t : .75f - .25f * t;
        rate = m < 3.5f ? .25f + .41f * t : .66f + .34f * t;
    }
    if (sinceMomentum_ > delay * config_.momentumTime)
        momentum_ = std::max(0.f, momentum_ - rate * dt / config_.momentumTime);
}
void Swing::update(float seconds, const Input& input, const World& world) {
    events_.clear();
    if (!std::isfinite(seconds) || seconds <= 0)
        return;
    if (!input.focused || seconds > .1f) {
        releaseAll();
        jumpHeld_ = input.jump;
        return;
    }
    inputs(seconds, input, world);
    senseWall(seconds, input, world);
    accumulator_ += seconds;
    int steps = 0;
    while (accumulator_ + 1e-9 >= config_.fixedStep && steps++ < 13) {
        step(config_.fixedStep, input, world, &world);
        accumulator_ -= config_.fixedStep;
    }
}
void Swing::settleStep(float predictedSeconds, float actualSeconds) {
    float extra = actualSeconds - predictedSeconds;
    if (!std::isfinite(extra) || std::abs(extra) > .1f)
        return;
    // A step longer than the longest was predicted as that one: the body
    // went that share of the way, and the rope is wound for as much.
    if (predictedSeconds > longestNativeStep)
        extra *= longestNativeStep / predictedSeconds;
    // Only what the winch really took in: a rope held at its shortest length
    // did not shorten, and must not start breathing with the frame time.
    const auto lengths = reeled({hands_[0].winch, hands_[1].winch}, extra);
    for (int i = 0; i < 2; ++i)
        if (webs_[i].attached)
            webs_[i].length = lengths[i];
}
MotionIntent Swing::predictNativeStep(float seconds, const Input& input, const WorldQueries& world,
                                      Body actual) {
    return predictNativeStep(seconds, input, world, actual, seconds);
}
MotionIntent Swing::predictNativeStep(float seconds, const Input& input, const WorldQueries& world,
                                      Body actual, float inputSeconds) {
    events_.clear();
    accumulator_ = 0;
    if (!std::isfinite(seconds) || seconds <= 0 || !input.focused ||
        !std::isfinite(inputSeconds) || inputSeconds < 0 || !finite(actual.position) ||
        !finite(actual.velocity) || length(actual.velocity) > limit_ * 2) {
        releaseAll();
        return {};
    }
    // A long step is a slow game, not stale input. Over 50 ms used to be
    // refused here: the caller let go of a swinging player, whom the game
    // then dropped at its own fall speed (October 10 headset report).
    seconds = std::min(seconds, longestNativeStep);
    if (actual.grounded && !body_.grounded)
        sinceLanding_ = 0;
    body_ = actual;
    // A winch takes in only rope the body has come in on (reelFollows): one
    // the ground held away from its anchor waits for it, a frame's winding
    // short of where it is.
    if (config_.reelFollows)
        for (int i = 0; i < 2; ++i)
            if (webs_[i].attached && hands_[i].reeling)
                webs_[i].length = std::max(webs_[i].length, length(body_.position - hold(i)) -
                                                                config_.reelSpeed * seconds);
    // The steps below hold the body to the speed limit. A web attached in
    // inputs() first sees the fall it catches at its full speed (fallCarry).
    stepSeconds_ = seconds;
    // A hand moves its rope once per sample of it.
    if (!(inputSeconds > 0))
        for (auto& hand : hands_)
            hand.rate = {};
    if (inputSeconds > 0) {
        // The first sample after a gap in input (a long frame, a moment
        // without focus) starts each hand's motion afresh: travel across the
        // gap is no yank. Releasing the webs there dropped the player.
        if (inputSeconds > .1f)
            for (auto& hand : hands_)
                hand.sample = false;
        inputs(inputSeconds, input, world);
    }
    senseWall(seconds, input, world);
    const auto count = static_cast<unsigned>(std::ceil(seconds / config_.fixedStep));
    const float dt = seconds / count;
    for (unsigned i = 0; i < count; ++i)
        step(dt, input, world, nullptr);
    MotionIntent result{body_.position, body_.velocity, true};
    // Native collision owns position and ground contact. Do not publish the
    // prediction as if the game had already accepted it.
    body_.position = actual.position;
    body_.grounded = actual.grounded;
    return result;
}
} // namespace spidy
