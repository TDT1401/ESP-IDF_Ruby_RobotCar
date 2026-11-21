#pragma once

#include <cstdint>

namespace ruby_robot {

// Lease for the single browser driver. The caller owns the mutex and the motor
// stop; this value object only tracks command validity.
class WebControlSession {
public:
    static constexpr int kTimeoutMs = 600;

    void SetConnected(bool connected) { connected_ = connected; }
    bool connected() const { return connected_; }
    bool active() const { return token_ != 0; }
    bool CanBegin(int64_t now_us) const {
        return connected_ && (!active() || now_us >= deadline_us_);
    }
    void Begin(uint32_t token, int64_t now_us) {
        token_ = token;
        sequence_ = 0;
        Refresh(now_us);
    }
    bool Accept(uint32_t token, uint32_t sequence, int64_t now_us) {
        if (!connected_ || token == 0 || token != token_ || sequence <= sequence_ ||
            now_us >= deadline_us_)
            return false;
        sequence_ = sequence;
        return true;
    }
    void Refresh(int64_t now_us) { deadline_us_ = now_us + kTimeoutMs * 1000LL; }
    bool Expired(int64_t now_us) const { return active() && now_us >= deadline_us_; }
    void Revoke() {
        token_ = 0;
        sequence_ = 0;
        deadline_us_ = 0;
    }

private:
    bool connected_ = false;
    uint32_t token_ = 0;
    uint32_t sequence_ = 0;
    int64_t deadline_us_ = 0;
};

}  // namespace ruby_robot
