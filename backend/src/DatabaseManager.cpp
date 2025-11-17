#include "../include/DatabaseManager.h"
#include <sstream>
#include <iomanip>
#include <mutex>

// 简单的连接管理器，确保每个操作使用独立的连接
class ConnectionManager {
private:
    static std::mutex driver_mutex;
    static sql::mysql::MySQL_Driver* driver;
    
public:
    static sql::Connection* createConnection() {
        std::lock_guard<std::mutex> lock(driver_mutex);
        if (!driver) {
            driver = sql::mysql::get_mysql_driver_instance();
        }
        
        try {
            // 使用与主连接相同的凭据
            return driver->connect("tcp://127.0.0.1:3306", "bank_admin", "BankAdmin123!");
        } catch (sql::SQLException& e) {
            std::cerr << "创建数据库连接失败: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    static void closeConnection(sql::Connection* conn) {
        if (conn) {
            try {
                // 先设置schema
                conn->setSchema("bank_system");
                delete conn;
            } catch (...) {
                // 忽略关闭连接时的异常
            }
        }
    }
};

// 静态成员初始化
std::mutex ConnectionManager::driver_mutex;
sql::mysql::MySQL_Driver* ConnectionManager::driver = nullptr;

// 获取用户信息
std::string DatabaseManager::getUserInfo(const std::string& cardNumber) {
    sql::Connection* conn = ConnectionManager::createConnection();
    if (!conn) {
        return "{\"status\":\"error\",\"message\":\"数据库连接失败\"}";
    }
    
    try {
        // 设置数据库schema
        conn->setSchema("bank_system");
        
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT u.name, u.phone, c.card_number, c.balance, c.create_time "
                "FROM Users u JOIN Cards c ON u.user_id = c.user_id "
                "WHERE c.card_number = ?"
            )
        );
        pstmt->setString(1, cardNumber);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string name = res->getString("name");
            std::string phone = res->getString("phone");
            std::string card_number = res->getString("card_number");
            double balance = res->getDouble("balance");
            std::string create_time = res->getString("create_time");
            
            std::stringstream json;
            json << "{"
                 << "\"status\":\"success\","
                 << "\"name\":\"" << name << "\","
                 << "\"card_number\":\"" << card_number << "\","
                 << "\"phone\":\"" << phone << "\","
                 << "\"balance\":" << std::fixed << std::setprecision(2) << balance << ","
                 << "\"create_time\":\"" << create_time << "\""
                 << "}";
            
            ConnectionManager::closeConnection(conn);
            return json.str();
        }
        
        ConnectionManager::closeConnection(conn);
        return "{\"status\":\"error\",\"message\":\"用户不存在\"}";
    } catch (sql::SQLException& e) {
        std::cerr << "获取用户信息错误: " << e.what() << std::endl;
        ConnectionManager::closeConnection(conn);
        return "{\"status\":\"error\",\"message\":\"数据库错误: " + std::string(e.what()) + "\"}";
    }
}

// 获取交易记录
std::string DatabaseManager::getTransactionHistory(const std::string& cardNumber) {
    sql::Connection* conn = ConnectionManager::createConnection();
    if (!conn) {
        return "{\"status\":\"error\",\"message\":\"数据库连接失败\"}";
    }
    
    try {
        // 设置数据库schema
        conn->setSchema("bank_system");
        
        // 首先验证卡号是否存在
        std::unique_ptr<sql::PreparedStatement> pstmt_check(
            conn->prepareStatement(
                "SELECT card_id FROM Cards WHERE card_number = ?"
            )
        );
        pstmt_check->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res_check(pstmt_check->executeQuery());
        
        if (!res_check->next()) {
            ConnectionManager::closeConnection(conn);
            return "{\"status\":\"error\",\"message\":\"卡号不存在\"}";
        }
        
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT t.type, t.amount, t.balance_after, t.description, t.create_time "
                "FROM Transactions t JOIN Cards c ON t.card_id = c.card_id "
                "WHERE c.card_number = ? ORDER BY t.create_time DESC LIMIT 20"
            )
        );
        pstmt->setString(1, cardNumber);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        std::stringstream json;
        json << "{\"status\":\"success\",\"transactions\":[";
        
        bool first = true;
        while (res->next()) {
            if (!first) json << ",";
            json << "{"
                 << "\"type\":\"" << res->getString("type") << "\","
                 << "\"amount\":" << std::fixed << std::setprecision(2) << res->getDouble("amount") << ","
                 << "\"balance_after\":" << std::fixed << std::setprecision(2) << res->getDouble("balance_after") << ","
                 << "\"description\":\"" << res->getString("description") << "\","
                 << "\"create_time\":\"" << res->getString("create_time") << "\""
                 << "}";
            first = false;
        }
        
        json << "]}";
        
        ConnectionManager::closeConnection(conn);
        return json.str();
    } catch (sql::SQLException& e) {
        std::cerr << "获取交易记录错误: " << e.what() << std::endl;
        ConnectionManager::closeConnection(conn);
        return "{\"status\":\"error\",\"message\":\"数据库错误: " + std::string(e.what()) + "\"}";
    }
}

