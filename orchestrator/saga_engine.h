#ifndef SAGA_ENGINE_H
#define SAGA_ENGINE_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <array>

// The Finite State Machine States
enum class SagaState {
    PENDING,
    EXECUTING_ORDER,
    ORDER_SUCCESS,
    EXECUTING_INVENTORY,
    INVENTORY_SUCCESS,
    EXECUTING_PAYMENT,
    COMPLETED,
    COMPENSATING_INVENTORY,
    COMPENSATING_ORDER,
    ABORTED
};

// Represents a single distributed transaction
struct SagaContext {
    std::string tx_id;
    std::string payload;
    std::atomic<SagaState> current_state;

    SagaContext(std::string id, std::string data) 
        : tx_id(std::move(id)), payload(std::move(data)), current_state(SagaState::PENDING) {}

    // Compare-And-Swap (CAS) to prevent timeout race conditions
    bool SafeTransition(SagaState expected, SagaState next) {
        return current_state.compare_exchange_strong(expected, next);
    }
};

// Lock-Striped Map for high-throughput concurrent access
class ActiveSagas {
    static constexpr size_t NUM_STRIPES = 256;
    std::array<std::mutex, NUM_STRIPES> locks;
    std::array<std::unordered_map<std::string, std::shared_ptr<SagaContext>>, NUM_STRIPES> maps;

    size_t GetStripe(const std::string& key) const {
        return std::hash<std::string>{}(key) % NUM_STRIPES;
    }

public:
    void Put(const std::string& tx_id, std::shared_ptr<SagaContext> ctx) {
        size_t stripe = GetStripe(tx_id);
        std::lock_guard<std::mutex> lock(locks[stripe]);
        maps[stripe][tx_id] = ctx;
    }

    std::shared_ptr<SagaContext> Get(const std::string& tx_id) {
        size_t stripe = GetStripe(tx_id);
        std::lock_guard<std::mutex> lock(locks[stripe]);
        auto it = maps[stripe].find(tx_id);
        if (it != maps[stripe].end()) {
            return it->second;
        }
        return nullptr;
    }
};

#endif