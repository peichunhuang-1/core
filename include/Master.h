#ifndef MASTER_H
#define MASTER_H
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <mutex>
#include <thread>
#include <condition_variable>

#include "registration.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace core {
    class RegistrationServiceImpl final : public Registration::Service {
        public:
        Status Subscribe(ServerContext* context, const SubscribeRequest* request,
                          grpc::ServerWriter<SubscribeReply>* writer) override {
            std::cout << "receive subscriber\n";
            const std::string topic_name = request->topic_name();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                receive_topic = topic_name;
                receive_ip = request->endpoint().ip();
                receive_port = request->endpoint().port();
            }
            subscribe_condition_.notify_all();
            while (1) {
                std::unique_lock<std::mutex> lock(mutex_);
                publish_condition_.wait(lock);
                std::cout << "publisher condition unlock\n";
                if (topic_name != receive_topic) continue;
                SubscribeReply reply;
                reply.set_topic_name(topic_name);
                EndPoint* endpoint = reply.mutable_endpoint();
                endpoint->set_ip(receive_ip);
                endpoint->set_port(receive_port);
                if (!writer->Write(reply)) break;
            }
            std::cout << "A subscriber died\n";
            return Status::OK;
        }
        Status Publish(ServerContext* context, const PublishRequest* request,
                          grpc::ServerWriter<PublishReply>* writer) override {
            std::cout << "receive publisher\n";
            const std::string topic_name = request->topic_name();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                receive_topic = topic_name;
                receive_ip = request->endpoint().ip();
                receive_port = request->endpoint().port();
            }
            publish_condition_.notify_all();
            while (1) {
                std::unique_lock<std::mutex> lock(mutex_);
                subscribe_condition_.wait(lock);
                std::cout << "subscriber condition unlock\n";
                if (topic_name != receive_topic) continue;
                PublishReply reply;
                reply.set_topic_name(topic_name);
                EndPoint* endpoint = reply.mutable_endpoint();
                endpoint->set_ip(receive_ip);
                endpoint->set_port(receive_port);
                if (!writer->Write(reply)) break;
            }
            std::cout << "A subscriber died\n";
            return Status::OK;
        }
        Status ServiceServers(ServerContext* context, const ServiceServerRequest* request,
                          ServiceServerReply *reply) override {
            {
                std::lock_guard<std::mutex> lock(service_mutex_);
                service_servers[request->service_name()] = std::pair<std::string, uint32_t>(request->endpoint().ip(), request->endpoint().port());
            }
            service_condition_.notify_all();
            return Status::OK;
        }
        Status ServiceClients(ServerContext* context, const ServiceClientRequest* request,
                          grpc::ServerWriter<ServiceClientReply>* writer) override {
            {
                ServiceClientReply reply;
                std::unique_lock<std::mutex> lock(service_mutex_);
                auto iter = service_servers.find(request->service_name());
                if ( iter != service_servers.end()) {
                    core::EndPoint* endpoint = reply.mutable_endpoint();
                    endpoint->set_ip(iter->second.first);
                    endpoint->set_port(iter->second.second);
                    writer->Write(reply);
                }
            }
            while (1) {
                std::unique_lock<std::mutex> lock(mutex_);
                service_condition_.wait(lock);
                {
                    ServiceClientReply reply;
                    std::lock_guard<std::mutex> lock(service_mutex_);
                    auto iter = service_servers.find(request->service_name());
                    if ( iter != service_servers.end()) {
                        core::EndPoint* endpoint = reply.mutable_endpoint();
                        endpoint->set_ip(iter->second.first);
                        endpoint->set_port(iter->second.second);
                        writer->Write(reply);
                    }
                }
            }
            return Status::OK;
        }
        private:
        std::mutex mutex_;
        std::mutex service_mutex_;
        std::condition_variable publish_condition_;
        std::condition_variable subscribe_condition_;
        std::condition_variable service_condition_;
        std::string receive_topic = "";
        std::string receive_ip = "";
        uint32_t receive_port = 0;
        std::unordered_map<std::string, std::pair<std::string, uint32_t> > service_servers;
    };
    void RunServer(std::string server_address) {
        RegistrationServiceImpl service;
        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<Server> server(builder.BuildAndStart());
        server->Wait();
    }
}
#endif