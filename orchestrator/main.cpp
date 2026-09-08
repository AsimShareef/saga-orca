#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "saga.grpc.pb.h"
#include "saga_engine.h"
#include "db_logger.h"
#include "saga_client.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using saga::StartSagaRequest;
using saga::StartSagaResponse;
using saga::OrchestratorAPI;

ActiveSagas active_sagas;
std::unique_ptr<DbLogger> db_logger;

void RunSagaWorkflow(std::string tx_id, std::string payload) {
    auto ctx = active_sagas.Get(tx_id);
    if (!ctx) return;

    // Initialize clients
    SagaParticipantClient order_client(grpc::CreateChannel("localhost:50053", grpc::InsecureChannelCredentials()));
    SagaParticipantClient inv_client(grpc::CreateChannel("localhost:50052", grpc::InsecureChannelCredentials()));
    SagaParticipantClient pay_client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    // Step 1: Order
    db_logger->LogStateTransition(tx_id, "EXECUTING_ORDER", payload);
    if (!order_client.Execute(tx_id, payload)) {
        db_logger->LogStateTransition(tx_id, "ABORTED", payload);
        ctx->SafeTransition(SagaState::PENDING, SagaState::ABORTED);
        return;
    }
    db_logger->LogStateTransition(tx_id, "ORDER_SUCCESS", payload);
    ctx->SafeTransition(SagaState::PENDING, SagaState::ORDER_SUCCESS);

    // Step 2: Inventory
    db_logger->LogStateTransition(tx_id, "EXECUTING_INVENTORY", payload);
    if (!inv_client.Execute(tx_id, payload)) {
        db_logger->LogStateTransition(tx_id, "COMPENSATING_ORDER", payload);
        order_client.Compensate(tx_id, payload);
        db_logger->LogStateTransition(tx_id, "ABORTED", payload);
        ctx->SafeTransition(SagaState::ORDER_SUCCESS, SagaState::ABORTED);
        return;
    }
    db_logger->LogStateTransition(tx_id, "INVENTORY_SUCCESS", payload);
    ctx->SafeTransition(SagaState::ORDER_SUCCESS, SagaState::INVENTORY_SUCCESS);

    // Step 3: Payment
    db_logger->LogStateTransition(tx_id, "EXECUTING_PAYMENT", payload);
    if (!pay_client.Execute(tx_id, payload)) {
        db_logger->LogStateTransition(tx_id, "COMPENSATING_INVENTORY", payload);
        inv_client.Compensate(tx_id, payload);
        db_logger->LogStateTransition(tx_id, "COMPENSATING_ORDER", payload);
        order_client.Compensate(tx_id, payload);
        db_logger->LogStateTransition(tx_id, "ABORTED", payload);
        ctx->SafeTransition(SagaState::INVENTORY_SUCCESS, SagaState::ABORTED);
        return;
    }

    // Success
    db_logger->LogStateTransition(tx_id, "COMPLETED", payload);
    ctx->SafeTransition(SagaState::INVENTORY_SUCCESS, SagaState::COMPLETED);
}

class OrchestratorServiceImpl final : public OrchestratorAPI::Service {
    int tx_counter = 1000;
    std::mutex counter_mtx;

    std::string GenerateTxId() {
        std::lock_guard<std::mutex> lock(counter_mtx);
        return "TX-" + std::to_string(++tx_counter);
    }

public:
    Status StartSaga(ServerContext* context, const StartSagaRequest* request, StartSagaResponse* reply) override {
        std::string tx_id = GenerateTxId();
        
        auto ctx = std::make_shared<SagaContext>(tx_id, request->order_details());
        active_sagas.Put(tx_id, ctx);

        db_logger->LogStateTransition(tx_id, "PENDING", request->order_details());
        std::cout << "[Orchestrator] Started Saga: " << tx_id << std::endl;

        // Spawn a background thread to execute the workflow asynchronously
        std::thread(RunSagaWorkflow, tx_id, request->order_details()).detach();

        reply->set_tx_id(tx_id);
        reply->set_final_status("PROCESSING");
        return Status::OK;
    }
};

void RunOrchestrator() {
    std::string server_address("0.0.0.0:50000");
    OrchestratorServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[Orchestrator] Listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    try {
        std::string conn_str = "dbname=saga_db user=saga_user password=saga_password host=127.0.0.1 port=5433";
        db_logger = std::make_unique<DbLogger>(conn_str);
        RunOrchestrator();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[Fatal Error] An unknown exception occurred." << std::endl;
        return 1;
    }
    return 0;
}