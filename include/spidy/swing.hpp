#pragma once
#include "math.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace spidy {
struct RayHit {
    Vec3 point{}, normal{};
    std::uint64_t surface{};
    bool fixed{true};
};
struct MoveResult {
    Vec3 position{}, normal{};
    bool hit{};
};
class WorldQueries {
  public:
    virtual ~WorldQueries() = default;
    virtual std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float distance) const = 0;
    virtual bool exists(std::uint64_t surface) const = 0;
};
class World : public WorldQueries {
  public:
    // Swept body center, sphere radius. Production adapter must use game collision.
    virtual MoveResult sweep(Vec3 from, Vec3 to, float radius) const = 0;
};
struct SwingConfig {
    float gravity = 9.81f, radius = .35f, maxRange = 100, minRope = 1.5f;
    float reelSpeed = 8, maxSpeed = 65, airAcceleration = 2, groundAcceleration = 24;
    float yankSpeed = 1.3f, yankDistance = .14f, yankWindow = .25f, zipMultiplier = 3.5f, maxZipImpulse = 12;
    float jumpSpeed = 6, pointLaunchWindow = .25f, zipLandingWindow = 1.2f;
    // A charged leap from the ground (Input::leap): up at leapUp for the
    // least charge and leapUpFull for a full one (4 and 11.5 m high at real
    // gravity), and along the stick at up to leapForward.
    float leapUp = 9, leapUpFull = 15, leapForward = 12;
    float fixedStep = 1.0f / 120.0f;
    // A held web lets go only when something stays between the body and the
    // anchor this long. Hits this close to the anchor (or a tenth of the rope,
    // if longer) are the anchor's own facade, ledges and sills, not a wall.
    float obstructionTime = .15f, anchorClearance = 1.5f;
    bool airAnchors = true; // A clear ray attaches at maximum reach, including open sky.
    // Walls: a body in the air that comes into a wall stays on it (WallHold).
    // The game held such a body still for a second and then stuck it to the
    // wall in its crawl, which only a jump left (October 9 headset reports).
    // On a wall the body keeps wallClearance between its centre and the wall,
    // so the game's own collision never meets it, and stays on it within
    // wallReach. A wall: fixed, its normal within wallSlope of level.
    bool walls = false;
    float wallClearance = .9f, wallReach = 1.5f, wallSlope = .5f;
    // Follow shallow facade steps and clear a sill before the native capsule
    // reaches it. Deeper setbacks still end the wall.
    float wallStep = .75f;
    // The speed along the wall goes on (a wall run), raised by up to
    // wallCarry of itself for the speed that went into the wall; gravity does
    // not pull along a wall. The stick walks the
    // wall at wallWalkSpeed (what of it points into the wall goes up it) and
    // steers a faster run; without it or a web a run slows by wallBrake a
    // second, and below the walk's speed the body stops and stays.
    float wallCarry = .35f, wallWalkSpeed = 6, wallAcceleration = 24, wallBrake = 3;
    // A jump leaves the wall: out from it and up, on top of the speed along
    // it; no wall takes the body again within wallJumpPause.
    float wallJumpOut = 6, wallJumpUp = 5, wallJumpPause = .3f;
    // A wall the ray misses for wallGrace is gone (a recess, the wall's end).
    // Lost while the body went up it, the body hops over its top edge and is
    // pushed on over the roof for crestSeconds.
    float wallGrace = .15f, crestSeconds = .4f, crestPush = 12, crestHop = 4.5f;
    // A body on the ground cannot come into a wall: the game walks it, and
    // put one that walked into a wall on it in its own crawl (all five crawls
    // of the October 10 headset report began so). Walked at a wall within
    // mountReach for mountSeconds, such a body wants up onto it (mounting()):
    // the caller has the game jump, and the wall takes the body as it leaves
    // the ground, at a walk at most, and again within mountHold: as its jump
    // begins the game reports the body standing once more for a step (in the
    // game, October 10). A wall, not a kerb or a car: it is there mountHeight
    // above the body too. In the air the stick takes a wall within mountReach
    // the same way, with no speed into it.
    float mountReach = 1.3f, mountSeconds = .1f, mountHold = .5f, mountHeight = 1.2f;
    // Physical webs: a web is a rope (the VR settings' PHYSICAL WEBS).
    // - It bends round the edge that comes between the body and its end
    //   instead of letting go there, round maxBends edges at most, and lies
    //   straight again when the body swings back. Until October 12 a wall in
    //   the way for obstructionTime cut the web off. A bend sits bendOut out
    //   of its edge, and a hit within bendClearance of it is that edge again.
    //   The rope beyond a bend is at least bendRope long.
    // - Its length is measured to the hand that holds it, not to the body: a
    //   taut web keeps its length whatever the arm does, and a hand pulled
    //   back pulls the body after it, as fast as it moved (maxHandSpeed at
    //   most). The hand is where the grip is from the head, handHeight above
    //   the body's centre.
    // - payOut(): it grows while the game walks the body away from it.
    //   Until October 12 it kept the length it was shot with, and snapped a
    //   body that had walked on back by all of that once it left the ground.
    bool physical = false;
    float bendOut = .06f, bendClearance = .35f, bendRope = .5f, handHeight = .6f, maxHandSpeed = 8;
    // A dive (Input::dive) in free flight: the game's own free fall
    // (herotraversalconfig: StandardFallGravity 26 m/s^2, terminal velocity
    // 42 m/s, rising at StandardFallTermVelAccel 8 m/s per second toward
    // StandardFallTerminalVelocityMax, 100 in the game and the mover's limit here).
    float diveGravity = 26, diveSpeed = 42, diveSpeedRamp = 8, diveSpeedMax = 65;
    // Speed over maxSpeed (a dive's, or what a web carries out of one) bleeds
    // off at this rate once nothing holds it up.
    float overspeedDecay = 4;
    // Swing model 2.0 (SwingModel; these defaults are the classic swing of
    // 0.2.11, which has none of it):
    // The elastic web. A rope pulled taut while the body moves away from its
    // anchor faster than catchMin (a web catching a fall, a slack rope after
    // a yank) gives like an elastic band instead of cutting that speed in one
    // step: it stretches, a spring of catchFrequency (Hz; a quarter period,
    // 0.25 s at 1 Hz, to the deepest stretch: the game's SlingDataAir
    // Duration), and springs back to its length, flinging the body toward
    // the anchor at catchBounce of the speed it caught. Of the rest, catchKeep
    // goes on along the swing (the game's sling: the fall turns into the
    // swing). catchFrequency 0: a rigid web, which cuts that speed at once.
    float catchMin = 2, catchFrequency = 0, catchBounce = .4f, catchKeep = 1;
    // A faster catch stiffens the web so it stretches catchDepth m at most
    // (the October 11 headset report: 6-7 m at 65 m/s, a bungee). 0: no limit.
    float catchDepth = 0;
    // The winch takes in only rope the body has come in on. Without it a
    // winch held on the ground (where the game holds the body) wound 9 m of
    // rope in that the body never followed, and the take-off pulled it all
    // in at once at the speed limit (October 11 headset report).
    bool reelFollows = false;
    // The swing's pump (hero_swingsetup MotionParams GravityParams: 24 m/s^2
    // on the way down, 2-14 on the way up with the stick held). While a taut
    // web holds the body up, gravity is swingFall times as strong as it falls
    // and swingRise times as strong as it rises, so a swing leaves faster
    // than it came in. 1 and 1: a swing never leaves faster than it came.
    float swingFall = 1, swingRise = 1;
    // A jump with a web attached in the air leaps off the swing
    // (hero_swingconfig ReleaseParams by the velocity's pitch): every web lets
    // go, the body rises at least jumpOffUp m/s and goes jumpOffBoost m/s
    // faster along its way, each from the low band (level or falling) through
    // the middle one (rising at jumpOffMid degrees) to the high one
    // (jumpOffHigh degrees and steeper). All 0: the jump does nothing there.
    float jumpOffUp[3]{}, jumpOffBoost[3]{};
    float jumpOffMid = 25, jumpOffHigh = 50;
    // 2.0's numbers from the game are the game's own, m/s and m/s^2 under its
    // free fall of gameFall (herotraversalconfig StandardFallGravity 26
    // m/s^2), and the solver scales them to the player's gravity so that they
    // lift the body as high: speeds by sqrt(gravity / gameFall), 0.61 at real
    // gravity, accelerations by gravity / gameFall.
    float gameFall = 26;
    // The swing's speed boost (hero_swingsetup_standard SpeedParams, read in
    // the game's code, Oct 13 2026: SpeedBoostTarget 10 m/s, an absolute
    // speed; SpeedBoostAccel 20 m/s^2, starting at 8 and rising by
    // SpeedBoostAccelAccel 120 m/s^3). A slow swing speeds up along its way
    // from a web's attaching to the bottom of its arc, while it goes slower
    // than boostTarget (a rising body: along the level). It lifts a slow start;
    // at swinging speeds it does nothing. boostTarget 0: no boost.
    float boostTarget = 0, boostStart = 8, boostAccel = 20, boostJerk = 120;
    // The momentum meter (hero_swingconfig MomentumConfig; Swing::momentum):
    // 0 to 5, raised by swinging, letting go on the way up, jumping off,
    // yanks and dives, falling away in the air within a second of the last
    // rise and on the ground after 2 s. From momentumHigh (its stage 2 at 4)
    // the swing's speed limit is momentumLimit times maxSpeed (its
    // TerminalVelocityHorzFloor 28 over the 24 of a swing with none) and a
    // taut web's gravity momentumGravity times (its GravityScale). Its clock
    // runs momentumTime times slower than the game's: real gravity's swings
    // take about 1.5 times as long as the game's (24 m/s^2), and the meter
    // fell away between them (spidy_swing_bench: 2.5 after nine jumps off in
    // 20 s). false: no meter.
    bool momentum = false;
    float momentumHigh = 4, momentumLimit = 28.f / 24, momentumGravity = .5f, momentumTime = 1;
};
// What one swing did to the body's speed (Swing::lastSwing): from a web
// attaching while none was to the last one letting go. Energies per unit
// mass, m^2/s^2; over gravity, metres of height. With gravity's work
// (gravity x drop) they add up to exit^2/2 - entry^2/2, but for what the game
// changed between predictions.
struct SwingLedger {
    uint32_t swings{}; // swings finished so far, this one included
    uint32_t snaps{};  // steps in which a rope cut more than catchMin of the body's speed at once
    uint32_t catches{}, yanks{}, jumpOff{};
    float seconds{}, entry{}, exit{}, peak{};
    float fall{}; // speed down as the first web attached, m/s
    float drop{}; // height lost from the first attachment to the release, m
    // rope: what rigid ropes cut from the body (snapLoss: in snaps); reel:
    // what winding a rope in gave; elastic: what stretching ropes took and
    // their springing back gave (net); sling: what they kept along the
    // swing; yank: the yanks; pump: the swing's own gravity, over the
    // player's; steer: the stick; cap: what the speed limit cut; jump: the
    // jump off the swing; boost: the swing's speed boost. Steps on the
    // ground are not counted (the game holds the body there).
    float rope{}, snapLoss{}, reel{}, elastic{}, sling{}, yank{}, pump{}, steer{}, cap{}, jump{};
    float deepest{};  // the most a rope stretched, m
    float boost{};
    float momentum{}; // the momentum meter as the swing ended
};
// The edges one web bends round at most (SwingConfig::physical).
constexpr int maxBends = 6;

