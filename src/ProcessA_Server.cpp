#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "data_transfer.grpc.pb.h"
#include "data_classes.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using datatransfer::CollisionRecordMsg;
using datatransfer::SendDataResponse;
using datatransfer::DataTransfer;

// Global CollisionDataManager instance.
CollisionDataManager manager;
std::atomic<int> global_record_count(0);

void processRecord(const CollisionRecordMsg& recordMsg) {
    CollisionRecord record(
        recordMsg.crash_date(),
        recordMsg.crash_time(),
        recordMsg.persons_injured(),
        recordMsg.persons_killed(),
        recordMsg.pedestrians_injured(),
        recordMsg.pedestrians_killed(),
        recordMsg.cyclists_injured(),
        recordMsg.cyclists_killed(),
        recordMsg.motorists_injured(),
        recordMsg.motorists_killed()
    );
    
    int totalNodes = 5;
    int nodeId = 0;
    int partition = getPartition(record, totalNodes);
    if (partition == nodeId) {
        manager.insertRecord(record);
        int count = ++global_record_count;
        if (count % 10000 == 0) {
            std::cout << "Processed " << count << " records locally on Node A." << std::endl;
        }
    } else {
        std::cout << "Forwarding record to Node " << partition << std::endl;
    }
}

// Updated service implementation with StreamData RPC.
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

void RunServer(const std::string& server_address) {
    DataTransferServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Processor A (Node A) Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50051");
    RunServer(server_address);
    return 0;
}
