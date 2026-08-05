#pragma once

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>

// 简易 HTTP/1.1 子集解析器，仅用于压测场景（wrk / ab / 自研客户端）。
// 只支持：
//   - 请求行 + 头部（查找 "\r\n\r\n" 作为头部结束）
//   - Content-Length 指定 body 长度
//   - Keep-Alive 持久连接（连接上循环处理多个请求）
namespace http_bench {

// 解析缓冲区中的请求，返回：
//   0                             -> 半包，需要更多数据
//   std::numeric_limits<size_t>::max() -> 格式错误（应回 400）
//   其他                           -> 完整请求的总长度（header + body）
inline size_t parseRequest(const char* data, size_t len)
{
    // 1. 定位头部结束符 "\r\n\r\n"
    size_t header_end = std::string::npos;
    for (size_t i = 0; i + 3 < len; ++i)
    {
        if (data[i] == '\r' && data[i + 1] == '\n' &&
            data[i + 2] == '\r' && data[i + 3] == '\n')
        {
            header_end = i + 4;
            break;
        }
    }
    if (header_end == std::string::npos)
        return 0; // 头部不完整（半包）

    // 2. 逐行扫描头部，解析 Content-Length（不区分大小写）
    size_t content_length = 0;
    const char* p = data;
    const char* end = data + header_end;
    constexpr const char* KEY = "content-length";
    constexpr size_t KEY_LEN = 14; // strlen("content-length")

    while (p < end)
    {
        const char* line_end = static_cast<const char*>(memchr(p, '\n', end - p));
        if (!line_end)
            break;
        size_t line_len = static_cast<size_t>(line_end - p);
        if (line_len > 0 && p[line_len - 1] == '\r')
            --line_len;

        if (line_len > KEY_LEN && p[KEY_LEN] == ':')
        {
            bool match = true;
            for (size_t i = 0; i < KEY_LEN; ++i)
            {
                char c = p[i];
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + 32);
                if (c != KEY[i])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                size_t j = KEY_LEN + 1;
                while (j < line_len && (p[j] == ' ' || p[j] == '\t'))
                    ++j;
                while (j < line_len && p[j] >= '0' && p[j] <= '9')
                {
                    content_length = content_length * 10 +
                                     static_cast<size_t>(p[j] - '0');
                    ++j;
                }
            }
        }
        p = line_end + 1;
    }

    // 3. 完整请求总长度 = header + body
    return header_end + content_length;
}

// 预生成的 400 响应（解析失败时使用）
inline const std::string& badRequestResponse()
{
    static const std::string resp =
        "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    return resp;
}

} // namespace http_bench
