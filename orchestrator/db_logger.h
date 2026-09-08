#ifndef DB_LOGGER_H
#define DB_LOGGER_H

#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <memory>

struct LogEntry {
    std::string tx_id;
    std::string state;
    std::string payload;
};

class DbLogger {
public:
    explicit DbLogger(const std::string& conn_str);
    ~DbLogger();

    void LogStateTransition(const std::string& tx_id, const std::string& state, const std::string& payload);

private:
    void FlushWorker();

    std::unique_ptr<pqxx::connection> conn_;
    
    // Async Batching Infrastructure
    std::queue<LogEntry> log_queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    std::atomic<bool> stop_flag_{false};
};

#endif // DB_LOGGER_H