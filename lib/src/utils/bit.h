#pragma once

#include <bit>
#include <clu/macros.h>

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
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
