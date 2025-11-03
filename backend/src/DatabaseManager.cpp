#include "../include/DatabaseManager.h"
#include <sstream>
#include <iomanip>

// 获取用户信息
std::string DatabaseManager::getUserInfo(const std::string& cardNumber) {
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection->prepareStatement(
                "SELECT u.name, u.phone, c.card_number, c.balance, c.create_time "
                "FROM Users u JOIN Cards c ON u.user_id = c.user_id "
                "WHERE c.card_number = ?"
            )
        );
        pstmt->setString(1, cardNumber);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::stringstream json;
            json << "{"
                 << "\"status\":\"success\","
                 << "\"name\":\"" << res->getString("name") << "\","
                 << "\"card_number\":\"" << res->getString("card_number") << "\","
                 << "\"phone\":\"" << res->getString("phone") << "\","
                 << "\"balance\":" << res->getDouble("balance") << ","
                 << "\"create_time\":\"" << res->getString("create_time") << "\""
                 << "}";
            return json.str();
        }
        return "{\"status\":\"error\",\"message\":\"用户不存在\"}";
    } catch (sql::SQLException& e) {
        std::cerr << "获取用户信息错误: " << e.what() << std::endl;
        return "{\"status\":\"error\",\"message\":\"数据库错误\"}";
    }
}

// 存款操作
bool DatabaseManager::deposit(const std::string& cardNumber, double amount) {
    try {
        connection->setAutoCommit(false); // 开始事务
        
        // 1. 更新余额
        std::unique_ptr<sql::PreparedStatement> pstmt1(
            connection->prepareStatement(
                "UPDATE Cards SET balance = balance + ? WHERE card_number = ?"
            )
        );
        pstmt1->setDouble(1, amount);
        pstmt1->setString(2, cardNumber);
        pstmt1->executeUpdate();
        
        // 2. 获取更新后的余额
        std::unique_ptr<sql::PreparedStatement> pstmt2(
            connection->prepareStatement(
                "SELECT balance FROM Cards WHERE card_number = ?"
            )
        );
        pstmt2->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res(pstmt2->executeQuery());
        res->next();
        double newBalance = res->getDouble("balance");
        
        // 3. 记录交易
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
            connection->rollback(); // 回滚事务
            connection->setAutoCommit(true);
        } catch (...) {}
        std::cerr << "存款操作错误: " << e.what() << std::endl;
        return false;
    }
}

// 取款操作
bool DatabaseManager::withdraw(const std::string& cardNumber, double amount) {
    try {
        connection->setAutoCommit(false);
        
        // 1. 检查余额是否足够
        std::unique_ptr<sql::PreparedStatement> pstmt1(
            connection->prepareStatement(
                "SELECT balance FROM Cards WHERE card_number = ? AND status = 'active'"
            )
        );
        pstmt1->setString(1, cardNumber);
        std::unique_ptr<sql::ResultSet> res1(pstmt1->executeQuery());
        
        if (!res1->next()) {
            throw sql::SQLException("卡号不存在或已冻结");
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
        pstmt2->executeUpdate();
        
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
            connection->rollback();
            connection->setAutoCommit(true);
        } catch (...) {}
        std::cerr << "取款操作错误: " << e.what() << std::endl;
        return false;
    }
}

// 获取交易记录
std::string DatabaseManager::getTransactionHistory(const std::string& cardNumber) {
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection->prepareStatement(
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
                 << "\"amount\":" << res->getDouble("amount") << ","
                 << "\"balance_after\":" << res->getDouble("balance_after") << ","
                 << "\"description\":\"" << res->getString("description") << "\","
                 << "\"create_time\":\"" << res->getString("create_time") << "\""
                 << "}";
            first = false;
        }
        
        json << "]}";
        return json.str();
    } catch (sql::SQLException& e) {
        std::cerr << "获取交易记录错误: " << e.what() << std::endl;
        return "{\"status\":\"error\",\"message\":\"数据库错误\"}";
    }
}
