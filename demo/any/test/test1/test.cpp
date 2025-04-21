#include <iostream>
#include <string>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <unistd.h>

/*Any类主要是实现一个通用类型出来，在c++17和boost库中都有现成的可以使用，但是这里实现一下
了解其思想，这样也就避免了第三方库的使用了*/

/*首先Any类肯定不能是一个模板类，否则编译的时候 Any<int> a, Any<float>b,需要传类型作
为模板参数，也就是说在使用的时候就要确定其类型*/

/*这是行不通的，因为保存在Content中的协议上下文，我们在定义any对象的时候是不知道他们的协
议类型的，因此无法传递类型作为模板参数*/

/*因此考虑Any内部设计一个模板容器holder类，可以保存各种类型数据*/

/*而因为在Any类中无法定义这个holder对象或指针，因为any也不知道这个类要保存什么类型的数
据，因此无法传递类型参数*/

/*所以，定义一个基类placeholder，让holder继承于placeholder，而Any类保存父类指针即可*/
/*当需要保存数据时，则new一个带有模板参数的子类holder对象出来保存数据，然后让Any类中的父
类指针，指向这个子类对象就搞定了*/
class Any
{
public:
    Any() : _content(NULL) {}

    /*为了能够接收所有类型的对象，因此将构造函数定义为一个模板函数*/
    template <typename T>
    Any(const T &val) : _content(new holder<T>(val)) {}

    Any(const Any &other) : _content(other._content ? other._content->clone() : NULL) {}

    ~Any()
    {
        if (_content)
            delete _content;
    }

    const std::type_info &type() { return _content ? _content->type() : typeid(void); }

    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    template <typename T>
    T *get()
    {
        assert(typeid(T) == _content->type());
        return &((holder<T> *)_content)->val;
    }

    template <typename T>
    Any &operator=(const T &val)
    {
        /*先构造一个const T对象出来，然后进行交换，这样临时对象销毁的时候，顺带原先
        保存的placeholder也会被销毁*/
        Any(val).swap(*this);
        return *this;
    }

    Any &operator=(Any other)
    {
        /*这里要注意Any只是一个临时对象，进行交换后就会释放，所以交换后，原先保存的
        placeholder指针也会被销毁*/
        other.swap(*this);
        return *this;
    }

private:
    /*因为模板类编译器就会确定类型，因此*/
    class placeholder
    {
    public:
        virtual ~placeholder() {}
        virtual const std::type_info &type() = 0;
        virtual placeholder *clone() = 0;
    };

    /*当前的Any类中无法保存所有类型的对象，或者说不能整成模板类，因此声明一个holder
    模板类出来而holder类只要管理holder对象即可*/
    template <typename T>
    class holder : public placeholder
    {
    public:
        holder(const T &v) : val(v) {}
        ~holder() {}
        const std::type_info &type() { return typeid(T); }
        placeholder *clone() { return new holder(val); }
        T val;
    };
    placeholder *_content;
};

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
        int a = 10;
        float b = 20;
        std::string c = "Hello World";
        Any any_a(a);
        Any any_b(b);
        Any any_c(c);
        int *aa = any_a.get<int>();
        float *bb = any_b.get<float>();
        std::string *cc = any_c.get<std::string>();

        std::cout << *aa << std::endl;
        std::cout << *bb << std::endl;
        std::cout << *cc << std::endl;
    }
    {
        std::cout << "----------------通过构造和析构看看有没有内存泄漏----------------\n";
        Test d("Leihou");

        Any any_d = d;
        Any any_e(d);
        Any any_f(any_d);
        Any any_g = any_d;
    }
    {
        std::cout << "------------------any之间相互的赋值-就算any保存的类型不同也可以-----------------\n";
        Any any_f;

        any_f = 33;
        int *ff = any_f.get<int>();
        std::cout << *ff << std::endl;

        std::string c = "Hello World";
        any_f = c;
        std::string *gg = any_f.get<std::string>();
        std::cout << *gg << std::endl;

        any_f = Any(Test("test"));
        Test *hh = any_f.get<Test>();
        std::cout << hh->_data << std::endl;
    }

    while (1)
        sleep(1);
    return 0;
}