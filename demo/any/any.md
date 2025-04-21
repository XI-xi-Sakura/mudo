### 通用类型any类型的实现：


每一个Connection对连接进行管理，最终都不可避免需要涉及到应用层协议的处理，因此在Connection中需要设置协议处理的上下文来控制处理节奏。但是应用层协议千千万，为了降低耦合度，这个协议接收解析上下文就不能有明显的协议倾向，它可以是任意协议的上下文信息，**因此就需要一个通用的类型来保存各种不同的数据结构**。

在C语言中，通用类型可以使用void*来管理，但是在C++中，boost库和C++17给我们提供了一个通用类型any来灵活使用，如果考虑增加代码的移植性，尽量减少第三方库的依赖，则可以使用C++17特性中的any，或者自己来实现。而这个any通用类型类的实现其实并不复杂，test1中是简单的部分实现。 



在C++ 17及以后的版本中，`std::any` 是标准库引入的一个类型，它可以存储任意类型的单个值。`std::any` 位于 `<any>` 头文件中，它为存储不同类型的数据提供了一种灵活的方式。以下是关于 `std::any` 的详细介绍：

### 特性
1. **类型安全**：`std::any` 是类型安全的，它会记录存储值的实际类型，并且在访问时会进行类型检查。
2. **可存储任意类型**：它可以存储任何可复制构造的类型，包括基本数据类型（如 `int`、`double` 等）、自定义类对象等。
3. **动态类型**：`std::any` 是一种动态类型，在运行时可以改变存储的值和类型。

### 基本用法
#### 1. 创建 `std::any` 对象
```cpp
#include <iostream>
#include <any>

int main() {
    std::any value;  // 创建一个空的 std::any 对象
    value = 42;      // 存储一个 int 类型的值
    value = "Hello"; // 存储一个 const char* 类型的值

    return 0;
}
```
#### 2. 检查 `std::any` 是否包含值
```cpp
#include <iostream>
#include <any>

int main() {
    std::any value;
    if (value.has_value()) {
        std::cout << "Value exists." << std::endl;
    } else {
        std::cout << "No value stored." << std::endl;
    }

    value = 42;
    if (value.has_value()) {
        std::cout << "Value exists." << std::endl;
    }

    return 0;
}
```
#### 3. 获取存储值的类型信息
```cpp
#include <iostream>
#include <any>

int main() {
    std::any value = 42;
    const std::type_info& type = value.type();
    std::cout << "Type: " << type.name() << std::endl;

    return 0;
}
```
#### 4. 访问存储的值
```cpp
#include <iostream>
#include <any>

int main() {
    std::any value = 42;
    try {
        int intValue = std::any_cast<int>(value);
        std::cout << "Value: " << intValue << std::endl;
    } catch (const std::bad_any_cast& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
```

### 注意事项
- **类型转换异常**：使用 `std::any_cast` 进行类型转换时，如果类型不匹配，会抛出 `std::bad_any_cast` 异常，因此在使用时需要进行异常处理。
- **性能开销**：由于 `std::any` 需要管理不同类型的值，并且进行类型检查，因此会有一定的性能开销。在性能敏感的场景中，应谨慎使用。
- **生命周期管理**：`std::any` 会负责管理存储值的生命周期，当 `std::any` 对象被销毁时，存储的值也会被销毁。 