// The swing models (the launcher's SWING MODEL). classic: 0.2.11's swing,
// SwingConfig's defaults. v2: the elastic web (3 m deep at most), the swing's
// pump, the jump off the swing, the speed boost and the momentum meter, at
// about the game's timings and half its jump (its VertFloorJump 12/22/30 and
// JumpBoost 8/20/24 m/s), for a player whose gravity is real gravity rather
// than the game's 26 m/s^2; and a winch that waits for the body.
enum class SwingModel : uint32_t { classic, v2 };
inline void applySwingModel(SwingConfig& c, SwingModel model) {
    const SwingConfig classic;
    const bool v2 = model == SwingModel::v2;
    c.catchFrequency = v2 ? 1.f : classic.catchFrequency;
    c.catchBounce = v2 ? .4f : classic.catchBounce;
    c.catchKeep = v2 ? 1.f : classic.catchKeep;
    c.catchDepth = v2 ? 3.f : classic.catchDepth;
    c.reelFollows = v2 || classic.reelFollows;
    c.boostTarget = v2 ? 10.f : classic.boostTarget;
    c.momentum = v2 || classic.momentum;
    c.momentumTime = v2 ? 1.5f : classic.momentumTime;
    c.swingFall = v2 ? 1.25f : classic.swingFall;
    c.swingRise = v2 ? .6f : classic.swingRise;
    constexpr float up[3]{6, 10, 13}, boost[3]{4, 9, 11};
    for (int i = 0; i < 3; ++i) {
        c.jumpOffUp[i] = v2 ? up[i] : classic.jumpOffUp[i];
        c.jumpOffBoost[i] = v2 ? boost[i] : classic.jumpOffBoost[i];
    }
}
// The game's swing carries a fall into the swing that catches it: its speed
// limit for the swing rises with the fall speed when the web attaches
// (hero_swingsetup_standard.config SpeedParams: falling at 6, 16 and 42 m/s,
// the horizontal terminal velocity is 24, 27.25 and 40 m/s). The limit a
// swing gets, as a multiple of the normal one: 1 up to 6 m/s, 5/3 from 42.
inline float fallCarry(float fallSpeed) {
    if (!std::isfinite(fallSpeed) || fallSpeed <= 6)
        return 1;
    const float terminal = fallSpeed < 16   ? 24 + (fallSpeed - 6) * (27.25f - 24) / (16 - 6)
                           : fallSpeed < 42 ? 27.25f + (fallSpeed - 16) * (40 - 27.25f) / (42 - 16)
                                            : 40;
    return terminal / 24;
}
// The web shooter's nozzle, as Spider-Man's: on the palm side of the wrist,
// at the heel of the hand (a right hand's grip frame: +y back toward the
// forearm, +x toward the back of the hand; a left hand mirrors x). Webs leave
// here, along the hand's line (handLine) or the controller's aim; the aim
// pose itself sits at the knuckles.
constexpr Vec3 webShooterFromGrip{-.02f, .075f, 0};
inline Vec3 webShooterPoint(Pose grip, unsigned hand) {
    const Vec3 o = webShooterFromGrip;
    return grip.position + grip.orientation.rotate({hand == 0 ? -o.x : o.x, o.y, o.z});
}
// The hand's line, as Spider-Man's thwip: from the wrist straight through the
// fist past the middle fingers' knuckles, the grip frame's -y (OpenXR's grip:
// -z runs up the handle toward the thumb, +-x out of the palm), with the
// handle's top as up. A natural, straight hand aims it; the controller's
// pointer leaves its top by the thumb instead (Oct 11 2026: aims turned from
// the pointer toward the palm by 15-90 degrees all missed the hand's line).
inline Quat handLine(Quat grip) {
    return grip * Quat::around({1, 0, 0}, -1.5707964f);
}
struct HandInput {
    // World space, -Z forward: the hand's line (handLine) or the controller's
    // aim, from the web shooter (webShooterPoint; trackedSwingInput).
    Pose aim{};
    Vec3 gripRelativeToHead{}; // tracking meters, excludes artificial travel
    bool tracked{};
    // grip shoots and holds this hand's web; trigger reels it in (a free
    // hand's shoots a web ball). The controller's own grip and trigger unless
    // the player swapped them (trackedSwingInput).
    float trigger{}, grip{};
    // World space: the knuckles (the controller's aim point), where punches
    // land; the aim's point when it is not known.
    Vec3 fist{};
};
inline Vec3 fistPoint(const HandInput& h) {
    return finite(h.fist) && (h.fist.x != 0 || h.fist.y != 0 || h.fist.z != 0) ? h.fist : h.aim.position;
}
struct Input {
    std::array<HandInput, 2> hands{};
    Vec3 move{}; // world horizontal direction, magnitude <= 1
    float trackingYaw{};
    // dive: a dive is wanted (the XR side's toggle); it acts only in free
    // flight, with no web attached.
    bool focused = true, jump{}, dive{};
    // A flip's tilt of the tracking space (Rig::tilt), the identity while level.
    Quat tilt{};
    // A charged leap let go of: its charge, above 0 up to 1, for as long as
    // the caller holds it out (it counts once, as it appears). The swing's
    // own Charge Jump: the game's is a skill a save may lack.
    float leap{};
    // Where the head looks (world, unit; zero: unknown): the enemy a punch
    // would go to is the one looked at (game_punch).
    Vec3 look{};
};
// The tracking space's orientation in the world (Rig::orientation): hand
// motion relative to the head turns into the world with it.
inline Quat trackingTurn(const Input& in) {
    return Quat::yaw(in.trackingYaw) * in.tilt;
}
struct Body {
    Vec3 position{0, 3, 0}, velocity{};
    bool grounded{};
};
struct MotionIntent {
    Vec3 target{}, velocity{};
    bool valid{};
};
// The longest step predictNativeStep predicts. A longer one counts as this
// long: the body goes less far in it, and keeps its webs and its flight.
constexpr float longestNativeStep = .1f;
// An edge a web bends round (SwingConfig::physical).
struct Bend {
    Vec3 from{}; // what the rope hung from before: the web's end, or the bend before
    Vec3 out{};  // out of the edge, unit: the rope lies on this side of it
    float span{}; // the rope between `from` and this bend
};
struct Web {
    bool attached{};
    // What the body hangs from: the web's end, or the nearest of its bends.
    Vec3 anchor{};
    std::uint64_t surface{};
    // length: the rope from `anchor` on (to the body, or with physical webs
    // to the hand).
    float length{}, tension{};
    bool airAnchor{};
    // The edges it bends round, from its end on; `anchor` is the last one.
    std::array<Bend, maxBends> bends{};
    int bent{};
    // Where the web sticks.
    Vec3 end() const {
        return bent ? bends[0].from : anchor;
    }
};
// What a web shot along an aim would do (Swing::shot): the web it would
// attach, or nothing when it would miss; and the first surface its ray met,
// held by a web or not (nothing: the ray was clear to maximum reach).
struct WebShot {
    std::optional<Web> web;
    std::optional<RayHit> hit;
};
// The wall the body is on (SwingConfig's walls).
struct WallHold {
    bool on{};
    Vec3 normal{};    // out of the wall, unit
    Vec3 point{};     // on the wall beside the body
    float distance{}; // the body's centre from the wall
    float seconds{};  // on walls since it came onto one
};
enum class EventKind {
    Attach,
    Miss,
    Release,
    Zip,
    PointLaunch,
    TrackingLost,
    Obstructed,
    WallOn,   // the body came onto a wall; strength: how hard it came into it
    WallOff,  // it left the wall, or the wall ended
    WallJump, // it jumped off the wall
    Wrap,     // a web bent round an edge
    Unwrap,   // it came off its last one
    Leap,     // a charged leap left the ground; strength: its charge
    Catch,    // an elastic web caught the body; strength: how fast it was going out
    JumpOff,  // a jump left the swing (its webs' releases come first)
};
struct Event {
    EventKind kind;
    int hand;
    float strength;
};
class Swing {
  public:
    explicit Swing(SwingConfig config = {});
    void update(float seconds, const Input& input, const World& world);
    // Predict a request for an external collision engine. The caller must submit
    // it through that engine and supply its actual result on the next call.
    // body().position always remains the supplied authoritative position.
    MotionIntent predictNativeStep(float seconds, const Input&, const WorldQueries&, Body actual);
    // Input and physics clocks may differ. Zero inputSeconds reuses button/
    // steering state without treating a repeated pose as a new hand sample.
    MotionIntent predictNativeStep(float seconds, const Input&, const WorldQueries&, Body actual,
                                   float inputSeconds);
    // A prediction covers a step whose length is not known yet. Once the
    // external engine reports how long that step lasts, call this before the
    // next prediction, so a winch has reeled for the real time. Frame times
    // vary by milliseconds; a rope that shortened for a different time than
    // the body travelled snapped the body by centimetres, a 1 m/s jolt.
    void settleStep(float predictedSeconds, float actualSeconds);
    void reset(Body body = {});
    void releaseAll();
    // A new speed limit during play (the VR settings). A body
    // faster than it slows to it in the next step.
    void limitSpeed(float maxSpeed);
    // A new gravity during play (the VR settings' weight), m/s^2, from the
    // next step.
    void setGravity(float gravity);
    // The swing model (applySwingModel) from the next step. Ropes stretched
    // by the elastic web keep their length.
    void setModel(SwingModel);
    // Whether a clear shot attaches in the air at maximum reach from now on
    // (the VR settings); off, it misses. Webs already attached
    // keep their anchors.
    void allowAirAnchors(bool allowed);
    // Whether a body in the air stays on the walls it comes into from now on
    // (SwingConfig's walls); off, it leaves the one it is on.
    void allowWalls(bool allowed);
    // Whether webs are ropes from now on (SwingConfig::physical). Off, a web
    // that bends round an edge hangs from that edge as if it had stuck there.
    void allowPhysical(bool allowed);
    // The body, at `position`, stands or moves some other way than this
    // swing moves it (the game walks it): each physical web is long enough
    // to reach it with `slack` to spare, so an arm that moves does not lift a
    // standing body off the ground; a winch, a zip or a step off an edge
    // does, as before. Before the prediction of such a step.
    void payOut(Vec3 position, float slack);
    // Off the wall at once, with no jump (the caller lost track of the body).
    // releaseAll() keeps the wall: the body stays on it without input.
    void leaveWall();
    const WallHold& wall() const {
        return wall_;
    }
    // After a prediction: the body stands and wants up onto a wall, because
    // the stick walks it at one or the caller offered one (SwingConfig's
    // mountReach). The caller makes the game's jump while this holds.
    bool mounting() const {
        return mounting_;
    }
    // The same from the first moment the stick walks the standing body at a
    // wall. The caller keeps the stick from the game meanwhile: the game's
    // crawl takes a body 0.1 s after it meets a wall it is walked into (in
    // the game, October 10), as soon as the mount's own wait.
    bool mountBegun() const {
        return mounting_ || mount_ > 0;
    }
    // Before a prediction: a wall the game holds the body on in its own way
    // (its wall crawl), which the swing takes as soon as the body is off it.
    // A body that does not stand is taken at once, and arrives at rest: the
    // jump that took it off was not the player's.
    void offerWall(const RayHit& wall);
    // A web shot now along `aim` (world space, -Z forward) with the body at
    // `from`, without shooting it: the same test a grip press makes.
    WebShot shot(Pose aim, Vec3 from, const WorldQueries&) const;
    bool pointLaunchReady() const {
        return body_.grounded && sinceLanding_ <= config_.pointLaunchWindow &&
               sinceZip_ <= config_.zipLandingWindow;
    }
    const Body& body() const {
        return body_;
    }
    const std::array<Web, 2>& webs() const {
        return webs_;
    }
    const SwingConfig& config() const {
        return config_;
    }
    // The speed limit now: maxSpeed, or above it while a dive's speed lasts.
    float speedLimit() const {
        return limit_;
    }
    bool diving() const {
        return diving_;
    }
    const std::vector<Event>& events() const {
        return events_;
    }
    // The last swing that ended (SwingLedger), and the one going on now.
    const SwingLedger& lastSwing() const {
        return lastSwing_;
    }
    const SwingLedger& swing() const {
        return swing_;
    }
    // How far each hand's rope is stretched beyond its length now (the
    // elastic web), m.
    float stretch(int hand) const {
        return hands_[hand].stretch;
    }
    // The momentum meter (SwingConfig's momentum), 0 to 5, and how many times
    // it has reached momentumHigh since the reset.
    float momentum() const {
        return momentum_;
    }
    uint32_t surges() const {
        return surges_;
    }

