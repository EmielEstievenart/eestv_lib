#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
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

    std::optional<std::chrono::system_clock::time_point> to_time_point() const;
};

using DateAndTimeParserStep = std::function<bool(std::string& to_parse, int index, int& index_jump, DateAndTime& output)>;

struct compiledDataAndTimeParser
{
    std::vector<DateAndTimeParserStep> dateParser;
};

class TimestampParser
{
public:
    compiledDataAndTimeParser CompileFormat(const std::string& format);

    static bool is_separator(char tested);
    static std::vector<int> possible_parse_start_indices(const std::string& to_parse);
    static std::vector<int> possible_parse_start_indices(const std::string& to_parse, int max_indices);

private:
    static bool is_digit(char tested);
    static bool try_parse_two_digits(const std::string& to_parse, int index, unsigned& value);
    static bool try_parse_four_digits(const std::string& to_parse, int index, int& value);

    static DateAndTimeParserStep make_separator_parser(char separator);
    static DateAndTimeParserStep make_two_digit_year_parser();
    static DateAndTimeParserStep make_four_digit_year_parser();
    static DateAndTimeParserStep make_two_digit_field_parser(unsigned DateAndTime::* field, unsigned min_value, unsigned max_value);
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
