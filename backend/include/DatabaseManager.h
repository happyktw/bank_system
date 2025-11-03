#pragma once
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <iostream>
#include <string>
#include <memory>

class DatabaseManager {
private:
    sql::mysql::MySQL_Driver* driver;
    std::unique_ptr<sql::Connection> connection;
    
    DatabaseManager() {
        try {
            // 获取driver实例（这个是单例，不需要unique_ptr管理）
            driver = sql::mysql::get_mysql_driver_instance();
            connect();
        } catch (sql::SQLException& e) {
            std::cerr << "数据库连接失败: " << e.what() << std::endl;
        }
    }

    void connect() {
        std::string host = "tcp://127.0.0.1:3306";
        std::string user = "bank_admin";
        std::string password = "BankAdmin123!";
        std::string database = "bank_system";
        
        try {
            connection = std::unique_ptr<sql::Connection>(
                driver->connect(host, user, password)
            );
            connection->setSchema(database);
            std::cout << "数据库连接成功!" << std::endl;
        } catch (sql::SQLException& e) {
            std::cerr << "数据库连接错误: " << e.what() << std::endl;
        }
    }

public:
    static DatabaseManager& getInstance() {
        static DatabaseManager instance;
        return instance;
    }

    // 用户登录验证
    bool verifyLogin(const std::string& cardNumber, const std::string& password) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                connection->prepareStatement(
                    "SELECT card_id FROM Cards WHERE card_number = ? AND password_hash = MD5(?) AND status = 'active'"
                )
            );
            pstmt->setString(1, cardNumber);
            pstmt->setString(2, password);
            
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            bool success = res->next();
            std::cout << "登录验证: 卡号=" << cardNumber << " 结果=" << (success ? "成功" : "失败") << std::endl;
            return success;
        } catch (sql::SQLException& e) {
            std::cerr << "登录验证错误: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 获取用户信息
    std::string getUserInfo(const std::string& cardNumber);
    
    // 存款操作
    bool deposit(const std::string& cardNumber, double amount);
    
    // 取款操作
    bool withdraw(const std::string& cardNumber, double amount);
    
    // 获取交易记录
    std::string getTransactionHistory(const std::string& cardNumber);
    
    // 开户功能
    bool createAccount(const std::string& name, const std::string& idCard,
                      const std::string& phone, const std::string& address,
                      const std::string& cardNumber, const std::string& password,
                      double initialDeposit);

    // 查询余额
    double getBalance(const std::string& cardNumber) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                connection->prepareStatement(
                    "SELECT balance FROM Cards WHERE card_number = ?"
                )
            );
            pstmt->setString(1, cardNumber);
            
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            if (res->next()) {
                double balance = res->getDouble("balance");
                std::cout << "余额查询: 卡号=" << cardNumber << " 余额=" << balance << std::endl;
                return balance;
            }
            std::cout << "余额查询: 卡号不存在 " << cardNumber << std::endl;
            return -1;
        } catch (sql::SQLException& e) {
            std::cerr << "查询余额错误: " << e.what() << std::endl;
            return -1;
        }
    }

    // 检查数据库连接状态
    bool isConnected() {
        return connection && !connection->isClosed();
    }

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
};
