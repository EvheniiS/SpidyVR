#pragma once
#include "slow_motion.hpp"
#include "vertex.hpp"
#include "web_strike.hpp"
#include <cstdint>
#include <vector>

namespace spidy {
// One web as drawn over the eye images. Positions are world metres.
struct WebLine {
    Vec3 start{}, end{}; // wrist and anchor
    float slack{};       // rope length beyond the straight distance; 0 when taut
    float extended = 1;  // 0..1 of the way from the wrist while the web is shot
    bool splat = true;   // impact web at the anchor once the shot arrives
    float seed{};        // varies splat strands between webs
};
struct WebLook {
    float radius = .005f;   // half width of the strand
    float minPixels = 1.6f; // on-screen width floor, so distant webs stay continuous
    Vec3 core{.86f, .87f, .9f}, edge{.32f, .36f, .44f};
};
// Camera-facing braided strand with a twisting sheen, parabolic sag when
// slack, a travelling tip during the shot, and a splat at the anchor.
// pixelAngle is the eye's angular size of one pixel, in radians.
void appendWeb(std::vector<Vertex>& out, const WebLine& web, Vec3 viewer, float pixelAngle,
               const WebLook& look = {});
// Shot and release timing shared by the headset overlay and the lab.
struct WebAnimation {
    static constexpr float shotSeconds = .09f, releaseSeconds = .22f;
    // Portion of the web extended this long after attachment.
    static float extended(float sinceAttach) {
        return std::isfinite(sinceAttach) ? std::clamp(sinceAttach / shotSeconds, 0.f, 1.f) : 1.f;
    }
    // After release the free end whips back toward the anchor and sags.
    static bool released(float sinceRelease, Vec3 hand, Vec3 anchor, WebLine& out) {
        if (!std::isfinite(sinceRelease) || sinceRelease < 0 || sinceRelease >= releaseSeconds)
            return false;
        const float t = sinceRelease / releaseSeconds, pull = t * t * (3 - 2 * t);
        out.start = hand + (anchor - hand) * pull;
        out.end = anchor;
        out.slack = length(anchor - hand) * .35f * (1 - pull);
        out.extended = 1;
        return length(out.end - out.start) > .05f;
    }
};
// When each hand's web was shot or let go, in predicted display time (ns).
class WebTimeline {
  public:
    struct Hand {
        int64_t attachedAt{}, releasedAt{};
        Vec3 anchor{}, wrist{};
        bool attached{};
    };
    void update(unsigned hand, bool attached, Vec3 anchor, Vec3 wrist, int64_t now) {
        auto& h = hands_.at(hand);
        if (attached && !h.attached)
            h.attachedAt = now;
        if (!attached && h.attached)
            h.releasedAt = now;
        if (attached) {
            h.anchor = anchor;
            h.wrist = wrist;
        }
        h.attached = attached;
    }
    const Hand& operator[](unsigned hand) const {
        return hands_.at(hand);
    }
    void clear() {
        hands_ = {};
    }

  private:
    std::array<Hand, 2> hands_{};
};
// The lab's web start, from a hand's aim-oriented pose (the game takes the
// shooter from the grip: webShooterPoint in swing.hpp).
inline Vec3 webWrist(Pose hand) {
    return hand.position + hand.orientation.rotate({0, -.025f, .03f});
}
// Aim markers: where a grip press would send a hand's web now, drawn over the
// eye images facing the viewer, the same size on screen at any distance, with
// soft edges and a soft dark shadow. Each hand has its own colour: the left
// hand's markers are sky blue, the right hand's orange.
enum class AimMark : std::uint8_t {
    anchor,  // the web attaches here: a ring around a dot
    air,     // nothing within reach: the web attaches in the air, a faint dashed ring
    blocked, // the web misses here: a red cross, either hand
    target,  // the web catches what is here: a turning ring of three arcs around it
};
struct AimMarker {
    AimMark kind = AimMark::anchor;
    Vec3 point{};      // where the web goes; a target's centre
    float radius{};    // a target's radius, metres
    float squeeze{};   // the hand's grip, 0..1: the marker tightens as it closes
    unsigned hand{};   // 0 the left hand, 1 the right: the marker's colour
    float opacity = 1; // 0..1 while the marker fades in or out
    float spin{};      // radians the target's ring has turned
    float lock = 1;    // 0..1 as the target's ring closes in on it from wider
};
// pixelAngle as for appendWeb.
void appendAimMarker(std::vector<Vertex>& out, const AimMarker& marker, Vec3 viewer, float pixelAngle);
// Steadies one hand's aim marker from eye image to eye image. Hand tremor and
// tracking noise swing the aim ray by a few tenths of a degree, which the
// marker shows as jitter at any distance; the aim previews arrive at the
// game's own rate. The ray's direction goes through a speed-adaptive low-pass
// (the "1 euro filter": steady while the hand holds still, a few tenths of a
// degree behind while it sweeps), in the tracking space so a snap turn is no
// motion; a marker eases along the ray to a new distance, a target's ring
// slides to a new target, and each kind of marker fades in and out instead of
// switching at once.
class AimMarkerMotion {
  public:
    static constexpr float minCutoffHz = 1, speedCutoff = 60, speedDeadband = .06f, speedCutoffHz = 4;
    static constexpr float fadeInSeconds = .04f, lockSeconds = .12f, fadeOutSeconds = .08f;
    static constexpr float distanceSeconds = .03f, targetSeconds = .035f, gapSeconds = .25f;
    // Starts the eye image shown at `timeNs` (predicted display time): the
    // same image again changes nothing, and after a gap of over gapSeconds
    // the markers start afresh.
    void begin(int64_t timeNs);
    // The steadied direction of the hand's aim ray for this image, from its
    // `direction` (world, unit) and the tracking space's yaw in the world.
    Vec3 aim(Vec3 direction, float yaw);
    // The same with the tracking space's whole orientation (trackingTurn): a
    // flip's tilt is no motion of the aim either.
    Vec3 aim(Vec3 direction, Quat turn);
    // Appends what to draw for this image: `wanted` (nullptr: no marker) on
    // the steadied ray from `origin` along `direction` (from aim()), easing
    // in, and the markers before it fading out where they were last wanted.
    void markers(const AimMarker* wanted, Vec3 origin, Vec3 direction, std::vector<AimMarker>& out);
    void reset();

