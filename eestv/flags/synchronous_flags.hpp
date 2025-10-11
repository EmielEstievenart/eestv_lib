#pragma once

#include <cstdint>
#include <type_traits>

namespace eestv
{

template <typename FlagType>
class SynchronousFlags
{
    static_assert(std::is_enum<FlagType>::value, "FlagType must be an enum");

public:
    SynchronousFlags() = default;

    /**
     * Set a specific flag bit
     * @param flag The flag to set (enum value will be converted to bit position)
     */
    void set_flag(FlagType flag) { _flags |= (1U << static_cast<uint32_t>(flag)); }

    /**
     * Clear a specific flag bit
     * @param flag The flag to clear (enum value will be converted to bit position)
     */
    void clear_flag(FlagType flag) { _flags &= ~(1U << static_cast<uint32_t>(flag)); }

    /**
     * Get the state of a specific flag bit
     * @param flag The flag to check (enum value will be converted to bit position)
     * @return true if the flag bit is set, false otherwise
     */
    bool get_flag(FlagType flag) const { return (_flags & (1U << static_cast<uint32_t>(flag))) != 0; }

    /**
     * Clear all flags
     */
    void clear_all() { _flags = 0; }

    /**
     * Get the raw flags value
     * @return The current flags as a uint32_t
     */
    uint32_t get_raw() const { return _flags; }

private:
    uint32_t _flags = 0;
};
} // namespace eestv
