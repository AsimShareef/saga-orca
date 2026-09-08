#include "saga_client.h"
#include <iostream>

using grpc::ClientContext;
using grpc::Status;
using saga::TxRequest;
using saga::TxResponse;

SagaParticipantClient::SagaParticipantClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(saga::SagaParticipant::NewStub(channel)) {}

bool SagaParticipantClient::Execute(const std::string& tx_id, const std::string& payload) {
    TxRequest request;
    request.set_tx_id(tx_id);
    request.set_payload(payload);
    
    TxResponse reply;
    ClientContext context;

    Status status = stub_->ExecuteAction(&context, request, &reply);
    
    if (status.ok()) {
        return reply.success();
    } else {
        std::cerr << "[gRPC Error] Execute Failed for " << tx_id << ": " 
                  << status.error_code() << ": " << status.error_message() << std::endl;
        return false;
    }
}

bool SagaParticipantClient::Compensate(const std::string& tx_id, const std::string& payload) {
    TxRequest request;
    request.set_tx_id(tx_id);
    request.set_payload(payload);
    
    TxResponse reply;
    ClientContext context;

    Status status = stub_->CompensateAction(&context, request, &reply);
    
    if (status.ok()) {
        return reply.success();
    } else {
        std::cerr << "[gRPC Error] Compensate Failed for " << tx_id << ": " 
                  << status.error_code() << ": " << status.error_message() << std::endl;
        return false;
    }
}