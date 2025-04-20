# 查找当前目录下所有的 .cpp 文件
SRC = $(wildcard *.cpp)

# 最终要生成的可执行文件
TARGET = main

# 默认目标，生成可执行文件
all: $(TARGET)

# 生成可执行文件的规则
$(TARGET): $(SRC)
	g++ -std=c++11 $^ -o $@

# 清理生成的文件
.PHONY: clean
clean:
	rm -f $(TARGET)




