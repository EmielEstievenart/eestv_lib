#include "eestv/timestamp/timestamp_parser.hpp"

namespace eestv
{

compiledDataAndTimeParser TimestampParser::CompileFormat(const std::string& format)
{
    compiledDataAndTimeParser parser;
    const int size = static_cast<int>(format.size());

    for (int index = 0; index < size; ++index)
    {
        const char token = format[index];

        if (is_separator(token))
        {
            parser.dateParser.push_back(make_separator_parser(token));
            continue;
        }

        if (token == 'Y')
        {
            const bool has_double_year = index + 1 < size && format[index + 1] == 'Y';
            const bool has_quad_year   = has_double_year && index + 3 < size && format[index + 2] == 'Y' && format[index + 3] == 'Y';

            if (has_quad_year)
            {
                parser.dateParser.push_back(make_four_digit_year_parser());
                index += 3;
            }
            else if (has_double_year)
            {
                parser.dateParser.push_back(make_two_digit_year_parser());
                index += 1;
            }

            continue;
        }

        if (token == 'M' && index + 1 < size && format[index + 1] == 'M')
        {
            parser.dateParser.push_back(make_two_digit_field_parser(&DateAndTime::month, 1, 12));
            index += 1;
            continue;
        }

        if (token == 'D' && index + 1 < size && format[index + 1] == 'D')
        {
            parser.dateParser.push_back(make_two_digit_field_parser(&DateAndTime::day, 1, 31));
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
        }
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
        return false;
    default:
        return true;
    }
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

} // namespace eestv