  private:
    struct HandState {
        // held: grip squeezed, with release hysteresis. A web shoots only on the
        // press that sets it, so a lost web never re-fires while the grip stays held.
        bool held{}, sample{}, zipUsed{}, triggerReleased{}, reeling{};
        Vec3 previous{};
        float pullDistance{}, pullTime{}, obstructed{};
        float winch{}; // how fast this hand's rope shortened in the last step
        // Physical webs: the hand from the body's centre, in the world, and
        // how fast it moved there in the latest input sample.
        Vec3 offset{}, rate{};
        // The elastic web: how far the rope is stretched beyond its length
        // (included in Web::length), how fast that grows, and how fast it
        // grew in the last step.
        float stretch{}, stretchSpeed{}, give{};
        float omega{}; // the stretching rope's spring, rad/s (catchDepth stiffens it)
        bool taut{};   // the rope held the body at the end of the last step
    };
    void inputs(float dt, const Input&, const WorldQueries&);
    void step(float dt, const Input&, const WorldQueries&, const World* collision);
    void move(Vec3 target, const World* collision);
    // Walls, once per update: follows the wall the body is on (round an
    // inside corner, and at a walk round an outside one), or finds the wall
    // it is coming into.
    void senseWall(float dt, const Input&, const WorldQueries&);
    void joinWall(const RayHit&);
    // The body's velocity on its wall for one step: the stick's, the brake.
    void wallMotion(float dt, const Input&);
    // Keeps the body at the wall's clearance; `before`: its distance from
    // the wall as the step began.
    void holdWall(float dt, float before, const World* collision);
    void release(int hand, EventKind reason = EventKind::Release);
    // Whether a hit on the line from `from` to `anchor` is a real wall between
    // them; `bend`: the anchor is an edge the web bends round.
    bool blocks(const RayHit& hit, Vec3 from, Vec3 anchor, bool bend = false) const;
    // What hand `i`'s rope holds the body's centre to: its web's anchor, less
    // the hand's offset with physical webs.
    Vec3 hold(int i) const;
    // The line from the body to web `i`'s anchor met `first`: bends the web
    // round the edge between there and `before`, where the body was a step
    // ago with the line clear. False when no such edge is found.
    bool wrap(int i, Vec3 before, const RayHit& first, const WorldQueries&);
    // Takes web `i` off its last bend once the rope lies free of that edge.
    void unwrap(int i, const WorldQueries&);
    // The elastic web for one step (SwingConfig's catch*): starts a catch on
    // a rope pulled taut, or stretches and springs back one going on. What
    // each rope's stretch kept to go on along the swing, per unit mass.
    std::array<float, 2> stretchRopes(float dt);
    // A jump with a web attached in the air (SwingConfig's jumpOff*).
    void jumpOff(const Input&);
    // A taut web holding the body up, and the rope's angle from straight
    // down at the most upright such web (degrees; 0 for none).
    bool hung(float* angle = nullptr) const;
    // The swing's speed boost for one step (SwingConfig's boost*).
    void boostSwing(float dt);
    // 2.0's game speeds at this player's gravity (SwingConfig::gameFall):
    // speeds scale by this, accelerations by its square.
    float gameScale() const;
    // The momentum meter: a rise of `amount` up to `cap` (none once the meter
    // is there); `retrigger`, the seconds this source waits after its last
    // rise. And its fall for one step.
    void gainMomentum(float amount, float cap, float* since = nullptr, float retrigger = 0);
    void decayMomentum(float dt);
    // Whether this step counts in the swing ledger: in a swing, off the ground.
    bool counting() const {
        return swinging_ && !body_.grounded;
    }
    // The swing ledger: a swing begins as a web attaches while none is, and
    // ends as the last lets go.
    void beginSwing();
    void endSwing();
    // Energy per unit mass a velocity change gave the body (negative: took).
    static float gained(Vec3 before, Vec3 after) {
        return .5f * (dot(after, after) - dot(before, before));
    }
    // Rope lengths after winding each hand's rope in at its rate for a time,
    // which may be negative, within what the ropes and their anchors allow.
    std::array<float, 2> reeled(std::array<float, 2> rates, float seconds) const;
    // The share of this hand's winch rate the body's velocity takes up, given
    // the unit direction from its anchor to the body.
    float follow(int hand, Vec3 radial) const;
    SwingConfig config_;
    Body body_{};
    std::array<Web, 2> webs_{};
    std::array<HandState, 2> hands_{};
    std::vector<Event> events_;
    double accumulator_{};
    // Raises or bleeds limit_ toward what this step allows (see overspeedDecay).
    void steerLimit(float dt, const Input&);
    float sinceZip_ = 100, sinceLanding_ = 100, zipSpeed_{};
    Vec3 zipDirection_{};
    bool jumpHeld_{}, jumpQueued_{};
    // A leap's charge waiting for the next step, and whether one is held out.
    float leapQueued_{};
    bool leapHeld_{};
    WallHold wall_{};
    // Time the wall's ray has missed, time since a wall jump, and what is
    // left of the push over a wall's top edge, against crestNormal_.
    float wallLost_{}, sinceWallJump_ = 100, crest_{};
    Vec3 crestNormal_{};
    // Rounding a corner onto the wall's next face: the way along that face,
    // and the time left to come in front of it.
    Vec3 wrapWay_{};
    float wrapLeft_{};
    // Mounting from the ground: how long the stick has walked at a wall, the
    // wall that takes the body once it is in the air and for how long yet,
    // and whether the caller offered that wall (now, and the one kept).
    RayHit mountWall_{};
    float mount_{}, mountLeft_{};
    bool mounting_{}, offered_{}, mountOffered_{};
    // The step the input sample in hand is for (a hand's rate is no faster
    // than its travel over that step).
    float stepSeconds_{};
    // limit_: the speed limit now. carry_: what the attached webs' swing
    // allows (fallCarry at the attachment). diveTime_: seconds in this dive.
    float limit_{}, carry_{}, diveTime_{};
    bool diving_{};
    SwingLedger swing_{}, lastSwing_{};
    float swingStartY_{};
    bool swinging_{};
    // The speed boost's acceleration now.
    float boostRamp_{};
    // Where the swing is on its arc (the game's swing mode): 0 from a web's
    // attaching to the bottom of its arc, 1 once a taut web carries the body
    // up past it.
    int swingMode_{};
    // The momentum meter, the seconds since it last rose, since each source
    // raised it, and the times it has reached momentumHigh.
    float momentum_{}, sinceMomentum_ = 100, sinceLetGo_ = 100, sinceJumpOff_ = 100, sinceYank_ = 100;
    uint32_t surges_{};
};
} // namespace spidy
