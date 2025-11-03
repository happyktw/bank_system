cmake_minimum_required(VERSION 3.10)
project(BankSystemBackend)

set(CMAKE_CXX_STANDARD 14)

# 查找MySQL连接器
find_path(MYSQL_CONNECTOR_INCLUDE_DIR
    NAMES mysql_driver.h
    PATHS /usr/include/mysql-cppconn-8 /usr/include/mysql-cppconn-8/mysql
)

find_library(MYSQL_CONNECTOR_LIB
    NAMES mysqlcppconn8 mysqlcppconn
    PATHS /usr/lib/x86_64-linux-gnu
)

# 查找Boost库
find_package(Boost REQUIRED COMPONENTS system)

# 包含头文件
include_directories(include ${MYSQL_CONNECTOR_INCLUDE_DIR})

# 创建可执行文件
add_executable(bank_server 
    src/main.cpp
    src/DatabaseManager.cpp
)

# 链接库
target_link_libraries(bank_server 
    ${MYSQL_CONNECTOR_LIB}
    ${Boost_LIBRARIES}
    pthread
)
