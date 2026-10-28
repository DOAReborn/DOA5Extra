#pragma once
#include <cstdint>
#include <limits>

namespace onlinecostume {
struct Context {
    bool readable = false;
    std::uint32_t context = 0;
    std::uint8_t networkFlags = 0;
    bool optIn = false;
    std::uint32_t lobbyType = 0;
    bool frameReadable = false;
    bool frameStable = false;
    bool rollbackQueryKnown = false;
    bool rollbackActive = false;

    bool rollbackConfiguredDisabled = false;

    bool rollbackOptIn = false;
};

inline bool OfflineAllowed(const Context& c) noexcept {
    return c.readable && (c.context & 1u) && !(c.networkFlags & 8u);
}

inline bool NativeOnlineAllowed(const Context& c) noexcept {
    return c.readable && (c.networkFlags & 8u) && c.optIn &&
           c.lobbyType == 3u && c.frameReadable && c.frameStable &&
           c.rollbackQueryKnown && !c.rollbackActive;
}

inline bool RollbackOnlineAllowed(const Context& c) noexcept {
    return c.readable && (c.networkFlags & 8u) && c.optIn && c.rollbackOptIn &&
           c.lobbyType == 3u && c.frameReadable && c.rollbackQueryKnown && c.rollbackActive;
}

inline bool Allowed(const Context& c) noexcept {
    return OfflineAllowed(c) || NativeOnlineAllowed(c) || RollbackOnlineAllowed(c);
}

struct ClockSample {
    bool valid = false;
    bool advanced = false;
    std::uint32_t milliseconds = 0;
};

class FrameClock {
public:

    static constexpr std::uint32_t maxFrameAdvance = 120u;
    static constexpr std::uint32_t initialMilliseconds = 1000u;

    void Reset() noexcept {
        initialized_ = false;
        invalid_ = false;
        lastFrame_ = 0;
        elapsedFrames_ = 0;
        milliseconds_ = initialMilliseconds;
    }

    void Invalidate() noexcept { invalid_ = true; }
    bool Invalid() const noexcept { return invalid_; }

    ClockSample Observe(std::uint32_t frame, bool allowRewind = false) noexcept {
        if (invalid_) return {false, false, milliseconds_};
        if (!initialized_) {
            initialized_ = true;
            lastFrame_ = frame;
            return {true, false, milliseconds_};
        }

        if (frame < lastFrame_) {
            if (allowRewind) return {true, false, milliseconds_};
            return Reject();
        }
        const std::uint32_t delta = frame - lastFrame_;
        if (delta > maxFrameAdvance) return Reject();
        if (!delta) return {true, false, milliseconds_};
        const std::uint64_t elapsed = elapsedFrames_ + delta;

        const std::uint64_t ms = initialMilliseconds +
                                (elapsed * 1000u + 59u) / 60u;
        if (ms > std::numeric_limits<std::uint32_t>::max()) return Reject();
        elapsedFrames_ = elapsed;
        lastFrame_ = frame;
        milliseconds_ = static_cast<std::uint32_t>(ms);
        return {true, true, milliseconds_};
    }

private:
    ClockSample Reject() noexcept {
        invalid_ = true;
        return {false, false, milliseconds_};
    }
    bool initialized_ = false;
    bool invalid_ = false;
    std::uint32_t lastFrame_ = 0;
    std::uint64_t elapsedFrames_ = 0;
    std::uint32_t milliseconds_ = initialMilliseconds;
};
}
