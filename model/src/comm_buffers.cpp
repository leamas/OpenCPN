#include "model/comm_buffers.h"

#include <algorithm>

void OutputBuffer::Put(const std::string line) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_buffer.push_back(line);
}

bool OutputBuffer::Get(std::string& line) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_buffer.size() == 0) {
    return false;
  }
  line = m_buffer[0];
  m_buffer.pop_front();
  return true;
}

void LineBuffer::Put(uint8_t ch) {
  if (m_last_ch == '\n' && ch == '\r') {
    // Replace possible <lf><cr> with <lf>
    m_last_ch = 0;
  } else if (m_last_ch == '\r' && ch == '\n') {
    // Replace possible <cr><lf> with <lf>
    if (m_buff.size() > 0) m_buff[m_buff.size() - 1] = '\n';
    m_last_ch = 0;
  } else {
    m_buff.push_back(ch);
    m_last_ch = ch;
  }
  if (ch == '\n') m_line_count += 1;
}

bool LineBuffer::HasLine() const { return m_line_count > 0; }

std::vector<uint8_t> LineBuffer::GetLine() {
  using namespace std;
  auto nl_pos = find(m_buff.begin(), m_buff.end(), '\n');
  if (nl_pos == m_buff.end()) {
    return vector<uint8_t>();
  }
  auto line = vector<uint8_t>(m_buff.begin(), nl_pos);
  m_buff = vector<uint8_t>(nl_pos + 1, m_buff.end());

  m_line_count -= 1;
  return line;
}
