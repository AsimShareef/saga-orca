#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <grpcpp/grpcpp.h>
#include "saga.grpc.pb.h"

// Include your gRPC and Protobuf headers here
// #include <grpcpp/grpcpp.h>
// #include "saga.grpc.pb.h"

std::atomic<int> success_count{0};
std::atomic<int> failure_count{0};

void WorkerThread(int num_requests, int thread_id) {
    // 1. Create a channel and link it to the OrchestratorAPI Stub
    auto channel = grpc::CreateChannel("127.0.0.1:50000", grpc::InsecureChannelCredentials());
    std::unique_ptr<saga::OrchestratorAPI::Stub> stub = saga::OrchestratorAPI::NewStub(channel);

    for (int i = 0; i < num_requests; ++i) {
        // Build your payload testing both success and failure paths
        std::string status = (i % 4 == 0) ? "fail_payment" : "success";
        std::string payload = "{\"user\": \"user_" + std::to_string(thread_id) + "\", \"item\": \"macbook\", \"status\": \"" + status + "\"}";

        grpc::ClientContext context;
        
        // 2. Instantiate the exact Request and Response types from your proto
        saga::StartSagaRequest request;
        saga::StartSagaResponse response;
        
        // Match the 'order_details' field name defined in StartSagaRequest
        request.set_order_details(payload);

        // 3. Call the StartSaga RPC Method
        grpc::Status grpc_status = stub->StartSaga(&context, request, &response);

        // 4. Record the result using the atomic counters
        if (grpc_status.ok()) {
            success_count++;
        } else {
            failure_count++;
        }
    }
}

int main() {
    const int NUM_THREADS = 50;           // Number of concurrent clients
    const int REQUESTS_PER_THREAD = 200;  // 10,000 total requests

    std::cout << "Starting Load Test: " << (NUM_THREADS * REQUESTS_PER_THREAD) << " requests...\n";

    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    // Spawn threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(WorkerThread, REQUESTS_PER_THREAD, i);
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    int total_requests = success_count + failure_count;
    double tps = total_requests / elapsed.count();

    std::cout << "\n=== Load Test Results ===\n";
    std::cout << "Total Requests Executed: " << total_requests << "\n";
    std::cout << "Successful RPCs:         " << success_count << "\n";
    std::cout << "Failed RPCs:             " << failure_count << "\n";
    std::cout << "Total Time Elapsed:      " << elapsed.count() << " seconds\n";
    std::cout << "Peak Throughput:         " << tps << " TPS\n";

    return 0;
}