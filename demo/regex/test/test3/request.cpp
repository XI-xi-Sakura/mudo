#include <iostream>
#include <string>
#include <regex>

void req_line()
{
    std::cout << "---------------first line start---------------\n";
    // std::string str = "GET /bitejiuyeke HTTP/1.1\r\n";
    // std::string str = "GET /bitejiuyeke HTTP/1.1\n";
    std::string str = "GET /bitejiuyeke?a=b&c=d HTTP/1.1\r\n";
    std::regex re("(GET|HEAD|POST|PUT|DELETE) (([^?]+)(?:\\?(.*))?) (HTTP/1\\.[01])(?:\\r?\\n|\\n)");
    std::smatch matches;
    std::regex_match(str, matches, re);
    /*正则匹配获取完毕之后matches中的存储情况*/
    /*  matches[0]    整体首行    GET /bitejiuyeke?a=b&c=d HTTP/1.1
        matches[1]    请求方法    GET
        matches[2]    整体URL     /bitejiuyeke?a=b&c=d
        matches[3]    ?之前       /bitejiuyeke
        matches[4]    查询字符串  a=b&c=d
        matches[5]    协议版本    HTTP/1.1  */
    int i = 0;
    for (const auto &it : matches)
    {
        std::cout << i++ << ": ";
        std::cout << it << std::endl;
    }
    if (matches[4].length() > 0)
    {
        std::cout << "have param!\n";
    }
    else
    {
        std::cout << "have not param!\n";
    }
    std::cout << "---------------first line start---------------\n";
    return;
}

void method_match(const std::string str)
{
    std::cout << "---------------method start---------------\n";
    std::regex re("(GET|HEAD|POST|PUT|DELETE).*");
    /* () 表示捕捉符合括号内格式的数据
     *  GET|HEAD|POST...  |表示或，也就是匹配这几个字符串中的任意一个
     * .* 中.表示匹配除换行外的任意单字符，*表示匹配前边的字符任意次； 合起来在这里就是表示空格后匹配任意字符
     *  最终合并起来表示匹配以GET或者POST或者PUT...几个字符串开始，然后后边有个空格的字符串，并在匹配成功后捕捉前边的请求方法字符串
     */
    std::smatch matches;
    std::regex_match(str, matches, re);
    std::cout << matches[0] << std::endl;
    std::cout << matches[1] << std::endl;
    std::cout << "---------------method over---------------\n";
}

void path_match(const std::string str)
{
    // std::regex re("(([^?]+)(?:\\?(.*))?)");
    std::cout << "---------------path start---------------\n";
    std::regex re("([^?]+).*");
    /* 最外层的() 表示捕捉提取括号内指定格式的内容
     * ([^?]+)  [^xyz 负值匹配集合，指匹配^之后的字符， 比如[^abc] 则plain就匹配到plin字符
     * +匹配前面的子表达式一次或多次
     * 合并起来就是匹配非?字符一次或多次
     */
    std::smatch matches;
    std::regex_match(str, matches, re);
    std::cout << matches[0] << std::endl;
    std::cout << matches[1] << std::endl;
    std::cout << "---------------path over---------------\n";
}

void query_match(const std::string str)
{
    std::cout << "---------------query start---------------\n";
    std::regex re("(?:\\?(.*)?)? .*");
    /*
     * (?:\\?(.*)?)   最后的?表示匹配前边的表达式0次或1次，因为有的请求可能没有查询字符串
     * (?:\\?(.*))    (?:pattern)表示匹配pattern但是不获取匹配结果
     *  \\?(.*) \\?表示原始的?字符，这里表示以?字符作为起始
     *          .表示\n之外任意字符，
     *          *表示匹配前边的字符0次或多次，
     * ?跟在*或+之后表示懒惰模式， 也就是说以?开始的字符串就只匹配这一次就行，后边还有以?开始的同格式字符串也不会匹配
     * () 表示捕捉获取符合内部格式的数据
     * 合并起来表示的就是，匹配以?开始的字符串，但是字符串整体不要，
     * 只捕捉获取?之后的字符串,且只匹配一次，就算后边还有以?开始的同格式字符串也不会匹配
     */
    std::smatch matches;
    std::regex_match(str, matches, re);
    std::cout << matches[0] << std::endl;
    std::cout << matches[1] << std::endl;
    std::cout << "---------------query over---------------\n";
}

void version_mathch(const std::string str)
{
    std::cout << "---------------version start---------------\n";
    std::regex re("(HTTP/1\\.[01])(?:\\r?\\n|\\n)");
    /*
     * (HTTP/1\\.[01])  外层的括号表示捕捉字符串
     * HTTP/1  表示以HTTP/1开始的字符串
     * \\.    表示匹配. 原始字符
     * [01]   表示匹配0字符或者1字符
     * (?:\\r?\\n|\\n) 表示匹配一个\r\n或者\n字符，但是并不捕捉这个内容
     * 合并起来就是匹配以HTTP/1.开始，后边跟了一个0或1的字符，且最终以\n或者\r\n作为结尾的字符串
     */
    std::smatch matches;
    std::regex_match(str, matches, re);
    std::cout << matches[0] << std::endl;
    std::cout << matches[1] << std::endl;
    std::cout << "---------------version over---------------\n";
}

int main()
{
    req_line();
    method_match("GET /s");
    path_match("/search?name=bitejiuyeke ");
    query_match("?name=xiaoming&age=19 HTTP/1.1");
    version_mathch("HTTP/1.1\r\n");
    return 0;
}