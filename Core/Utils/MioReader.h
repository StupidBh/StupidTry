#pragma once
#include <string_view>
#include <string>
#include <vector>

#include <cmath>
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
            while (ptr < end && static_cast<unsigned char>(*ptr) <= 32) {
                ++ptr;
            }
            if (ptr >= end) {
                break;
            }

            _Ty val {};
            auto [p, ec] = std::from_chars(ptr, end, val);
            if (ec == std::errc {}) {
                if (p < end && (*p == '-' || *p == '+') && std::isdigit(static_cast<unsigned char>(*(p - 1)))) {
                    int exponent = 0;
                    auto [p_exp, ec_exp] = std::from_chars(p, end, exponent);

                    if (ec_exp == std::errc {}) {
                        val *= static_cast<_Ty>(std::pow(10.0, exponent));
                        out.emplace_back(val);
                        ptr = p_exp;
                    }
                    else {
                        while (ptr < end && !std::isspace(static_cast<unsigned char>(*ptr))) {
                            ++ptr;
                        }
                    }
                }
                else {
                    out.emplace_back(val);
                    ptr = p;
                }
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
