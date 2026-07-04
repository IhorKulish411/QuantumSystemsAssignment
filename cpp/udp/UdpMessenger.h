#pragma once

#include "IUdpMessenger.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

// Concrete UdpMessenger: one shared UDP socket for all sends, plus a single
// background scheduler thread that services delayed/periodic tasks from a
// min-heap ordered by next fire time. No thread is spawned per call.
class UdpMessenger final : public IUdpMessenger
{
public:
    UdpMessenger();
    ~UdpMessenger() override;

    UdpMessenger(const UdpMessenger&) = delete;
    UdpMessenger& operator=(const UdpMessenger&) = delete;

    void SendNow(const std::string& ip, std::uint16_t port, const std::string& data) override;

    TaskId SendDelayed(
        const std::string& ip,
        std::uint16_t port,
        const std::string& data,
        std::uint8_t delaySeconds) override;

    TaskId SendPeriodic(
        const std::string& ip,
        std::uint16_t port,
        const std::string& data,
        std::uint8_t periodSeconds) override;

    void Cancel(TaskId id) override;

private:
    struct ScheduledTask
    {
        TaskId id;
        std::chrono::steady_clock::time_point nextFire;
        std::chrono::seconds period; // zero for one-shot delayed sends
        std::string ip;
        std::uint16_t port;
        std::string data;

        // std::greater<> ordering below needs operator>: smaller nextFire
        // sorts as "greater" here so priority_queue keeps it at top().
        bool operator>(const ScheduledTask& other) const
        {
            return nextFire > other.nextFire;
        }
    };

    TaskId ScheduleLocked(
        const std::string& ip,
        std::uint16_t port,
        const std::string& data,
        std::uint8_t delaySeconds,
        std::chrono::seconds period);
    void RunScheduler();
    void SendRaw(const std::string& ip, std::uint16_t port, const std::string& data);

    int socketFd_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, std::greater<>> queue_;
    // Ids canceled before they fired. Entries for ids that never get popped
    // again (already fired one-shots, unknown ids) are never removed; for
    // the scope of this exercise that's an acceptable bounded-in-practice
    // leak. A production version would index tasks by id (e.g. map<id,
    // task*>) to erase in place instead of marking-and-checking.
    std::unordered_set<TaskId> canceled_;
    TaskId nextId_ = 1;
    bool stopping_ = false;
    std::thread schedulerThread_;
};
