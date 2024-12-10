#pragma once

#include <chrono>
#include <format>

namespace flr
{
    template <typename Rep, typename Per>
    struct HumanizedDuration
    {
        std::chrono::duration<Rep, Per> duration;
    };

    template <typename Rep, typename Per>
    constexpr auto humanize(const std::chrono::duration<Rep, Per> duration) noexcept
    {
        return HumanizedDuration<Rep, Per>{duration};
    }
} // namespace flr

template <typename Rep, typename Per>
struct std::formatter<flr::HumanizedDuration<Rep, Per>> // NOLINT(cert-dcl58-cpp)
{
    constexpr auto parse(auto& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end() || *it == '}')
        {
            width_ = default_width;
            return it;
        }
        for (; it != ctx.end() && *it != '}'; ++it)
        {
            const char ch = *it;
            if (ch < '0' || ch > '9')
                throw std::format_error("Invalid width specifier");
            width_ = width_ * 10 + (ch - '0');
        }
        return it;
    }

    auto format(const flr::HumanizedDuration<Rep, Per>& dur, auto& ctx) const
    {
        namespace chr = std::chrono;
        struct Scale
        {
            chr::duration<double, std::nano> duration;
            std::string_view unit;
            std::size_t width;
        } constexpr scales[]{
            {chr::days{1}, "d", 1}, //
            {chr::hours{1}, "h", 1}, //
            {chr::minutes{1}, "min", 3}, //
            {chr::seconds{1}, "s", 1}, //
            {chr::milliseconds{1}, "ms", 2}, //
            {chr::microseconds{1}, "μs", 2} //
        };
        Scale scale{chr::nanoseconds{1}, "ns", 2};
        for (const auto& s : scales)
            if (dur.duration >= s.duration)
            {
                scale = s;
                break;
            }
        const auto [unit, str, width] = scale;
        return std::format_to(ctx.out(), "{:{}g}{}", dur.duration / unit, width_ - width, str);
    }

private:
    static constexpr int default_width = 10;
    int width_ = 0;
};
