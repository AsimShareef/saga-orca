#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "saga.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using saga::TxRequest;
using saga::TxResponse;
using saga::SagaParticipant;

class OrderServiceImpl final : public SagaParticipant::Service {
    std::unordered_map<std::string, bool> processed_tx;
    std::mutex mtx;

public:
    Status ExecuteAction(ServerContext* context, const TxRequest* request, TxResponse* reply) override {
        std::lock_guard<std::mutex> lock(mtx);
        if (processed_tx.find(request->tx_id()) != processed_tx.end()) {
            reply->set_success(true); return Status::OK;
        }
        if (request->payload().find("fail_order") != std::string::npos) {
            std::cout << "[Order] FAILED creation for TxID: " << request->tx_id() << std::endl;
            reply->set_success(false); reply->set_error_message("Invalid order data"); return Status::OK;
        }
        std::cout << "[Order] Created order for TxID: " << request->tx_id() << std::endl;
        processed_tx[request->tx_id()] = true; reply->set_success(true); return Status::OK;
    }

    Status CompensateAction(ServerContext* context, const TxRequest* request, TxResponse* reply) override {
        std::lock_guard<std::mutex> lock(mtx);
        if (processed_tx.find(request->tx_id()) != processed_tx.end()) {
            std::cout << "[Order] ROLLED BACK order for TxID: " << request->tx_id() << std::endl;
            processed_tx.erase(request->tx_id());
        }
        reply->set_success(true); return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50053");
    OrderServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Order Service listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) { RunServer(); return 0; }