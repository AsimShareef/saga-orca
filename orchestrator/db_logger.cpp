#include "db_logger.h"
#include <iostream>

DbLogger::DbLogger(const std::string& conn_str) : conn_(std::make_unique<pqxx::connection>(conn_str)) {}

void DbLogger::LogStateTransition(const std::string& tx_id, const std::string& state, const std::string& payload) {
    std::lock_guard<std::mutex> lock(db_mtx_);
    try {
        pqxx::work W(*conn_);
        
        // Explicitly cast parameter $3 to jsonb so PostgreSQL validates and stores it as a JSON object
        W.exec_params(
            "INSERT INTO saga_event_log (tx_id, current_state, payload) VALUES ($1, $2, $3::jsonb)",
            tx_id, state, payload
        );
        
        W.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB Error] Failed to log " << tx_id << ": " << e.what() << std::endl;
    }
}