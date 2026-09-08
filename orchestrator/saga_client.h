#ifndef SAGA_CLIENT_H
#define SAGA_CLIENT_H

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "saga.grpc.pb.h"

class SagaParticipantClient {
    std::unique_ptr<saga::SagaParticipant::Stub> stub_;

public:
    SagaParticipantClient(std::shared_ptr<grpc::Channel> channel);
    
    // Sends the Execute network request
    bool Execute(const std::string& tx_id, const std::string& payload);
    
    // Sends the Compensate (Rollback) network request
    bool Compensate(const std::string& tx_id, const std::string& payload);
};

#endif