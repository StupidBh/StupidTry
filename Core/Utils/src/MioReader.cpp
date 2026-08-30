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

    size_t count = 0;
    const char* cur = this->m_data + this->m_pos;
    const char* end = this->m_data + this->m_size;
    const char* line_start = cur;

    while (cur < end && count < max_lines) {
        const char* nl = static_cast<const char*>(memchr(cur, '\n', end - cur));
        const char* cr = static_cast<const char*>(memchr(cur, '\r', end - cur));

        const char* eol = nullptr;
        if (!nl) {
            eol = cr;
        }
        else if (!cr) {
            eol = nl;
        }
        else {
            eol = (nl < cr) ? nl : cr;
        }

        if (!eol) {
            // 剩余部分没有换行符
            lines.emplace_back(line_start, end - line_start);
            ++count;
            cur = end;
            break;
        }

        // 生成一行
        lines.emplace_back(line_start, eol - line_start);
        ++count;

        // 跳过换行符
        if (*eol == '\r' && eol + 1 < end && *(eol + 1) == '\n') {
            cur = eol + 2;
        }
        else {
            cur = eol + 1;
        }
        line_start = cur;
    }

    this->m_pos = cur - this->m_data;
    return count;
}
