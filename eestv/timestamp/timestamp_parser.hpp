#pragma once

#include <string>
#include <vector>
#include <functional>

namespace eestv
{
struct DateAndTime
{
    int year {};
    unsigned month {};
    unsigned day {};

    unsigned hour {};
    unsigned minute {};
    unsigned second {};
    unsigned nanosecond {};

    bool has_timezone_offset {false};
    int timezone_offset_minutes {};

    bool leap_second {false};
};

struct compiledDataAndTimeParser
{
    std::vector<std::function<bool(std::string& to_parse, int index, int& index_jump, DateAndTime& output)>> dateParser;
};

class TimestampParser
{
public:
    compiledDataAndTimeParser CompileFormat(const std::string& format)
    {
        compiledDataAndTimeParser parser;
        size_t size = format.size();
        int index   = 0;

        while (index < static_cast<int>(size))
        {
            if (is_separator(format[index]))
            {
                const char found_separator = format[index];
                parser.dateParser.push_back(
                    [found_separator](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
                    {
                        if (to_parse[index] == found_separator)
                        {
                            index_jump = 1;
                            return true;
                        }
                        return false;
                    });
            }
            else if (format[index] == 'Y')
            {
                bool detect_double_year = index + 1 < static_cast<int>(size) && format[index + 1] == 'Y';
                bool detect_quad_year   = false;

                if (detect_double_year && index + 3 < static_cast<int>(size) && format[index + 2] == 'Y' && format[index + 3] == 'Y')
                {
                    detect_double_year = false;
                    detect_quad_year   = true;
                }

                if (detect_double_year)
                {
                    parser.dateParser.push_back(
                        [](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
                        {
                            if (index < 0 || index + 1 >= static_cast<int>(to_parse.size()))
                            {
                                return false;
                            }

                            const char first  = to_parse[index];
                            const char second = to_parse[index + 1];

                            if (first < '0' || first > '9' || second < '0' || second > '9')
                            {
                                return false;
                            }

                            output.year = 2000 + (first - '0') * 10 + (second - '0');
                            index_jump  = 2;
                            return true;
                        });
                    index += 1;
                }
                else if (detect_quad_year)
                {
                    parser.dateParser.push_back(
                        [detect_double_year, detect_quad_year](std::string& to_parse, int index, int& index_jump, DateAndTime& output)
                        {
                            static_cast<void>(detect_double_year);
                            static_cast<void>(detect_quad_year);

                            if (index < 0 || index + 3 >= static_cast<int>(to_parse.size()))
                            {
                                return false;
                            }

                            const char first  = to_parse[index];
                            const char second = to_parse[index + 1];
                            const char third  = to_parse[index + 2];
                            const char fourth = to_parse[index + 3];

                            if (first < '0' || first > '9' || second < '0' || second > '9' || third < '0' || third > '9' || fourth < '0' ||
                                fourth > '9')
                            {
                                return false;
                            }

                            output.year = (first - '0') * 1000 + (second - '0') * 100 + (third - '0') * 10 + (fourth - '0');
                            index_jump  = 4;
                            return true;
                        });
                    index += 3;
                }
            }
            index++;
        }

        return parser;
    }

    static bool is_separator(char tested)
    {
        switch (tested)
        {
        case '[':
        case '"':
        case '\'':
        case '(':
        case '{':
        case '<':
        case '-':
        case '.':
        case ',':
            return true;
        default:
            return false;
        }
    }
};
} //namespace eestv
