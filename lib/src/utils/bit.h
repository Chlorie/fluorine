#pragma once

#include <bit>

#include "fluorine/core/macros.h"

#ifdef FLUORINE_HAS_BMI2
    #include <immintrin.h>
#endif

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    /// A 64-bit mask interpreted as an 8x8 bit row-major matrix.
    /// The top-left bit is the least significant bit.
    using BitBoard = std::uint64_t;

    namespace detail
    {
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

        constexpr BitBoard pdep_fallback(const BitBoard value, BitBoard mask) noexcept
        {
            BitBoard res = 0;
            for (BitBoard bit = 1; mask; bit <<= 1)
            {
                if (value & bit)
                    res |= mask & -mask;
                mask &= mask - 1;
            }
            return res;
        }

        constexpr BitBoard pext_fallback(const BitBoard value, BitBoard mask) noexcept
        {
            BitBoard res = 0;
            for (BitBoard bit = 1; mask; bit <<= 1)
            {
                if (value & mask & -mask)
                    res |= bit;
                mask &= mask - 1;
            }
            return res;
        }

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
    } // namespace detail

    // clang-format off
    [[nodiscard]] constexpr BitBoard bit_expandr(const BitBoard value, const BitBoard mask) noexcept
    {
#ifdef FLUORINE_HAS_BMI2
        FLUORINE_IF_NOT_CONSTEVAL { return _pdep_u64(value, mask); } else
#endif
        { return detail::pdep_fallback(value, mask); }
    }

    [[nodiscard]] constexpr BitBoard bit_compressr(const BitBoard value, const BitBoard mask) noexcept
    {
#ifdef FLUORINE_HAS_BMI2
        FLUORINE_IF_NOT_CONSTEVAL { return _pext_u64(value, mask); } else
#endif
        { return detail::pext_fallback(value, mask); }
    }
    // clang-format on

    struct FLUORINE_API SetBits
    {
        BitBoard bits;

        struct FLUORINE_API Iterator
        {
            BitBoard bits;

            [[nodiscard]] constexpr friend bool operator==(Iterator, Iterator) noexcept = default;
            [[nodiscard]] constexpr int operator*() const noexcept { return std::countr_zero(bits); }

            constexpr Iterator& operator++() noexcept
            {
                bits &= bits - 1;
                return *this;
            }
        };

        [[nodiscard]] constexpr Iterator begin() const noexcept { return Iterator{bits}; }
        [[nodiscard]] constexpr Iterator end() const noexcept { return Iterator{}; }
    };

    inline constexpr BitBoard no_a_file = 0xfefefefe'fefefefeull;
    inline constexpr BitBoard no_h_file = 0x7f7f7f7f'7f7f7f7full;
    inline constexpr BitBoard center_6x6 = 0x007e7e7e'7e7e7e00ull;
    inline constexpr BitBoard middle_6files = 0x7e7e7e7e'7e7e7e7eull;

    [[nodiscard]] constexpr BitBoard shift_west(const BitBoard bits) noexcept { return (bits & no_a_file) >> 1; }
    [[nodiscard]] constexpr BitBoard shift_east(const BitBoard bits) noexcept { return (bits & no_h_file) << 1; }
    [[nodiscard]] constexpr BitBoard shift_northwest(const BitBoard bits) noexcept { return (bits & no_a_file) >> 9; }
    [[nodiscard]] constexpr BitBoard shift_northeast(const BitBoard bits) noexcept { return (bits & no_h_file) >> 7; }
    [[nodiscard]] constexpr BitBoard shift_southwest(const BitBoard bits) noexcept { return (bits & no_a_file) << 7; }
    [[nodiscard]] constexpr BitBoard shift_southeast(const BitBoard bits) noexcept { return (bits & no_h_file) << 9; }

    [[nodiscard]] constexpr BitBoard mirror_main_diagonal(BitBoard bits) noexcept
    {
        std::uint64_t a = (bits ^ (bits >> 7)) & 0x00aa00aa00aa00aaull;
        bits = bits ^ a ^ (a << 7);
        a = (bits ^ (bits >> 14)) & 0x0000cccc0000ccccull;
        bits = bits ^ a ^ (a << 14);
        a = (bits ^ (bits >> 28)) & 0x00000000f0f0f0f0ull;
        return bits ^ a ^ (a << 28);
    }

    [[nodiscard]] constexpr BitBoard mirror_anti_diagonal(BitBoard bits) noexcept
    {
        uint64_t a = (bits ^ (bits >> 9)) & 0x0055005500550055ull;
        bits = bits ^ a ^ (a << 9);
        a = (bits ^ (bits >> 18)) & 0x0000333300003333ull;
        bits = bits ^ a ^ (a << 18);
        a = (bits ^ (bits >> 36)) & 0x000000000f0f0f0full;
        return bits ^ a ^ (a << 36);
    }

    [[nodiscard]] constexpr BitBoard mirror_vertical(BitBoard bits) noexcept
    {
        bits = ((bits >> 8) & 0x00ff00ff00ff00ffull) | ((bits << 8) & 0xff00ff00ff00ff00ull);
        bits = ((bits >> 16) & 0x0000ffff0000ffffull) | ((bits << 16) & 0xffff0000ffff0000ull);
        return ((bits >> 32) & 0x00000000ffffffffull) | ((bits << 32) & 0xffffffff00000000ull);
    }

    [[nodiscard]] constexpr BitBoard mirror_horizontal(BitBoard bits) noexcept
    {
        bits = ((bits >> 1) & 0x5555555555555555ull) | ((bits << 1) & 0xaaaaaaaaaaaaaaaaull);
        bits = ((bits >> 2) & 0x3333333333333333ull) | ((bits << 2) & 0xCCCCCCCCCCCCCCCCull);
        return ((bits >> 4) & 0x0f0f0f0f0f0f0f0full) | ((bits << 4) & 0xf0f0f0f0f0f0f0f0ull);
    }

    [[nodiscard]] constexpr BitBoard rotate_180(BitBoard bits) noexcept
    {
        bits = ((bits & 0x5555555555555555ull) << 1) | ((bits & 0xaaaaaaaaaaaaaaaaull) >> 1);
        bits = ((bits & 0x3333333333333333ull) << 2) | ((bits & 0xccccccccccccccccull) >> 2);
        bits = ((bits & 0x0f0f0f0f0f0f0f0full) << 4) | ((bits & 0xf0f0f0f0f0f0f0f0ull) >> 4);
        bits = ((bits & 0x00ff00ff00ff00ffull) << 8) | ((bits & 0xff00ff00ff00ff00ull) >> 8);
        bits = ((bits & 0x0000ffff0000ffffull) << 16) | ((bits & 0xffff0000ffff0000ull) >> 16);
        return ((bits & 0x00000000ffffffffull) << 32) | ((bits & 0xffffffff00000000ull) >> 32);
    }

    [[nodiscard]] constexpr BitBoard rotate_90_cw(const BitBoard bits) noexcept { return mirror_horizontal(mirror_main_diagonal(bits)); }
    [[nodiscard]] constexpr BitBoard rotate_90_ccw(const BitBoard bits) noexcept { return mirror_horizontal(mirror_anti_diagonal(bits)); }
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
