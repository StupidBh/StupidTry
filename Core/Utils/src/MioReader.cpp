#include "MioReader.h"

#include <cstring>
#include <stdexcept>

MioReader::MioReader(const std::string& filename) :
    m_mmap(filename),
    m_pos(0)
{
    if (!this->m_mmap.is_open()) {
        throw std::runtime_error("Open fail: " + filename);
    }

    this->m_data = this->m_mmap.data();
    this->m_size = this->m_mmap.size();
}

bool MioReader::GetLine(std::string_view& line)
{
    if (this->m_pos >= this->m_size) {
        return false;
    }

    const size_t line_start = this->m_pos;
    while (this->m_pos < this->m_size && this->m_data[this->m_pos] != '\n' && this->m_data[this->m_pos] != '\r') {
        ++this->m_pos;
    }

    const size_t line_end = this->m_pos;

    // 如果是 \r\n，跳过两个字符
    if (this->m_pos < this->m_size && this->m_data[this->m_pos] == '\r') {
        ++this->m_pos;
        if (this->m_pos < this->m_size && this->m_data[this->m_pos] == '\n') {
            ++this->m_pos;
        }
    }
    else if (this->m_pos < this->m_size && this->m_data[this->m_pos] == '\n') {
        ++this->m_pos;
    }

    line = std::string_view(&this->m_data[line_start], line_end - line_start);
    return true;
}

size_t MioReader::GetLineBatch(std::vector<std::string_view>& lines, const size_t max_lines)
{
    lines.clear();
    lines.reserve(max_lines);

    if (this->m_pos >= this->m_size || max_lines == 0) {
        return 0;
    }

    size_t count = 0;
    const char* cur = this->m_data + this->m_pos;
    const char* end = this->m_data + this->m_size;
    auto next_lf = this->m_next_lf;

    while (cur < end && count < max_lines) {
        const auto position = static_cast<size_t>(cur - this->m_data);
        if (!next_lf || *next_lf < position) {
            const auto nl = static_cast<const char*>(std::memchr(cur, '\n', end - cur));
            next_lf = nl != nullptr ? static_cast<size_t>(nl - this->m_data) : this->m_size;
        }

        // Reuse the next LF and search for CR only before that position.
        const auto cr = static_cast<const char*>(std::memchr(cur, '\r', *next_lf - position));
        const char* eol = cr != nullptr ? cr : this->m_data + *next_lf;
        lines.emplace_back(cur, eol - cur);
        ++count;

        cur = eol;
        if (cur < end) {
            ++cur;
            if (*eol == '\r' && cur < end && *cur == '\n') {
                ++cur;
            }
        }
    }

    this->m_pos = static_cast<size_t>(cur - this->m_data);
    this->m_next_lf = next_lf;
    return count;
}
