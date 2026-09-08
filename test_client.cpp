#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "saga.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using saga::StartSagaRequest;
using saga::StartSagaResponse;
using saga::OrchestratorAPI;

class TestClient {
    std::unique_ptr<OrchestratorAPI::Stub> stub_;

public:
    TestClient(std::shared_ptr<Channel> channel) 
        : stub_(OrchestratorAPI::NewStub(channel)) {}

    void SendTransaction(const std::string& scenario_name, const std::string& json_payload) {
        StartSagaRequest request;
        request.set_order_details(json_payload);
        
        StartSagaResponse reply;
        ClientContext context;

        std::cout << "\n=== Running: " << scenario_name << " ===" << std::endl;
        std::cout << "Payload: " << json_payload << std::endl;
        
        Status status = stub_->StartSaga(&context, request, &reply);
        
        if (status.ok()) {
            std::cout << "-> Acknowledged! TxID: " << reply.tx_id() 
                      << " | Status: " << reply.final_status() << std::endl;
        } else {
            std::cerr << "-> Network RPC Failed." << std::endl;
        }
    }
};

int main() {
    TestClient client(grpc::CreateChannel("127.0.0.1:50000", grpc::InsecureChannelCredentials()));
    
    client.SendTransaction("Successful Transaction", "{\"user\": \"user_101\", \"item\": \"macbook_pro\", \"status\": \"success\"}");
    client.SendTransaction("Order Validation Failure", "{\"user\": \"user_101\", \"item\": \"macbook\", \"status\": \"fail_order\"}");
    client.SendTransaction("Out of Stock Failure", "{\"user\": \"user_101\", \"item\": \"RTX_5090\", \"status\": \"fail_inventory\"}");
    client.SendTransaction("Insufficient Funds Failure", "{\"user\": \"user_101\", \"item\": \"macbook\", \"status\": \"fail_payment\"}");

    return 0;
}