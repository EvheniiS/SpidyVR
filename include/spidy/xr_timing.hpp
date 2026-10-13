#pragma once
#include <algorithm>
#include <cstdint>

namespace spidy {
struct XrFrameTiming {
    double total{}, wait{}, tracking{}, prepare{}, acquire{}, release{}, end{}, period{};
};
struct TimingMetric {
    uint64_t count{};
    double totalMs{}, maxMs{}, lastMs{};
    void add(double ms) {
        ++count;
        totalMs += ms;
        maxMs = std::max(maxMs, ms);
        lastMs = ms;
    }
};
// Smoothness, not averages: how evenly new scene pairs reach the headset.
constexpr unsigned cadenceBuckets = 128; // 0.5 ms each, the last one is "that or longer"
inline unsigned cadenceBucket(double ms) {
    if (!(ms > 0))
        return 0;
    const auto b = static_cast<unsigned>(ms * 2);
    return b < cadenceBuckets ? b : cadenceBuckets - 1;
}
struct XrTimingData {
    uint32_t magic = 0x5358544d, version = 2, bytes = sizeof(XrTimingData), reserved{};
    int64_t sequence{};
    // frame, wait-frame, tracking, prepare, acquire, copy submission wait,
    // hand overlay CPU, release, end-frame, runtime display period.
    TimingMetric stages[10];
    // Gaps between consecutive new scene pairs (menus and loading excluded)
    // and the XR loop's frame time, both in 0.5 ms buckets.
    uint32_t newGapHist[cadenceBuckets]{}, loopHist[cadenceBuckets]{};
    uint64_t newGaps{}, loopFrames{};
    // Gaps over 1.5 and 2 display periods, the longest run of reused images
    // between two new pairs, and the run in progress.
    uint32_t newOver15{}, newOver2{}, maxReuseRun{}, reuseRun{};
    double maxGapMs{}, periodMs{};
    void loop(double ms) {
        ++loopFrames;
        ++loopHist[cadenceBucket(ms)];
    }
    void newPair(double gapMs, double period) {
        periodMs = period;
        reuseRun = 0;
        if (gapMs <= 0)
            return;
        ++newGaps;
        ++newGapHist[cadenceBucket(gapMs)];
        newOver15 += period > 0 && gapMs > 1.5 * period;
        newOver2 += period > 0 && gapMs > 2 * period;
        maxGapMs = std::max(maxGapMs, gapMs);
    }
    void reused() {
        maxReuseRun = std::max(maxReuseRun, ++reuseRun);
    }
};
static_assert(sizeof(TimingMetric) == 32 && sizeof(XrTimingData) == 1416);
} // namespace spidy
