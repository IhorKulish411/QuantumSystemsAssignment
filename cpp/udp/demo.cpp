#include "UdpMessenger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace
{
    // Minimal UDP listener, used only to make this demo self-verifying.
    void RunListener(std::atomic<bool>& running, std::uint16_t port)
    {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        timeval tv{0, 200000}; // 200ms recv timeout so we can check `running`
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        char buf[1024];
        while (running)
        {
            ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n > 0)
            {
                std::printf("[listener] received: %.*s\n", static_cast<int>(n), buf);
                std::fflush(stdout);
            }
        }
        ::close(fd);
    }
}

int main()
{
    constexpr std::uint16_t kPort = 9123;
    std::atomic<bool> listening = true;
    std::thread listener(RunListener, std::ref(listening), kPort);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let it bind

    UdpMessenger messenger;

    std::printf("sending immediate message\n");
    messenger.SendNow("127.0.0.1", kPort, "hello-now");

    std::printf("scheduling delayed message (2s)\n");
    messenger.SendDelayed("127.0.0.1", kPort, "hello-delayed", 2);

    std::printf("scheduling periodic message (every 1s)\n");
    auto periodicId = messenger.SendPeriodic("127.0.0.1", kPort, "hello-periodic", 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    std::printf("canceling periodic message\n");
    messenger.Cancel(periodicId);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    listening = false;
    listener.join();
    std::printf("done\n");
    return 0;
}
