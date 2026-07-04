#pragma once

#include <cstdint>
#include <string>

// Pure virtual interface for a UDP messaging service. Implementations must
// support three send modes:
//   - SendNow:      immediate, blocking send.
//   - SendDelayed:  non-blocking, fires once after `delaySeconds`.
//   - SendPeriodic: non-blocking, fires every `periodSeconds` until Cancel().
// delaySeconds/periodSeconds are restricted to [1, 255] (std::uint8_t already
// caps the upper bound; implementations must reject 0).
class IUdpMessenger
{
public:
    using TaskId = std::uint64_t;
    static constexpr TaskId kInvalidTaskId = 0;

    virtual ~IUdpMessenger() = default;

    virtual void SendNow(const std::string& ip, std::uint16_t port, const std::string& data) = 0;

    virtual TaskId SendDelayed(
        const std::string& ip,
        std::uint16_t port,
        const std::string& data,
        std::uint8_t delaySeconds) = 0;

    virtual TaskId SendPeriodic(
        const std::string& ip,
        std::uint16_t port,
        const std::string& data,
        std::uint8_t periodSeconds) = 0;

    // Cancels a previously scheduled delayed or periodic send. No-op if the
    // id is unknown or already fired/canceled.
    virtual void Cancel(TaskId id) = 0;
};
