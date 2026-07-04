#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eestv
{
struct DateAndTime
{
    int year {0};
    unsigned month {0};
    unsigned day {0};

    unsigned hour {0};
    unsigned minute {0};
    unsigned second {0};
    unsigned nanosecond {0};

    std::optional<int> utc_offset_minutes;

    bool leap_second {false};

    /// Converts the fields to a system_clock time point.
    ///
    /// When utc_offset_minutes is set, the fields are interpreted as a civil time at that
    /// offset from UTC. When it is not set, the fields are interpreted in the machine's
    /// local time zone (std::mktime); callers that need a host-independent convention must
    /// convert the fields themselves. Note that time_t(-1) (1969-12-31 23:59:59 UTC) is
    /// indistinguishable from a conversion failure and is reported as std::nullopt.
    std::optional<std::chrono::system_clock::time_point> to_time_point() const;
};

/// One compiled step of a timestamp format. A step inspects `to_parse` starting at `index`
/// without modifying it, and on success reports the number of consumed characters through
/// `index_jump` (which may be zero for pure validation steps).
using DateAndTimeParserStep = std::function<bool(std::string_view to_parse, int index, int& index_jump, DateAndTime& output)>;

/// A timestamp format compiled to an executable sequence of parser steps.
///
/// When the format string is invalid, `error` describes the problem and `steps` is empty,
/// so the parser matches nothing.
struct CompiledDateAndTimeParser
{
    std::vector<DateAndTimeParserStep> steps;
    std::string error;

    bool valid() const { return error.empty(); }
};

/// Deprecated spelling, kept so existing code keeps compiling.
using compiledDataAndTimeParser = CompiledDateAndTimeParser;

/// Compiles timestamp format strings into parser step sequences.
///
/// Format tokens:
///   YYYY  four-digit year              YY   two-digit year (interpreted as 2000-2099)
///   MM    two-digit month (01-12)      MMM  English month name (Jan..Dec, any case)
///   DD    two-digit day (01-31)        D*   one- or two-digit day, optionally space-padded ("03", " 3" or "3")
///   hh    two-digit hour (00-23)       HH   same as hh
///   mm    two-digit minute (00-59)
///   ss    two-digit second (00-60, 60 flags a leap second)
///   s*    variable-length seconds (unbounded, 60 flags a leap second)
///   f     fraction digit; repeat for more digits (max 9, e.g. fff = milliseconds)
///   f*    variable-length fraction (1 or more digits; digits beyond 9 are consumed but ignored)
///   Z     UTC designator, matches 'Z' or 'z'
///   ZZ    UTC offset +-hhmm            ZZZ  UTC offset +-hh:mm
///
/// Every other character is a literal separator that must match exactly. A reserved
/// character that does not form a complete token (for example a lone 'Y' or a run of ten
/// 'f's) makes the whole format invalid, reported through CompiledDateAndTimeParser::error.
class TimestampParser
{
public:
    static CompiledDateAndTimeParser CompileFormat(const std::string& format);

    static bool is_separator(char tested);

    /// Candidate timestamp start offsets in a log line: offset 0 plus the start of every
    /// whitespace-delimited token, optionally capped at max_indices entries.
    static std::vector<int> possible_parse_start_indices(std::string_view to_parse);
    static std::vector<int> possible_parse_start_indices(std::string_view to_parse, int max_indices);

    /// Returns the nth (0-based) candidate start offset without allocating, or -1 when the
    /// line has fewer candidates.
    static int nth_parse_start_index(std::string_view to_parse, int n);

private:
    static bool is_digit(char tested);
    static bool try_parse_two_digits(std::string_view to_parse, int index, unsigned& value);
    static bool try_parse_four_digits(std::string_view to_parse, int index, int& value);

    static DateAndTimeParserStep make_separator_parser(char separator);
    static DateAndTimeParserStep make_two_digit_year_parser();
    static DateAndTimeParserStep make_four_digit_year_parser();
    static DateAndTimeParserStep make_two_digit_field_parser(unsigned DateAndTime::* field, unsigned min_value, unsigned max_value);
    static DateAndTimeParserStep make_padded_day_parser();
    static DateAndTimeParserStep make_month_name_parser();
    static DateAndTimeParserStep make_second_parser();
    static DateAndTimeParserStep make_variable_second_parser();
    static DateAndTimeParserStep make_fraction_parser(unsigned digits);
    static DateAndTimeParserStep make_variable_fraction_parser();
    static DateAndTimeParserStep make_utc_designator_parser();
    static DateAndTimeParserStep make_utc_offset_parser(bool with_colon);
    static DateAndTimeParserStep make_date_validator(bool has_year);

    static bool is_leap_year(int year);
    static unsigned max_day_in_month(unsigned month, bool leap_year);
};
} //namespace eestv
