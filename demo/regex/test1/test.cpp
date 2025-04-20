#include <iostream>
#include <regex>
#include <string>

int main()
{
    std::string str = "GET1248";
    std::regex re("(GET)(\\d+)");
    std::smatch matches;
    if (std::regex_match(str, matches, re))
    {
        for (auto s : matches)

        // 输出捕获组结果（请求方法）
        {
            std::cout << s << std::endl;
        }
    }
    else
    {
        std::cout << "匹配失败" << std::endl;
    }
    return 0;
}