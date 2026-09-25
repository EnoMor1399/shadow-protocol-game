#include "SPTravelValidation.h"
#include <iostream>
#include <string>
#include <vector>
int main()
{
    const std::vector<std::string> Valid = {
        "localhost", "game-1.example.com", "GAME.example.com.", "127.0.0.1", "255.255.255.255",
        "::1", "[::1]", "2001:db8::1", "[2001:db8::1]", "1:2:3:4:5:6:7:8", "::ffff:192.0.2.1"
    };
    const std::vector<std::string> Invalid = {
        "", " ", "host ", " host", "host?spConnectToken=evil", "host#fragment", "host/path", "host\\path",
        "https://host", "user@host", "host:7777", "host%3Flisten", "-host", "host-", "a..b", "a..",
        "256.0.0.1", "127.1", "127.0.0.1.", "012.0.0.1", "2130706433", "[::1]:7777", "[::1",
        "::1]", "[]", ":::1", "1::2::3", "1:2:3:4:5:6:7", "1:2:3:4:5:6:7:8:9",
        "1:2:3:4:5:6:7::8", "fe80::1%eth0", "gggg::1", "::ffff:999.0.0.1", "192.0.2.1::",
        "host\nlisten", std::string("host\0evil", 9), std::string(64, 'a') + ".com", std::string(254, 'a')
    };
    for (const auto& Host : Valid) if (!SPTravelValidation::IsValidHost(Host)) { std::cerr << "Rejected valid host\n"; return 1; }
    for (const auto& Host : Invalid) if (SPTravelValidation::IsValidHost(Host)) { std::cerr << "Accepted invalid host\n"; return 1; }
    // Every non-host ASCII byte must be rejected even inside an otherwise valid label.
    for (int C = 0; C < 128; ++C)
    {
        if (SPTravelValidation::IsAlpha(static_cast<char>(C)) || SPTravelValidation::IsDigit(static_cast<char>(C)) || C == '-' || C == '.') continue;
        std::string Host = "game"; Host += static_cast<char>(C); Host += "server.example";
        if (SPTravelValidation::IsValidHost(Host)) return 1;
    }
    std::cout << "Travel validator: " << Valid.size() + Invalid.size() << " address cases and ASCII injection sweep passed\n";
}
