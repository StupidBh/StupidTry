#pragma once
#include <string_view>
#include <string>
#include <vector>

#include <cctype>
#include <charconv>
#include <system_error>

#include "mio/mmap.hpp"

class MioReader {
public:
    explicit MioReader(const std::string& filename);

    bool getline(std::string_view& line);

    size_t getline_batch(std::vector<std::string_view>& lines, size_t max_lines = 10000);

    template<class _Ty>
    static _Ty parse_line(std::string_view line)
    {
        static std::vector<_Ty> value(1);
        parse_line<_Ty>(line, value);
        return value.back();
    }

    template<class _Ty>
    static void parse_line(std::string_view line, std::vector<_Ty>& out)
    {
        out.clear();
        const char* ptr = line.data();
        const char* end = ptr + line.size();

        while (ptr < end) {
            while (ptr < end && std::isspace(static_cast<unsigned char>(*ptr))) {
                ++ptr;
            }
            if (ptr >= end) {
                break;
            }

            _Ty value {};
            auto [p, ec] = std::from_chars(ptr, end, value);
            if (ec == std::errc {}) {
                out.emplace_back(value);
                ptr = p;
            }
            else {
                while (ptr < end && !std::isspace(static_cast<unsigned char>(*ptr))) {
                    ++ptr;
                }
            }
        }
    }

private:
    mio::mmap_source m_mmap;
    const char* m_data;
    size_t m_size;
    size_t m_pos;
};
