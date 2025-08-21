#include "MioReader.h"

MioReader::MioReader(const std::string& filename) :
    m_mmap(filename),
    m_pos(0)
{
    if (!m_mmap.is_open()) {
        throw std::runtime_error("Open fail: " + filename);
    }

    m_data = m_mmap.data();
    m_size = m_mmap.size();
}

bool MioReader::getline(std::string_view& line)
{
    if (m_pos >= m_size) {
        return false;
    }

    size_t line_start = m_pos;
    while (m_pos < m_size && m_data[m_pos] != '\n' && m_data[m_pos] != '\r') {
        ++m_pos;
    }

    size_t line_end = m_pos;

    // 如果是 \r\n，跳过两个字符
    if (m_pos < m_size && m_data[m_pos] == '\r') {
        ++m_pos;
        if (m_pos < m_size && m_data[m_pos] == '\n') {
            ++m_pos;
        }
    }
    else if (m_pos < m_size && m_data[m_pos] == '\n') {
        ++m_pos;
    }

    line = std::string_view(&m_data[line_start], line_end - line_start);
    return true;
}

size_t MioReader::getline_batch(std::vector<std::string_view>& lines, size_t max_lines)
{
    lines.clear();
    lines.reserve(max_lines);

    size_t count = 0;
    const char* cur = m_data + m_pos;
    const char* end = m_data + m_size;
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

    m_pos = cur - m_data;
    return count;
}
