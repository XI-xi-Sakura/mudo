#include <iostream>
#include <string>
#include <any>

class Test
{
public:
    std::string _data;
    Test(const std::string &data) : _data(data) { std::cout << "构造" << _data << std::endl; }
    Test(const Test &other)
    {
        _data = other._data;
        std::cout << "拷贝" << _data << std::endl;
    }
    ~Test() { std::cout << "析构" << _data << std::endl; }
};

int main()
{
    {
        std::any a = 10;
        std::any b = 88.88;
        std::any c = std::string("bitejiuyeke");
        /*T* any_cast<class T>() 成员函数用于返回any对象值的地址 */
        int *aa = std::any_cast<int>(&a);
        std::cout << *aa << std::endl;
        std::cout << *std::any_cast<double>(&b) << std::endl;
        std::cout << *std::any_cast<std::string>(&c) << std::endl;
    }
    {
        Test d("Leihou");

        std::any any_d = d;
        std::any any_e(d);
        std::any any_f(any_d);
        std::any any_g = any_d;
    }
    {
        std::cout << "------------------any之间相互的赋值-就算any保存的类型不同也可以-----------------\n";
        std::any any_f;

        any_f = 33;
        std::cout << *std::any_cast<int>(&any_f) << std::endl;

        std::string c = "Hello World";

        any_f = c;
        std::cout << *std::any_cast<std::string>(&any_f) << std::endl;

        any_f = std::any(Test("test"));
        std::cout << std::any_cast<Test>(&any_f)->_data << std::endl;
    }

    return 0;
}