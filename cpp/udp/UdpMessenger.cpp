#include "UdpMessenger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace
{
    void ValidateSeconds(std::uint8_t seconds)
    {
        if (seconds < 1)
        {
            throw std::invalid_argument("delay/period must be between 1 and 255 seconds");
        }
    }
}

UdpMessenger::UdpMessenger()
    : socketFd_(::socket(AF_INET, SOCK_DGRAM, 0))
{
    if (socketFd_ < 0)
    {
        throw std::runtime_error(std::string("failed to create UDP socket: ") + std::strerror(errno));
    }
    schedulerThread_ = std::thread(&UdpMessenger::RunScheduler, this);
}

UdpMessenger::~UdpMessenger()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    if (schedulerThread_.joinable())
    {
        schedulerThread_.join();
    }
    ::close(socketFd_);
}

void UdpMessenger::SendNow(const std::string& ip, std::uint16_t port, const std::string& data)
{
    SendRaw(ip, port, data);
}

IUdpMessenger::TaskId UdpMessenger::SendDelayed(
    const std::string& ip,
    std::uint16_t port,
    const std::string& data,
    std::uint8_t delaySeconds)
{
    ValidateSeconds(delaySeconds);
    std::lock_guard<std::mutex> lock(mutex_);
    TaskId id = ScheduleLocked(ip, port, data, delaySeconds, std::chrono::seconds(0));
    cv_.notify_all();
    return id;
}

IUdpMessenger::TaskId UdpMessenger::SendPeriodic(
    const std::string& ip,
    std::uint16_t port,
    const std::string& data,
    std::uint8_t periodSeconds)
{
    ValidateSeconds(periodSeconds);
    std::lock_guard<std::mutex> lock(mutex_);
    TaskId id = ScheduleLocked(ip, port, data, periodSeconds, std::chrono::seconds(periodSeconds));
    cv_.notify_all();
    return id;
}

IUdpMessenger::TaskId UdpMessenger::ScheduleLocked(
    const std::string& ip,
    std::uint16_t port,
    const std::string& data,
    std::uint8_t delaySeconds,
    std::chrono::seconds period)
{
    TaskId id = nextId_++;
    queue_.push(ScheduledTask{
        id,
        std::chrono::steady_clock::now() + std::chrono::seconds(delaySeconds),
        period,
        ip,
        port,
        data});
    return id;
}

void UdpMessenger::Cancel(TaskId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    canceled_.insert(id);
    cv_.notify_all();
}

void UdpMessenger::RunScheduler()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopping_)
    {
        if (queue_.empty())
        {
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            continue;
        }

        ScheduledTask next = queue_.top();
        bool wokenEarly = cv_.wait_until(lock, next.nextFire, [this, &next] {
            return stopping_ || queue_.empty() || queue_.top().nextFire < next.nextFire;
        });
        if (wokenEarly)
        {
            // Stop requested, or a new task with an earlier fire time was
            // scheduled while we waited: re-evaluate from the top instead
            // of firing `next`.
            continue;
        }

        queue_.pop();
        bool isCanceled = canceled_.erase(next.id) > 0;
        if (!isCanceled)
        {
            // Send without holding the lock: sendto() should never block
            // scheduling of other tasks.
            lock.unlock();
            SendRaw(next.ip, next.port, next.data);
            lock.lock();
        }

        if (!isCanceled && next.period.count() > 0)
        {
            next.nextFire = std::chrono::steady_clock::now() + next.period;
            queue_.push(std::move(next));
        }
    }
}

void UdpMessenger::SendRaw(const std::string& ip, std::uint16_t port, const std::string& data)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1)
    {
        throw std::invalid_argument("invalid IPv4 address: " + ip);
    }

    ssize_t sent = ::sendto(
        socketFd_, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (sent < 0)
    {
        throw std::runtime_error(std::string("sendto failed: ") + std::strerror(errno));
    }
}
