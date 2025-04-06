#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <fstream>
#include <functional>
#include <grpcpp/grpcpp.h>
#include "data_transfer.grpc.pb.h"
#include "data_classes.h"

// For JSON parsing, we use the popular nlohmann/json library.
// Make sure to add "json.hpp" to your include path (or use a package manager).
#include <nlohmann/json.hpp>
using json = nlohmann::json;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using datatransfer::CollisionRecordMsg;
using datatransfer::SendDataResponse;
using datatransfer::SendDataRequest;
using datatransfer::DataTransfer;

// Global CollisionDataManager instance (if Processor A stores some records locally)
CollisionDataManager manager;
std::atomic<int> global_record_count(0);
std::atomic<int> records_forwarded_B(0);
std::atomic<int> records_forwarded_C(0);

// Child stubs (for Processor B and Processor C)
std::unique_ptr<DataTransfer::Stub> childB_stub;
std::unique_ptr<DataTransfer::Stub> childC_stub;

// -------------------------
// Topology Configuration
// -------------------------

// Load the JSON topology file from the project root.
json loadTopology(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open topology configuration file: " + filename);
    }
    json topology;
    file >> topology;
    return topology;
}

// Initialize gRPC stubs for Processor B and Processor C using the topology.
void initializeChildStubs(const json& topology) {
    auto children = topology["neighbors"]["children"];
    if (children.size() < 2) {
        throw std::runtime_error("Insufficient child nodes defined in topology.");
    }
    // For this example, the first child is Processor B and the second is Processor C.
    std::string processorB_address = children[0]["ip"].get<std::string>() + ":" +
                                     std::to_string(children[0]["port"].get<int>());
    std::string processorC_address = children[1]["ip"].get<std::string>() + ":" +
                                     std::to_string(children[1]["port"].get<int>());

    auto channelB = grpc::CreateChannel(processorB_address, grpc::InsecureChannelCredentials());
    auto channelC = grpc::CreateChannel(processorC_address, grpc::InsecureChannelCredentials());
    childB_stub = DataTransfer::NewStub(channelB);
    childC_stub = DataTransfer::NewStub(channelC);
}

// -------------------------
// Partitioning and Forwarding
// -------------------------

// Global partitioning function.
// Assume we want to distribute records among 4 final nodes:
// 0,1,2 → go to Processor B’s subtree; 3 → goes to Processor C.
int getGlobalPartition(const CollisionRecordMsg& recordMsg) {
    int key = recordMsg.crash_date() + recordMsg.crash_time();
    std::hash<int> hasher;
    size_t hash_value = hasher(key);
    return hash_value % 4; // Returns 0, 1, 2, or 3.
}

// Forward record to Processor B's subtree.
void forwardToSubtreeB(const CollisionRecordMsg& recordMsg, int subPartition) {
    ClientContext context;
    SendDataResponse response;
    // Processor B must implement a unary RPC method "ForwardRecord" that accepts CollisionRecordMsg.
    grpc::Status status = childB_stub->ForwardRecord(&context, recordMsg, &response);
    if (!status.ok()) {
        std::cerr << "Forwarding to Processor B failed: " << status.error_message() << std::endl;
    } else {
        std::cout << "Forwarded record to Processor B's subtree (subPartition " << subPartition 
                  << "). Response: " << response.message() << std::endl;
    }
}

// Forward record directly to Processor C.
void forwardToProcessorC(const CollisionRecordMsg& recordMsg) {
    ClientContext context;
    SendDataResponse response;
    grpc::Status status = childC_stub->ForwardRecord(&context, recordMsg, &response);
    if (!status.ok()) {
        std::cerr << "Forwarding to Processor C failed: " << status.error_message() << std::endl;
    } else {
        std::cout << "Forwarded record to Processor C. Response: " << response.message() << std::endl;
    }
}

// Process record in Processor A: forward based on partition.
void processRecord(const CollisionRecordMsg& recordMsg) {
    int partition = getGlobalPartition(recordMsg);
    if (partition < 3) {
        forwardToSubtreeB(recordMsg, partition);
        ++records_forwarded_B;
    } else {
        forwardToProcessorC(recordMsg);
        ++records_forwarded_C;
    }
    int count = ++global_record_count;
    if (count % 10000 == 0) {
        std::cout << "Processor A has processed " << count << " records in total." << std::endl;
        std::cout << "Forwarded " << records_forwarded_B << " records to Processor B." << std::endl;
        std::cout << "Forwarded " << records_forwarded_C << " records to Processor C." << std::endl;
    }
}

// -------------------------
// gRPC Service Implementation
// -------------------------

class DataTransferServiceImpl final : public DataTransfer::Service {
public:
    // Existing unary RPC
    ::grpc::Status SendData(::grpc::ServerContext* context, 
                            const datatransfer::SendDataRequest* request,
                            datatransfer::SendDataResponse* reply) override {
        const CollisionRecordMsg& recordMsg = request->record();
        std::cout << "Received record (unary): Crash Date: " << recordMsg.crash_date()
                  << ", Crash Time: " << recordMsg.crash_time() << std::endl;
        processRecord(recordMsg);
        reply->set_success(true);
        reply->set_message("Record processed successfully.");
        return Status::OK;
    }
    
    // Implement the client-side streaming RPC
    ::grpc::Status StreamData(::grpc::ServerContext* context,
                            ::grpc::ServerReader<datatransfer::CollisionRecordMsg>* reader,
                            datatransfer::SendDataResponse* response) override {
        datatransfer::CollisionRecordMsg recordMsg;
        int local_count = 0;
        while (reader->Read(&recordMsg)) {
            processRecord(recordMsg);
            ++local_count;
            if (local_count % 10000 == 0) {
                std::cout << "Processed " << local_count << " records in this stream." << std::endl;
            }
        }
        // Optionally, include the final count in the response.
        response->set_success(true);
        response->set_message("Stream processed successfully: " + std::to_string(local_count) + " records received.");
        return ::grpc::Status::OK;
    }
};

void RunServer(const std::string& server_address, const json& topology) {
    // Initialize child stubs using the topology configuration.
    initializeChildStubs(topology);
    
    DataTransferServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Processor A (Node A) Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    try {
        // Load the topology configuration file from the project root.
        json topology = loadTopology("topology.json");
        std::string server_address("0.0.0.0:50051");
        RunServer(server_address, topology);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
