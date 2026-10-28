#pragma once
#include "Burst.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cb {

struct PS4EmitterSpec {
    unsigned delayTicks;
    unsigned birthsPerWave;
    float minimumLifeTicks;
    float maximumLifeTicks;
    unsigned texture;
    float minimumSize;
    float maximumSize;
    float minimumConeCosine;
    float maximumConeCosine;
    float dampingPerTick;
    float minimumAcceleration;
    float gravityPerTick;
    unsigned minimumFadeOut;
    unsigned maximumFadeOut;
    bool squaredFadeOut;
};

inline constexpr std::array<PS4EmitterSpec, 5> ps4EmitterSpecs{{
    {6, 200, 35, 120, 0, .010f, .100f, .80f, 1.00f, .225f, .0007f, .0010f,  8, 10, true },
    {7, 200, 45, 120, 1, .015f, .075f, .80f, 1.00f, .225f, .0010f, .0005f,  8, 10, true },
    {8, 200, 55, 120, 2, .025f, .075f, .80f, 1.00f, .225f, .0007f, .0005f, 10, 15, false},
    {8, 500, 45, 120, 2, .025f, .075f, .30f,  .80f, .250f, .0007f, .0010f, 10, 15, false},
    {8, 500, 45, 120, 0, .025f, .075f, .35f,  .95f, .300f, .0007f, .0010f, 10, 15, false},
}};

struct PS4Burst {

    static constexpr unsigned wavesPerEmitter = 6;
    static constexpr std::size_t capacity = 9600;
    static constexpr float ticksPerSecond = 60;
    static constexpr float lastBirthTick = 13;
    static constexpr float maximumAgeTicks = lastBirthTick + 120;

    Vec origin{};
    float age = 0;

    void start(Vec position, std::uint32_t seed, const float* basis = nullptr) {
        origin = position;
        age = 0;
        elapsedTicks_ = 0;
        count_ = 0;
        groupCount_ = 0;
        started_ = finite(position);
        if (!started_) return;
        if (!seed) seed = 0x9e3779b9u;

        Vec axis{0, 0, -1};
        if (basis) {
            axis = {-basis[8], -basis[9], -basis[10]};
            if (!normalize(axis)) { started_ = false; return; }
        }

        Vec right = cross(std::abs(axis.y) < .9f ? Vec{0, 1, 0} : Vec{1, 0, 0}, axis);
        normalize(right);
        const Vec up = cross(axis, right);

        for (unsigned tick = 6; tick <= unsigned(lastBirthTick); ++tick) {
            for (unsigned emitter = 0; emitter < ps4EmitterSpecs.size(); ++emitter) {
                const auto& spec = ps4EmitterSpecs[emitter];
                if (tick < spec.delayTicks ||
                    tick >= spec.delayTicks + wavesPerEmitter) continue;
                Wave& wave = waves_[groupCount_++];
                wave = {count_, 0, tick, emitter};
                for (unsigned birth = 0; birth < spec.birthsPerWave; ++birth) {
                    if (count_ == capacity) return;
                    ParticleState& p = particles_[count_++];
                    ++wave.count;

                    p.lifeTicks = float(integer(seed, unsigned(spec.minimumLifeTicks),
                                               unsigned(spec.maximumLifeTicks)));
                    p.size = range(seed, spec.minimumSize, spec.maximumSize);
                    const float roll = range(seed, -.5f * pi, .5f * pi);
                    p.alpha = range(seed, .5f, .75f);
                    p.fadeInTicks = float(integer(seed, 1, 2));
                    p.fadeOutTicks = float(integer(seed, spec.minimumFadeOut, spec.maximumFadeOut));

                    const float azimuth = range(seed, -pi, pi);
                    const float cosine = range(seed, spec.minimumConeCosine, spec.maximumConeCosine);
                    const float radial = std::sqrt(std::max(0.f, 1 - cosine * cosine));
                    const Vec direction = right * (radial * std::cos(azimuth)) +
                                          up * (radial * std::sin(azimuth)) + axis * cosine;
                    p.rotation = {-std::asin(std::clamp(direction.y, -1.f, 1.f)),
                                  std::atan2(direction.x, direction.z), roll};
                    p.offset = direction * range(seed, .3f, .5f);

                    p.velocity = direction * range(seed, .5f, 1.f) + Vec{0, 0, .018f};
                    p.acceleration = direction * range(seed, spec.minimumAcceleration, .0035f) +
                                     Vec{0, -spec.gravityPerTick, 0};
                }
            }
        }
    }