  private:
    struct Shown {
        AimMarker marker; // the latest wanted marker of this kind
        float weight{};   // 0..1: its fade
        float lock{};     // 0..1: a target's ring closing in
        float distance{}; // along the ray (anchor, air, blocked)
    };
    std::array<Shown, 4> kinds_{};
    Vec3 steady_{}, speed_{}; // the filtered direction in the tracking space, and its rate
    bool filtering_{};
    int64_t time_{};
    float seconds_{}; // since the image before
    float spin_{};
};
// Where the marker for a surface the aim ray met at `point` (facing `normal`)
// goes on an aim ray from `origin` along unit `direction`: where that ray
// crosses the surface's plane, so the marker stays on the line the hand
// points along until the next preview arrives. `point` itself when the ray
// runs nearly along the plane or crosses it far from `point`.
Vec3 onAimLine(Vec3 point, Vec3 normal, Vec3 origin, Vec3 direction);
// Slow motion's focus meter (slow_motion.hpp) over the back of the left
// wrist, facing the viewer: a cyan ring around an hourglass that empties
// clockwise from the top as focus is spent and fills again as it refills. In
// slow motion it brightens and pulses; a refused press or an empty meter
// flashes it red. `hand` is the left hand's pose as the glove's (fingers
// along -z, the back of the hand +y); `seconds` any running clock, for the
// pulse. pixelAngle as for appendWeb.
void appendSlowMotionMeter(std::vector<Vertex>& out, const SlowMotionView& view, Pose hand, Vec3 viewer,
                           float pixelAngle, float seconds);
// The combat marks below are printed, as the graphics of Spidy's videos are
// (their style system: flat plates of colour, every shape keyed in ink, a
// hard drop where a shadow would be, lettering out of register as it lands),
// and they move on twos: a pose held a twelfth of a second, then the next,
// with nothing eased or faded.
//
// A landed blow where the fist met him (web_strike.hpp), facing the viewer:
// a comic's burst. A star, small and then whole, yellow around paper for a
// jab and red around yellow for a harder blow; then its lines thrown clear,
// a pose for a jab and two for a harder blow. A finisher's has a third,
// widest star and three poses of lines. Above it the blow's count in the
// flow, from the second on ("X3"): yellow lettering keyed in ink that pops
// in off level, holds, and is gone in two poses.
struct ImpactView {
    static constexpr float seconds = .75f; // until the count has gone too
    Vec3 point{};
    float strength{}; // 0..1
    BlowKind kind{};
    int count{};
    float age = -1; // seconds since it landed; below 0, or past `seconds`: nothing
    // A called blow (Cues): its star is out of register, a cyan ghost to its
    // left and a magenta one to its right.
    bool called{};
};
// pixelAngle as for appendWeb.
void appendImpact(std::vector<Vertex>& out, const ImpactView& impact, Vec3 viewer, float pixelAngle);
// The mark over the chest of the enemy the player's next punch would go to
// (game_punch), facing the viewer, with what the flow calls (web_strike.hpp's
// Cues). Nothing called: a thin paper ring, a target and no more; once a
// blow has locked the player onto him (Lock) the ring is bolder, with four
// ticks in from it. A call: an arrow in a marker's stroke pointing the way
// the fist must travel, as the viewer sees him (left, right, up or down),
// coming in from the side the fist comes from and ending at his middle, so
// that "from there" and "that way" are one reading; paper on a cyan drop,
// over a cyan ring that runs down clockwise, a notch at a time, as the
// flow's time does. The call for the flow's finisher is
// larger: a yellow arrow on a red drop inside a burst's edge, a red zigzag
// that runs down the same way. A call pops in (three poses, the first two
// out of register) and its arrow then boils as a held drawing does; the mark
// keeps a readable size at a distance.
struct CueView {
    Vec3 point{};
    CueWay way{}; // None: the plain ring
    bool finisher{};
    float left{}; // 1..0 of the flow's time; 0: nothing at all
    float age{};  // seconds since it showed
    bool locked{}; // a blow holds the mark on him: it does not follow the look
};
// pixelAngle as for appendWeb.
void appendCue(std::vector<Vertex>& out, const CueView& cue, Vec3 viewer, float pixelAngle);
// A knockout of the player's, lettered where the enemy stood: "KO!" in yellow
// on a red burst with a paper heart, as a sound word is. It pops in, holds,
// and is gone in two poses: this one is done, look for the next.
struct KnockoutView {
    static constexpr float seconds = 8.f / 12; // eight poses
    Vec3 point{};
    float age = -1; // seconds since he went down; below 0, or past `seconds`: nothing
};
// pixelAngle as for appendWeb.
void appendKnockout(std::vector<Vertex>& out, const KnockoutView& knockout, Vec3 viewer, float pixelAngle);
} // namespace spidy
