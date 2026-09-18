#pragma once
#include <string_view>

// No engine dependencies: this same implementation is compiled by standalone CI.
namespace SPTravelValidation
{
inline bool IsDigit(char C) { return C >= '0' && C <= '9'; }
inline bool IsAlpha(char C) { return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z'); }
inline bool IsHex(char C) { return IsDigit(C) || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F'); }

inline bool IsIPv4(std::string_view Value)
{
    int Parts = 0;
    while (!Value.empty())
    {
        const auto End = Value.find('.');
        const auto Part = Value.substr(0, End);
        if (Part.empty() || Part.size() > 3 || (Part.size() > 1 && Part.front() == '0')) return false;
        int Number = 0;
        for (char C : Part) { if (!IsDigit(C)) return false; Number = Number * 10 + C - '0'; }
        if (Number > 255 || ++Parts > 4) return false;
        if (End == std::string_view::npos) return Parts == 4;
        Value.remove_prefix(End + 1);
    }
    return false;
}

inline int IPv6Groups(std::string_view Value, bool AllowIPv4)
{
    if (Value.empty()) return 0;
    int Count = 0;
    while (!Value.empty())
    {
        const auto End = Value.find(':');
        const auto Part = Value.substr(0, End);
        if (Part.find('.') != std::string_view::npos)
            return AllowIPv4 && End == std::string_view::npos && IsIPv4(Part) ? Count + 2 : -1;
        if (Part.empty() || Part.size() > 4) return -1;
        for (char C : Part) if (!IsHex(C)) return -1;
        ++Count;
        if (End == std::string_view::npos) return Count;
        Value.remove_prefix(End + 1);
    }
    return -1;
}

inline bool IsIPv6(std::string_view Value)
{
    const auto Compression = Value.find("::");
    if (Compression == std::string_view::npos) return IPv6Groups(Value, true) == 8;
    if (Value.find("::", Compression + 2) != std::string_view::npos) return false;
    const int Left = IPv6Groups(Value.substr(0, Compression), false);
    const int Right = IPv6Groups(Value.substr(Compression + 2), true);
    return Left >= 0 && Right >= 0 && Left + Right < 8;
}

inline bool IsValidHost(std::string_view Host)
{
    if (Host.empty() || Host.size() > 253) return false;
    if (Host.front() == '[')
        return Host.back() == ']' && IsIPv6(Host.substr(1, Host.size() - 2));
    if (Host.find(':') != std::string_view::npos) return IsIPv6(Host);
    bool Numeric = true;
    for (char C : Host) if (!IsDigit(C) && C != '.') Numeric = false;
    if (Numeric) return IsIPv4(Host);
    if (Host.back() == '.') Host.remove_suffix(1);
    while (!Host.empty())
    {
        const auto End = Host.find('.');
        const auto Label = Host.substr(0, End);
        if (Label.empty() || Label.size() > 63 || Label.front() == '-' || Label.back() == '-') return false;
        for (char C : Label) if (!IsAlpha(C) && !IsDigit(C) && C != '-') return false;
        if (End == std::string_view::npos) return true;
        Host.remove_prefix(End + 1);
    }
    return false;
}
}
