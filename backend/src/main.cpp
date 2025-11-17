#include "../crow_all.h"
#include "../include/DatabaseManager.h"
#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <fstream>
#include <sstream>
#include <iomanip>

// 获取项目绝对路径
std::string getProjectRoot() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::string path(cwd);
        size_t build_pos = path.find("/backend/build");
        if (build_pos != std::string::npos) {
            path = path.substr(0, build_pos);
        }
        return path;
    }
    return "/home/eo/bank_system";
}

// 手动读取文件内容
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "文件未找到: " + path;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    crow::SimpleApp app;
    
    std::string project_root = getProjectRoot();
    std::cout << "项目根目录: " << project_root << std::endl;
    
    std::cout << "银行系统后端服务启动中..." << std::endl;
    auto& db = DatabaseManager::getInstance();

    // 静态文件服务 - 手动读取文件
    CROW_ROUTE(app, "/")
    ([project_root]() {
        std::string file_path = project_root + "/frontend/html/login.html";
        std::cout << "加载登录页面: " << file_path << std::endl;
        
        std::string content = readFile(file_path);
        std::cout << "文件大小: " << content.size() << " 字节" << std::endl;
        
        crow::response response;
        response.write(content);
        response.add_header("Content-Type", "text/html; charset=utf-8");
        return response;
    });

    CROW_ROUTE(app, "/<string>")
    ([project_root](const std::string& filename) {
        std::string file_path = project_root + "/frontend/html/" + filename;
        std::cout << "加载页面: " << file_path << std::endl;
        
        std::string content = readFile(file_path);
        crow::response response;
        response.write(content);
        response.add_header("Content-Type", "text/html; charset=utf-8");
        return response;
    });
    
    CROW_ROUTE(app, "/api/userinfo/<string>")
    ([](const std::string& card_number) {
        std::cout << "获取用户信息: 卡号=" << card_number << std::endl;
    
        std::string userInfo = DatabaseManager::getInstance().getUserInfo(card_number);
        crow::response response(200, userInfo);
        response.add_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/css/<string>")
    ([project_root](const std::string& filename) {
        std::string file_path = project_root + "/frontend/css/" + filename;
        std::cout << "加载CSS: " << file_path << std::endl;
        
        std::string content = readFile(file_path);
        crow::response response;
        response.write(content);
        response.add_header("Content-Type", "text/css");
        return response;
    });

    CROW_ROUTE(app, "/js/<string>")
    ([project_root](const std::string& filename) {
        std::string file_path = project_root + "/frontend/js/" + filename;
        std::cout << "加载JS: " << file_path << std::endl;
        
        std::string content = readFile(file_path);
        crow::response response;
        response.write(content);
        response.add_header("Content-Type", "application/javascript");
        return response;
    });

    // 登录API
    CROW_ROUTE(app, "/api/login").methods("POST"_method)
    ([](const crow::request& req) {
        try {
            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "无效的JSON数据");
            }
            
            std::string card_number = json["card_number"].s();
            std::string password = json["password"].s();
            
            std::cout << "收到登录请求: 卡号=" << card_number << std::endl;
            
            bool success = DatabaseManager::getInstance().verifyLogin(card_number, password);
            
            crow::json::wvalue response;
            if (success) {
                response["status"] = "success";
                response["message"] = "登录成功";
                double balance = DatabaseManager::getInstance().getBalance(card_number);
                response["balance"] = balance;
            } else {
                response["status"] = "error";
                response["message"] = "卡号或密码错误";
            }
            return crow::response(200, response);
        } catch (const std::exception& e) {
            crow::json::wvalue error_response;
            error_response["status"] = "error";
            error_response["message"] = "服务器内部错误";
            return crow::response(500, error_response);
        }
    });

    // 查询余额API
    CROW_ROUTE(app, "/api/balance/<string>")
    ([](const std::string& card_number) {
        std::cout << "收到余额查询: 卡号=" << card_number << std::endl;
        
        double balance = DatabaseManager::getInstance().getBalance(card_number);
        
        crow::json::wvalue response;
        if (balance >= 0) {
            response["status"] = "success";
            response["card_number"] = card_number;
            response["balance"] = balance;
        } else {
            response["status"] = "error";
            response["message"] = "卡号不存在";
        }
        return crow::response(200, response);
    });

    // 存款API
    CROW_ROUTE(app, "/api/deposit").methods("POST"_method)
    ([](const crow::request& req) {
    try {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "{\"status\":\"error\",\"message\":\"无效的JSON数据\"}");
        }
        
        std::string card_number = json["card_number"].s();
        double amount = json["amount"].d();
        
        std::cout << "收到存款请求: 卡号=" << card_number << " 金额=" << amount << std::endl;
        
        bool success = DatabaseManager::getInstance().deposit(card_number, amount);
        
        crow::json::wvalue response;
            if (success) {
                response["status"] = "success";
                response["message"] = "存款成功";
                response["amount"] = amount;
                // 返回新余额
                double balance = DatabaseManager::getInstance().getBalance(card_number);
                response["balance"] = balance;
            } else {
                response["status"] = "error";
                response["message"] = "存款失败";
            }
            return crow::response(200, response);
        } catch (const std::exception& e) {
        crow::json::wvalue error_response;
        error_response["status"] = "error";
        error_response["message"] = "服务器内部错误";
        return crow::response(500, error_response);
        }
    });

    // 取款API
    CROW_ROUTE(app, "/api/withdraw").methods("POST"_method)
    ([](const crow::request& req) {
        try {
            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "{\"status\":\"error\",\"message\":\"无效的JSON数据\"}");
            }
        
        std::string card_number = json["card_number"].s();
        double amount = json["amount"].d();
        
        std::cout << "收到取款请求: 卡号=" << card_number << " 金额=" << amount << std::endl;
        
        bool success = DatabaseManager::getInstance().withdraw(card_number, amount);
        
        crow::json::wvalue response;
        if (success) {
            response["status"] = "success";
            response["message"] = "取款成功";
            response["amount"] = amount;
            double balance = DatabaseManager::getInstance().getBalance(card_number);
            response["balance"] = balance;
        } else {
            response["status"] = "error";
            response["message"] = "取款失败，请检查余额";
        }
        return crow::response(200, response);
    } catch (const std::exception& e) {
        crow::json::wvalue error_response;
        error_response["status"] = "error";
        error_response["message"] = "服务器内部错误";
        return crow::response(500, error_response);
        }
    });

    // 交易记录API
    CROW_ROUTE(app, "/api/transactions/<string>")
    ([](const std::string& card_number) {
        std::cout << "获取交易记录: 卡号=" << card_number << std::endl;
    
        std::string history = DatabaseManager::getInstance().getTransactionHistory(card_number);
        crow::response response(200, history);
        response.add_header("Content-Type", "application/json");
        return response;
    });

    // 健康检查端点
    CROW_ROUTE(app, "/health")
    ([]() {
        return "银行系统服务器运行正常";
    });

    std::cout << "服务器启动完成!" << std::endl;
    std::cout << "服务地址: http://0.0.0.0:18080" << std::endl;
    std::cout << "健康检查: http://0.0.0.0:18080/health" << std::endl;
    std::cout << "登录API: http://0.0.0.0:18080/api/login" << std::endl;
    
    app.port(18080).multithreaded().run();
    return 0;
}