    void advance(float seconds) {
        if (!started_ || !std::isfinite(seconds) || seconds <= 0) return;
        elapsedTicks_ = std::min(double(maximumAgeTicks),
                                elapsedTicks_ + double(seconds) * ticksPerSecond);
        age = float(elapsedTicks_ / ticksPerSecond);
    }

    bool alive() const { return started_ && elapsedTicks_ < maximumAgeTicks; }
    std::size_t particleCount() const { return count_; }

    std::size_t bornCount() const {
        std::size_t result = 0;
        for (std::size_t i = 0; i < groupCount_; ++i) {
            if (waves_[i].birthTick > elapsedTicks_ + boundaryTolerance) break;
            result += waves_[i].count;
        }
        return result;
    }

    template<class Emit> void draw(Emit emit) const {
        if (!alive()) return;
        for (std::size_t group = 0; group < groupCount_; ++group) {
            const auto& wave = waves_[group];
            const auto& spec = ps4EmitterSpecs[wave.emitter];
            const double delta = elapsedTicks_ - wave.birthTick;
            if (delta < -boundaryTolerance) break;
            const float t = std::abs(delta) <= boundaryTolerance ? 0.f : float(std::max(0., delta));
            const Motion motion = displacement(t, spec.dampingPerTick);
            for (std::size_t i = wave.first; i < wave.first + wave.count; ++i) {
                const auto& p = particles_[i];
                if (t >= p.lifeTicks) continue;
                const float fadeIn = std::clamp(t / p.fadeInTicks, 0.f, 1.f);
                float fadeOut = std::clamp((p.lifeTicks - t) / p.fadeOutTicks, 0.f, 1.f);
                if (spec.squaredFadeOut) fadeOut *= fadeOut;
                const float alpha = p.alpha * fadeIn * fadeOut;
                if (alpha <= 0) continue;
                const Vec center = origin + p.offset + p.velocity * motion.velocity +
                                   p.acceleration * motion.acceleration;

                emit(Quad{center, p.rotation + Vec{0, 0, .15f * t},
                          p.size, p.size, alpha, spec.texture});
            }
        }
    }

private:
    struct ParticleState {
        Vec offset;
        Vec velocity;
        Vec acceleration;
        float lifeTicks;
        float size;
        Vec rotation;
        float alpha;
        float fadeInTicks;
        float fadeOutTicks;
    };
    struct Wave {
        std::size_t first;
        std::size_t count;
        unsigned birthTick;
        unsigned emitter;
    };
    struct Motion {
        float velocity;
        float acceleration;
    };

    std::array<ParticleState, capacity> particles_{};
    std::array<Wave, wavesPerEmitter * ps4EmitterSpecs.size()> waves_{};
    double elapsedTicks_ = 0;
    std::size_t count_ = 0;
    std::size_t groupCount_ = 0;
    bool started_ = false;
    static constexpr double boundaryTolerance = 1e-5;

    static bool finite(Vec v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }
    static float random(std::uint32_t& s) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return float(s >> 8) / 16777216.f;
    }
    static float range(std::uint32_t& s, float low, float high) {
        return low + (high - low) * random(s);
    }
    static unsigned integer(std::uint32_t& s, unsigned low, unsigned high) {
        return low + unsigned(random(s) * float(high - low + 1));
    }
    static Motion displacement(float ticks, float damping) {

        const double whole = std::floor(double(ticks));
        const double fraction = double(ticks) - whole;
        const double d = damping;
        const double decay = std::pow(1 - d, whole);
        const double sum = (1 - d * .5) * (1 - decay) / d;
        const double partial = fraction - d * fraction * fraction * .5;
        return {float(sum + decay * partial),
                float((whole - sum) / d + (1 - decay) * partial / d +
                      fraction * fraction * .5)};
    }
};

static_assert(PS4Burst::capacity == PS4Burst::wavesPerEmitter * (200 + 200 + 200 + 500 + 500),
              "Emitter schedule must fit fixed storage");

}
