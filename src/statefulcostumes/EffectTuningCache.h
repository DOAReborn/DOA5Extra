#pragma once
#include <windows.h>
#include <cstdint>

namespace EffectTuning {
struct Snapshot {
    float peakAlpha;
    unsigned sphereBright;
    unsigned ribbonBright;
    bool costumePieces;
};

class Cache {
    static constexpr uint32_t DefaultPacked = 80u | (50u << 8) | (50u << 16) | (1u << 24);
    alignas(4) mutable volatile LONG packed_ = static_cast<LONG>(DefaultPacked);
    uint32_t lastPoll_ = 0;
    bool polled_ = false;

    static unsigned Percent(int value) noexcept {
        return value < 1 ? 1u : value > 100 ? 100u : static_cast<unsigned>(value);
    }
public:
    Snapshot Read() const noexcept {
        const uint32_t packed = static_cast<uint32_t>(InterlockedCompareExchange(&packed_, 0, 0));
        return Snapshot{(packed & 255u) / 100.0f, (packed >> 8) & 255u,
                        (packed >> 16) & 255u, (packed & (1u << 24)) != 0};
    }

    template<class Reader> bool PollFromWorker(uint32_t now, Reader read) noexcept {
        if (polled_ && now - lastPoll_ < 1000u) return false;
        lastPoll_ = now;
        polled_ = true;
        const unsigned alpha = Percent(read("SphereAlpha", 80));
        const unsigned sphere = Percent(read("SphereBright", 50));
        const unsigned ribbon = Percent(read("RibbonBright", 50));
        const unsigned pieces = read("CostumePieces", 1) != 0 ? 1u : 0u;
        const uint32_t packed = alpha | (sphere << 8) | (ribbon << 16) | (pieces << 24);
        InterlockedExchange(&packed_, static_cast<LONG>(packed));
        return true;
    }
};
static_assert(sizeof(LONG) == sizeof(uint32_t), "Effect settings use one 32-bit atomic snapshot");
}
