#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <google/protobuf/any.pb.h>

#include <mutex>
#include <thread>
#include <condition_variable>

#include "registration.grpc.pb.h"
#include "service_message.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using core::Registration;
using core::SubscribeReply;
using core::SubscribeRequest;
using core::PublishReply;
using core::PublishRequest;

using core::ServiceMessage;
using core::ServiceServerMessageReply;
using core::ServiceServerMessageRequest;
using core::ServiceClientMessageReply;
using core::ServiceClientMessageRequest;

struct EndPoint
{
    public:
    std::string node_name = "";
    std::string ip = "";
    uint32_t port = 0;
};

class RegistrationServiceImpl final : public Registration::Service {
  Status Subscribe(ServerContext* context, const SubscribeRequest* request,
                  grpc::ServerWriter<SubscribeReply>* writer) override {
    std::cout << "Subscriber\n";
    const std::string topic_name = request->topic_name();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        for (auto iter = history_endpoints[topic_name].begin(); iter != history_endpoints[topic_name].end(); iter++)
        {
            SubscribeReply reply;
            reply.set_topic_name(topic_name);
            core::EndPoint* endpoint = reply.mutable_endpoint();
            endpoint->set_node_name(iter->node_name);
            endpoint->set_node_ip(iter->ip);
            endpoint->set_node_port(iter->port);
            writer->Write(reply);
        }
    }
    while(true)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock);
        if (receive_topic != topic_name) continue;
        SubscribeReply reply;
        reply.set_topic_name(topic_name);
        core::EndPoint* endpoint = reply.mutable_endpoint();
        endpoint->set_node_name(receive_node_name);
        endpoint->set_node_ip(receive_ip);
        endpoint->set_node_port(receive_port);
        writer->Write(reply);
    }
    return Status::OK;
  }
  Status Publish(ServerContext* context, const PublishRequest* request,
                  PublishReply* reply) override {
    std::cout << "Publisher\n";
    std::lock_guard<std::mutex> lock(mutex_);
    receive_topic = request->topic_name();
    receive_node_name = request->endpoint().node_name();
    receive_ip= request->endpoint().node_ip();
    receive_port= request->endpoint().node_port();
    history_endpoints[receive_topic].push_back(EndPoint{
        .node_name = receive_node_name,
        .ip = receive_ip,
        .port = receive_port
    });
    condition_.notify_all();
    return Status::OK;
  }
  private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::string receive_topic = "";
  std::string receive_node_name = "";
  std::string receive_ip = "";
  uint32_t receive_port = 0;
  std::unordered_map<std::string, std::vector<EndPoint> > history_endpoints;

};

class ServiceMessageServiceImpl final : public ServiceMessage::Service{
    Status ServiceServer(ServerContext* context, grpc::ServerReader<ServiceServerMessageRequest>* reader,
                  ServiceServerMessageReply* reply){
        ServiceServerMessageRequest request;
        while (reader->Read(&request))
        {
            std::unique_lock<std::mutex> lock(mutex_);
            receive_service = request.service_name();
            std::cout << "Receive Service " << receive_service << "\n";
            receive_data = request.data();
            condition_.notify_all();
        }
        return Status::OK;
    }
    Status ServiceClient(ServerContext* context, const ServiceClientMessageRequest* request,
                  grpc::ServerWriter<ServiceClientMessageReply>* writer){
        const std::string service_name = request->service_name();
        std::cout << "Wait Service " << service_name << "\n";
        while (true)
        {
            ServiceClientMessageReply reply;
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock);
            if (receive_service != service_name) continue;
            std::cout << "Send Receive Service to Client\n";
            *reply.mutable_data() = receive_data;
            writer->Write(reply);
        }
        return Status::OK;
    }
    private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::string receive_service = "";
    google::protobuf::Any receive_data; 
};

void RunServer(std::string ip, uint16_t port)
{
    std::string server_address = ip + ":" + std::to_string(port);
    RegistrationServiceImpl service;
    ServiceMessageServiceImpl message_service;
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    builder.RegisterService(&message_service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
}

int main(int argc, char** argv)
{
    RunServer("127.0.0.1", 50051);
}
