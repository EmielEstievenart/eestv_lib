#include "eestv/timestamp/timestamp_parser.hpp"

namespace eestv
{

namespace
{

char to_lower_ascii(char character)
{
    if (character >= 'A' && character <= 'Z')
    {
        return static_cast<char>(character - 'A' + 'a');
    }

    return character;
}

} // namespace

compiledDataAndTimeParser TimestampParser::CompileFormat(const std::string& format)
{
    compiledDataAndTimeParser parser;
    const int size = static_cast<int>(format.size());
    bool has_year  = false;
    bool has_month = false;
    bool has_day   = false;

    for (int index = 0; index < size; ++index)
    {
        const char token = format[index];

        if (token == 'Y')
        {
            const bool has_double_year = index + 1 < size && format[index + 1] == 'Y';
            const bool has_quad_year   = has_double_year && index + 3 < size && format[index + 2] == 'Y' && format[index + 3] == 'Y';

            if (has_quad_year)
            {
                parser.dateParser.push_back(make_four_digit_year_parser());
                has_year = true;
                index += 3;
                continue;
            }

            if (has_double_year)
            {
                parser.dateParser.push_back(make_two_digit_year_parser());
                has_year = true;
                index += 1;
                continue;
            }
        }

        if (token == 'M')
        {
            const bool has_triple_month = index + 2 < size && format[index + 1] == 'M' && format[index + 2] == 'M';
            const bool has_double_month = index + 1 < size && format[index + 1] == 'M';

            if (has_triple_month)
            {
                parser.dateParser.push_back(make_month_name_parser());
                has_month = true;
                index += 2;
                continue;
            }

            if (has_double_month)
            {
                parser.dateParser.push_back(make_two_digit_field_parser(&DateAndTime::month, 1, 12));
                has_month = true;
                index += 1;
                continue;
            }
        }

        if (token == 'D' && index + 1 < size && format[index + 1] == 'D')
        {
            parser.dateParser.push_back(make_two_digit_field_parser(&DateAndTime::day, 1, 31));
            has_day = true;
            index += 1;
            continue;
        }

        if ((token == 'h' || token == 'H') && index + 1 < size && format[index + 1] == token)
        {
            parser.dateParser.push_back(make_two_digit_field_parser(&DateAndTime::hour, 0, 23));
            index += 1;
            continue;
        }

        if (token == 'm' && index + 1 < size && format[index + 1] == 'm')
        {
            parser.dateParser.push_back(make_two_digit_field_parser(&DateAndTime::minute, 0, 59));
            index += 1;
            continue;
        }

        if (token == 's' && index + 1 < size && format[index + 1] == 's')
        {
            parser.dateParser.push_back(make_second_parser());
            index += 1;
            continue;
        }

        if (token == 'f')
        {
            unsigned digits = 1;

            while (index + static_cast<int>(digits) < size && format[index + static_cast<int>(digits)] == 'f')
            {
                ++digits;
            }

            parser.dateParser.push_back(make_fraction_parser(digits));
            index += static_cast<int>(digits) - 1;
            continue;
        }

        if (token == 'Z')
        {
            const bool has_triple_z = index + 2 < size && format[index + 1] == 'Z' && format[index + 2] == 'Z';
            const bool has_double_z = index + 1 < size && format[index + 1] == 'Z';

            if (has_triple_z)
            {
                parser.dateParser.push_back(make_utc_offset_parser(true));
                index += 2;
                continue;
            }

            if (has_double_z)
            {
                parser.dateParser.push_back(make_utc_offset_parser(false));
                index += 1;
                continue;
            }

            parser.dateParser.push_back(make_utc_designator_parser());
            continue;
        }

        parser.dateParser.push_back(make_separator_parser(token));
    }

    if (has_month || has_day)
    {
        parser.dateParser.push_back(make_date_validator(has_year));
    }

    return parser;
}

bool TimestampParser::is_separator(char tested)
{
    switch (tested)
    {
    case 'Y':
    case 'M':
    case 'D':
    case 'h':
    case 'H':
    case 'm':
    case 's':
    case 'f':
    case 'Z':
        return false;
    default:
        return true;
    }
}

std::vector<int> TimestampParser::possible_parse_start_indices(const std::string& to_parse)
{
    std::vector<int> indices {0};
    bool in_whitespace = false;

    for (int index = 0; index < static_cast<int>(to_parse.size()); ++index)
    {
        const bool is_whitespace = to_parse[index] == ' ' || to_parse[index] == '\t';

        if (!in_whitespace && is_whitespace)
        {
            in_whitespace = true;
            continue;
        }

        if (in_whitespace && !is_whitespace)
        {
            indices.push_back(index);
            in_whitespace = false;
        }
    }

    return indices;
}

bool TimestampParser::is_digit(char tested)
{
    return tested >= '0' && tested <= '9';
}

bool TimestampParser::try_parse_two_digits(const std::string& to_parse, int index, unsigned& value)
{
    if (index < 0 || index + 1 >= static_cast<int>(to_parse.size()))
    {
        return false;
    }

    const char first  = to_parse[index];
    const char second = to_parse[index + 1];

    if (!is_digit(first) || !is_digit(second))
    {
        return false;
    }

    value = static_cast<unsigned>((first - '0') * 10 + (second - '0'));
    return true;
}

bool TimestampParser::try_parse_four_digits(const std::string& to_parse, int index, int& value)
{
    if (index < 0 || index + 3 >= static_cast<int>(to_parse.size()))
    {
        return false;
    }

    const char first  = to_parse[index];
    const char second = to_parse[index + 1];
    const char third  = to_parse[index + 2];
    const char fourth = to_parse[index + 3];

    if (!is_digit(first) || !is_digit(second) || !is_digit(third) || !is_digit(fourth))
    {
        return false;
    }

    value = (first - '0') * 1000 + (second - '0') * 100 + (third - '0') * 10 + (fourth - '0');
    return true;
}

DateAndTimeParserStep TimestampParser::make_separator_parser(char separator)
{
    return [separator](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        static_cast<void>(output);

        if (index < 0 || index >= static_cast<int>(to_parse.size()))
        {
            return false;
        }

        if (to_parse[index] != separator)
        {
            return false;
        }

        index_jump = 1;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_two_digit_year_parser()
{
    return [](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        unsigned year = 0;
        if (!TimestampParser::try_parse_two_digits(to_parse, index, year))
        {
            return false;
        }

        output.year = 2000 + static_cast<int>(year);
        index_jump  = 2;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_four_digit_year_parser()
{
    return [](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        int year = 0;
        if (!TimestampParser::try_parse_four_digits(to_parse, index, year))
        {
            return false;
        }

        output.year = year;
        index_jump  = 4;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_two_digit_field_parser(unsigned DateAndTime::* field, unsigned min_value, unsigned max_value)
{
    return [field, min_value, max_value](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        unsigned value = 0;
        if (!TimestampParser::try_parse_two_digits(to_parse, index, value))
        {
            return false;
        }

        if (value < min_value || value > max_value)
        {
            return false;
        }

        output.*field = value;
        index_jump    = 2;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_month_name_parser()
{
    return [](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        if (index < 0 || index + 2 >= static_cast<int>(to_parse.size()))
        {
            return false;
        }

        const char first  = to_lower_ascii(to_parse[index]);
        const char second = to_lower_ascii(to_parse[index + 1]);
        const char third  = to_lower_ascii(to_parse[index + 2]);

        unsigned month = 0;

        if (first == 'j' && second == 'a' && third == 'n')
            month = 1;
        else if (first == 'f' && second == 'e' && third == 'b')
            month = 2;
        else if (first == 'm' && second == 'a' && third == 'r')
            month = 3;
        else if (first == 'a' && second == 'p' && third == 'r')
            month = 4;
        else if (first == 'm' && second == 'a' && third == 'y')
            month = 5;
        else if (first == 'j' && second == 'u' && third == 'n')
            month = 6;
        else if (first == 'j' && second == 'u' && third == 'l')
            month = 7;
        else if (first == 'a' && second == 'u' && third == 'g')
            month = 8;
        else if (first == 's' && second == 'e' && third == 'p')
            month = 9;
        else if (first == 'o' && second == 'c' && third == 't')
            month = 10;
        else if (first == 'n' && second == 'o' && third == 'v')
            month = 11;
        else if (first == 'd' && second == 'e' && third == 'c')
            month = 12;
        else
            return false;

        output.month = month;
        index_jump   = 3;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_second_parser()
{
    return [](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        unsigned second = 0;
        if (!TimestampParser::try_parse_two_digits(to_parse, index, second))
        {
            return false;
        }

        if (second > 60)
        {
            return false;
        }

        output.second      = second;
        output.leap_second = second == 60;
        index_jump         = 2;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_fraction_parser(unsigned digits)
{
    return [digits](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        if (digits == 0 || digits > 9)
        {
            return false;
        }

        if (index < 0 || index + static_cast<int>(digits) > static_cast<int>(to_parse.size()))
        {
            return false;
        }

        unsigned value = 0;

        for (unsigned digit_index = 0; digit_index < digits; ++digit_index)
        {
            const char digit = to_parse[index + static_cast<int>(digit_index)];
            if (!TimestampParser::is_digit(digit))
            {
                return false;
            }

            value = value * 10 + static_cast<unsigned>(digit - '0');
        }

        for (unsigned scale_digits = digits; scale_digits < 9; ++scale_digits)
        {
            value *= 10;
        }

        output.nanosecond = value;
        index_jump        = static_cast<int>(digits);
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_utc_designator_parser()
{
    return [](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        if (index < 0 || index >= static_cast<int>(to_parse.size()))
        {
            return false;
        }

        if (to_parse[index] != 'Z')
        {
            return false;
        }

        output.utc_offset_minutes = 0;
        index_jump                = 1;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_utc_offset_parser(bool with_colon)
{
    return [with_colon](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        if (index < 0 || index >= static_cast<int>(to_parse.size()))
        {
            return false;
        }

        const char sign = to_parse[index];
        if (sign != '+' && sign != '-')
        {
            return false;
        }

        unsigned hours   = 0;
        unsigned minutes = 0;

        if (!TimestampParser::try_parse_two_digits(to_parse, index + 1, hours))
        {
            return false;
        }

        int consumed = 3;

        if (with_colon)
        {
            if (index + 3 >= static_cast<int>(to_parse.size()) || to_parse[index + 3] != ':')
            {
                return false;
            }

            if (!TimestampParser::try_parse_two_digits(to_parse, index + 4, minutes))
            {
                return false;
            }

            consumed = 6;
        }
        else
        {
            if (!TimestampParser::try_parse_two_digits(to_parse, index + 3, minutes))
            {
                return false;
            }

            consumed = 5;
        }

        if (hours > 23 || minutes > 59)
        {
            return false;
        }

        int total_minutes = static_cast<int>(hours) * 60 + static_cast<int>(minutes);
        if (sign == '-')
        {
            total_minutes = -total_minutes;
        }

        output.utc_offset_minutes = total_minutes;
        index_jump                = consumed;
        return true;
    };
}

DateAndTimeParserStep TimestampParser::make_date_validator(bool has_year)
{
    return [has_year](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
    {
        static_cast<void>(to_parse);
        static_cast<void>(index);

        if (output.month == 0 || output.day == 0)
        {
            index_jump = 0;
            return true;
        }

        const bool leap_year   = has_year && TimestampParser::is_leap_year(output.year);
        const unsigned max_day = TimestampParser::max_day_in_month(output.month, leap_year);

        if (max_day == 0 || output.day > max_day)
        {
            return false;
        }

        index_jump = 0;
        return true;
    };
}

bool TimestampParser::is_leap_year(int year)
{
    if (year % 400 == 0)
    {
        return true;
    }

    if (year % 100 == 0)
    {
        return false;
    }

    return year % 4 == 0;
}

unsigned TimestampParser::max_day_in_month(unsigned month, bool leap_year)
{
    switch (month)
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return leap_year ? 29U : 28U;
    default:
        return 0;
    }
}

} // namespace eestv