// 存款操作 - 使用主连接以确保事务一致性
bool DatabaseManager::deposit(const std::string& cardNumber, double amount) {
    if (amount <= 0) {
        std::cerr << "存款金额必须大于0" << std::endl;
        return false;
    }
    
    try {
        // 检查连接是否有效
        if (!connection || connection->isClosed()) {
            std::cerr << "数据库连接已关闭，重新连接..." << std::endl;
            connect();
        }
        
        connection->setAutoCommit(false); // 开始事务
        
        // 1. 检查卡号是否存在且状态正常
        std::unique_ptr<sql::PreparedStatement> pstmt_check(
            connection->prepareStatement(
                "SELECT card_id, status FROM Cards WHERE card_number = ?"
            )
        );
        pstmt_check->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res_check(pstmt_check->executeQuery());
        
        if (!res_check->next()) {
            throw sql::SQLException("卡号不存在");
        }
        
        std::string status = res_check->getString("status");
        if (status != "active") {
            throw sql::SQLException("卡片状态异常，无法存款");
        }
        
        // 2. 更新余额
        std::unique_ptr<sql::PreparedStatement> pstmt1(
            connection->prepareStatement(
                "UPDATE Cards SET balance = balance + ? WHERE card_number = ?"
            )
        );
        pstmt1->setDouble(1, amount);
        pstmt1->setString(2, cardNumber);
        int updated = pstmt1->executeUpdate();
        
        if (updated == 0) {
            throw sql::SQLException("更新余额失败");
        }
        
        // 3. 获取更新后的余额
        std::unique_ptr<sql::PreparedStatement> pstmt2(
            connection->prepareStatement(
                "SELECT balance FROM Cards WHERE card_number = ?"
            )
        );
        pstmt2->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res(pstmt2->executeQuery());
        res->next();
        double newBalance = res->getDouble("balance");
        
        // 4. 记录交易
        std::unique_ptr<sql::PreparedStatement> pstmt3(
            connection->prepareStatement(
                "INSERT INTO Transactions (card_id, type, amount, balance_after, description) "
                "SELECT card_id, 'deposit', ?, ?, ? FROM Cards WHERE card_number = ?"
            )
        );
        pstmt3->setDouble(1, amount);
        pstmt3->setDouble(2, newBalance);
        pstmt3->setString(3, "存款操作");
        pstmt3->setString(4, cardNumber);
        pstmt3->executeUpdate();
        
        connection->commit(); // 提交事务
        connection->setAutoCommit(true);
        
        std::cout << "存款成功: 卡号=" << cardNumber << " 金额=" << amount << " 新余额=" << newBalance << std::endl;
        return true;
    } catch (sql::SQLException& e) {
        try {
            if (connection) {
                connection->rollback(); // 回滚事务
                connection->setAutoCommit(true);
            }
        } catch (...) {}
        std::cerr << "存款操作错误: " << e.what() << std::endl;
        return false;
    }
}

// 取款操作 - 使用主连接以确保事务一致性
bool DatabaseManager::withdraw(const std::string& cardNumber, double amount) {
    if (amount <= 0) {
        std::cerr << "取款金额必须大于0" << std::endl;
        return false;
    }
    
    try {
        // 检查连接是否有效
        if (!connection || connection->isClosed()) {
            std::cerr << "数据库连接已关闭，重新连接..." << std::endl;
            connect();
        }
        
        connection->setAutoCommit(false);
        
        // 1. 检查余额是否足够
        std::unique_ptr<sql::PreparedStatement> pstmt1(
            connection->prepareStatement(
                "SELECT card_id, balance, status FROM Cards WHERE card_number = ?"
            )
        );
        pstmt1->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res1(pstmt1->executeQuery());
        
        if (!res1->next()) {
            throw sql::SQLException("卡号不存在");
        }
        
        std::string status = res1->getString("status");
        if (status != "active") {
            throw sql::SQLException("卡片已冻结，无法取款");
        }
        
        double currentBalance = res1->getDouble("balance");
        if (currentBalance < amount) {
            throw sql::SQLException("余额不足");
        }
        
        // 2. 更新余额
        std::unique_ptr<sql::PreparedStatement> pstmt2(
            connection->prepareStatement(
                "UPDATE Cards SET balance = balance - ? WHERE card_number = ?"
            )
        );
        pstmt2->setDouble(1, amount);
        pstmt2->setString(2, cardNumber);
        int updated = pstmt2->executeUpdate();
        
        if (updated == 0) {
            throw sql::SQLException("更新余额失败");
        }
        
        // 3. 获取新余额并记录交易
        std::unique_ptr<sql::PreparedStatement> pstmt3(
            connection->prepareStatement(
                "SELECT balance FROM Cards WHERE card_number = ?"
            )
        );
        pstmt3->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res2(pstmt3->executeQuery());
        res2->next();
        double newBalance = res2->getDouble("balance");
        
        std::unique_ptr<sql::PreparedStatement> pstmt4(
            connection->prepareStatement(
                "INSERT INTO Transactions (card_id, type, amount, balance_after, description) "
                "SELECT card_id, 'withdraw', ?, ?, ? FROM Cards WHERE card_number = ?"
            )
        );
        pstmt4->setDouble(1, amount);
        pstmt4->setDouble(2, newBalance);
        pstmt4->setString(3, "取款操作");
        pstmt4->setString(4, cardNumber);
        pstmt4->executeUpdate();
        
        connection->commit();
        connection->setAutoCommit(true);
        
        std::cout << "取款成功: 卡号=" << cardNumber << " 金额=" << amount << " 新余额=" << newBalance << std::endl;
        return true;
    } catch (sql::SQLException& e) {
        try {
            if (connection) {
                connection->rollback();
                connection->setAutoCommit(true);
            }
        } catch (...) {}
        std::cerr << "取款操作错误: " << e.what() << std::endl;
        return false;
    }
}
