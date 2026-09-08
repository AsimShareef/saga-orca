#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>

class DbLogger {
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex db_mtx_;

public:
    DbLogger(const std::string& conn_str);
    void LogStateTransition(const std::string& tx_id, const std::string& state, const std::string& payload);
};