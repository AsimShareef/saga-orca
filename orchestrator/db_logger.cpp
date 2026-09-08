#include "db_logger.h"
#include <iostream>
#include <chrono>

DbLogger::DbLogger(const std::string& conn_str) 
    : conn_(std::make_unique<pqxx::connection>(conn_str)) {
    // Spin up the background consumer thread immediately upon initialization
    worker_thread_ = std::thread(&DbLogger::FlushWorker, this);
}

DbLogger::~DbLogger() {
    // Safely signal the background thread to shut down and flush remaining logs
    stop_flag_ = true;
    cv_.notify_one();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void DbLogger::LogStateTransition(const std::string& tx_id, const std::string& state, const std::string& payload) {
    {
        // Lock the mutex for literally a microsecond just to push to RAM
        std::lock_guard<std::mutex> lock(mtx_);
        log_queue_.push({tx_id, state, payload});
    }
    // Wake up the background thread to process the queue
    cv_.notify_one(); 
}

void DbLogger::FlushWorker() {
    while (true) {
        std::vector<LogEntry> batch;
        
        {
            std::unique_lock<std::mutex> lock(mtx_);
            // Wait for data, a shutdown signal, or max 50ms to flush stragglers
            cv_.wait_for(lock, std::chrono::milliseconds(50), 
                         [this] { return !log_queue_.empty() || stop_flag_; });

            // Swap the entire queue into a local vector instantly
            // This completely unblocks the 50 gRPC threads making requests
            while (!log_queue_.empty()) {
                batch.push_back(log_queue_.front());
                log_queue_.pop();
            }
        }

        if (!batch.empty()) {
            try {
                // Execute the entire batch in ONE physical transaction
                pqxx::work W(*conn_);
                for (const auto& entry : batch) {
                    W.exec_params(
                        "INSERT INTO saga_event_log (tx_id, current_state, payload) VALUES ($1, $2, $3::jsonb)",
                        entry.tx_id, entry.state, entry.payload
                    );
                }
                W.commit(); // The disk only spins once for the entire batch
            } catch (const std::exception& e) {
                std::cerr << "[DB Batch Error] " << e.what() << std::endl;
            }
        }

        // Exit the loop only after processing the final batch on shutdown
        if (stop_flag_ && batch.empty()) {
            break;
        }
    }
}