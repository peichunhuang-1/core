#ifndef NODEHANDLER_H
#define NODEHANDLER_H

#include <iostream>
#include <queue>
#include "TCPSocket.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include "registration.grpc.pb.h"
#include "connection.grpc.pb.h"
#include "Timer.h"

#include "registration.grpc.pb.h"
#include "connection.grpc.pb.h"
#include <signal.h>

namespace core {
    using grpc::Channel;
    using grpc::ClientContext;
    using grpc::CompletionQueue;
    using grpc::Server;
    using grpc::ServerBuilder;
    using grpc::ServerContext;
    using grpc::Status;
    std::condition_variable spin_cv;
    void spinOnce() {
        spin_cv.notify_all();
    }
    class NodeHandler;
    class ConnectionServiceImpl;
    class Communicator {
        public: 
        Communicator() {}
        virtual void call(std::string &ip, uint32_t &port, float &freq) {}
    };
    template<class T>
    class Subscriber : public Communicator {
        using FunctionType = void(*)(T);
        public:
        Subscriber(std::string topic, float freq, void (*func)(T), NodeHandler *nh) ;
        void call(std::string &ip, uint32_t &port, float &freq) override {
            std::unique_lock<std::mutex> lock(this->mutex_);
            ip = this->tcp_ip;
            port = this->tcp_port;
            freq = this->rate;
        }
        private:
        NodeHandler* nh_;
        FunctionType cb_func;
        std::queue<T> msgs_queue;
        std::mutex mutex_;
        std::mutex queue_mutex_;
        std::mutex spin_mutex_;
        uint32_t tcp_port;
        uint32_t rpc_port;
        std::string tcp_ip;
        float rate;
        AcceptorSocket tcp_acceptor;
        int maxSize = 10;
        std::string topic_name;
    };
    template<class T>
    class Publisher : public Communicator {
        public:
        Publisher(std::string topic, NodeHandler *nh);
        void publish(T msg) {
            int siz = msg.ByteSizeLong() + 4;
            char *buf = new char [siz];
            google::protobuf::io::ArrayOutputStream aos(buf,siz);
            google::protobuf::io::CodedOutputStream *coded_output = new google::protobuf::io::CodedOutputStream(&aos);
            coded_output->WriteVarint32(msg.ByteSizeLong());
            msg.SerializeToCodedStream(coded_output);
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            if (msg_queue.size() > maxSize) msg_queue.pop();
            msg_queue.push(std::string(buf, siz));
        }
        void call(std::string &ip, uint32_t &port, float &freq) override {
            bool ret = false;
            std::shared_ptr<ClientSocket> c_sock = std::make_shared<ClientSocket>(ret);
            
            if (ret) {
                ret = c_sock->Connect(ip, port);
                if (ret){
                    /* Path 2 for create publisher client*/
                    std::cout << "Successful Connected from publisher to subscriber " << ip << ":" << port << "\n";
                    std::thread publish_thread_ = std::thread([this, c_sock, freq]() {
                        Rate rate(freq);
                        while (1) {
                            std::string msg;
                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex_);
                                if (this->msg_queue.size() > 0) {
                                    msg = this->msg_queue.back();
                                    if (!c_sock->Send(msg)) break;
                                }
                            }
                            // c_sock->Send(msg);
                            rate.sleep();
                        }
                        c_sock->disconnect();
                    });
                    publish_thread_.detach();
                }
            }
        }
        private:
        NodeHandler* nh_;
        std::mutex queue_mutex_;
        std::queue<std::string> msg_queue;
        std::string topic_name;
        int maxSize = 10;
    };
    class NodeHandler {
        public:
        NodeHandler();
        template<class T>
        Subscriber<T>& subscribe(std::string topic, float freq, void (*func)(T)) {
            std::shared_ptr<Subscriber<T> > sub = std::make_shared<Subscriber<T> >(topic, freq, func, this);
            std::unique_lock<std::mutex> lock(mutex_);
            this->subscribers[topic] = sub;
            return *sub;
        }
        template<class T>
        Publisher<T>& advertise(std::string topic) {
            std::shared_ptr<Publisher<T> > pub =  std::make_shared<Publisher<T> >(topic, this);
            std::unique_lock<std::mutex> lock(mutex_);
            this->publishers[topic] = pub;
            return *pub;
        }
        std::unique_ptr<Registration::Stub> stub_;
        std::unique_ptr<Server> server;
        ConnectionServiceImpl *service;
        std::unordered_map<std::string, std::shared_ptr<Communicator> > subscribers;
        std::unordered_map<std::string, std::shared_ptr<Communicator> > publishers; 
        std::mutex mutex_;
        std::string local_ip;
        int rpc_port;
        std::string master_addr;
    };
    class ConnectionServiceImpl final : public Connection::Service {
        public:
        ConnectionServiceImpl(NodeHandler *nh) : nh_(nh) {}
        Status Subscriber(ServerContext* context, const SubscriberRequest* request,
                        SubscriberReply* reply) override {
            std::string topic = request->topic_name();
            std::string ip = request->tcp_endpoint().ip();
            uint32_t port = request->tcp_endpoint().port();
            float freq = request->rate();
            std::unique_lock<std::mutex> lock(this->nh_->mutex_);
            this->nh_->publishers[topic]->call(ip, port, freq);
            std::cout << "Receive from Subscriber " << ip << ":" << port << "\n";
            return Status::OK;
        }
        Status Publisher(ServerContext* context, const PublisherRequest* request,
                        PublisherReply* reply) override {
            std::string topic = request->topic_name();
            std::string ip;
            uint32_t port;
            float freq;
            std::unique_lock<std::mutex> lock(this->nh_->mutex_);
            this->nh_->subscribers[topic]->call(ip, port, freq);
            core::EndPoint* endpoint = reply->mutable_tcp_endpoint();
            endpoint->set_ip(ip);
            endpoint->set_port(port);
            reply->set_rate(freq);
            reply->set_topic_name(topic);
            std::cout << "Receive from Publisher " << ip << ":" << port << "\n";
            return Status::OK;
        }
        private:
        NodeHandler *nh_;
    };
    template<class T>
    Subscriber<T>::Subscriber(std::string topic, float freq, void (*func)(T), NodeHandler *nh) :
        topic_name(topic), rate(freq), cb_func(func), nh_(nh)
    {
        /* Part I. start accepting and receiving from tcp port */
        {
            std::unique_lock<std::mutex> lock(this->nh_->mutex_);
            this->tcp_ip = this->nh_->local_ip;
            this->rpc_port = this->nh_->rpc_port;
        }
        bool ret = false;
        tcp_acceptor = AcceptorSocket(this->tcp_ip, this->tcp_port, ret);
        if (ret) {
            std::thread acceptor_thread_ = std::thread([this]() {
                while (1) {
                    bool ret;
                    int sock;
                    ret = this->tcp_acceptor.Accept(sock);
                    std::cout << "Successful Create acceptor socket\n";
                    if (ret) {
                        std::shared_ptr<ServerSocket<T> > srv_sock = std::make_shared<ServerSocket<T> >(sock);
                        std::thread receive_thread_ = std::thread([this, srv_sock]() {
                            std::cout << "Successful Connected as subscriber " << this->tcp_ip << ":" << this->tcp_port << "\n";
                            while (1) {
                                T msg;
                                bool ret = srv_sock->SocketHandler(msg);
                                if (ret) {
                                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                                    if (this->msgs_queue.size() > maxSize) this->msgs_queue.pop();
                                    this->msgs_queue.push(msg);
                                }
                                else {
                                    break;
                                }
                            }
                            srv_sock->disconnect();
                        });
                        receive_thread_.detach();
                    }
                }
                this->tcp_acceptor.disconnect();
            });
            acceptor_thread_.detach();
        }
        /* Part II. sending request to master for registration */
        SubscribeRequest request;
        {
            std::unique_lock<std::mutex> lock(this->mutex_);
            request.set_topic_name(topic);
            EndPoint* endpoint = request.mutable_endpoint();
            endpoint->set_ip(this->tcp_ip); // same as rpc ip, this is the ip of this node
            endpoint->set_port(this->rpc_port);
        }
        std::thread master_stream_thread_ = std::thread([this, request]() {
            ClientContext context;
            std::shared_ptr<grpc::ClientReader<SubscribeReply> > stream(
                this->nh_->stub_->Subscribe(&context, request));
            SubscribeReply response;
            while (stream->Read(&response)) {
                std::unique_lock<std::mutex> lock(this->mutex_);
                SubscriberRequest subscriber_request_;
                subscriber_request_.set_topic_name(this->topic_name);
                subscriber_request_.set_rate(this->rate);
                EndPoint* tcp_endpoint = subscriber_request_.mutable_tcp_endpoint();
                tcp_endpoint->set_ip(this->tcp_ip);
                tcp_endpoint->set_port(this->tcp_port);
                SubscriberReply subscriber_reply_;
                ClientContext subscriber_context_;
                std::unique_ptr<Connection::Stub> stub = 
                Connection::NewStub(
                    grpc::CreateChannel(response.endpoint().ip()+":"+std::to_string(response.endpoint().port()), 
                    grpc::InsecureChannelCredentials()));
                std::cout << "Receiving streaming message as Subscriber\n";
                Status status = stub->Subscriber(&subscriber_context_, subscriber_request_, &subscriber_reply_);
            }
        });
        master_stream_thread_.detach();
        /* Part III. start the spin handler thread */
        std::thread spin_thread_ = std::thread([this]() {
            while (true) {
                std::unique_lock<std::mutex> lock(this->spin_mutex_);
                spin_cv.wait(lock);
                {
                    std::unique_lock<std::mutex> lock_(this->queue_mutex_);
                    if (this->msgs_queue.size() > 0) {
                        this->cb_func(this->msgs_queue.back());
                    }
                }
            }
            
        });
        spin_thread_.detach();
    }
    template<class T>
    Publisher<T>::Publisher(std::string topic, NodeHandler *nh) :
        topic_name(topic), nh_(nh) {
        PublishRequest request;
        { 
            std::unique_lock<std::mutex> lock(nh_->mutex_);
            request.set_topic_name(topic);
            core::EndPoint* endpoint = request.mutable_endpoint();
            endpoint->set_ip(nh_->local_ip);
            endpoint->set_port(nh_->rpc_port);
        }
        std::thread master_stream_thread_ = std::thread([this, request]() {
            ClientContext context;
            std::shared_ptr<grpc::ClientReader<PublishReply> > stream(
                this->nh_->stub_->Publish(&context, request));
            PublishReply response;
            while (stream->Read(&response)) {
                PublisherRequest publisher_request_;
                publisher_request_.set_topic_name(this->topic_name);
                PublisherReply publisher_reply_;
                ClientContext publisher_context_;
                std::unique_ptr<Connection::Stub> stub = 
                Connection::NewStub(
                    grpc::CreateChannel(response.endpoint().ip()+":"+std::to_string(response.endpoint().port()), 
                    grpc::InsecureChannelCredentials()));
                Status status = stub->Publisher(&publisher_context_, publisher_request_, &publisher_reply_);
                
                /* Path 1 for create publisher client*/
                bool ret = false;
                std::shared_ptr<ClientSocket> c_sock = std::make_shared<ClientSocket>(ret);
                
                if (ret) {
                    ret = c_sock->Connect(publisher_reply_.tcp_endpoint().ip(), publisher_reply_.tcp_endpoint().port());
                    float freq = publisher_reply_.rate();
                    if (ret){
                        std::cout << "Successful Connected from publisher to subscriber " << publisher_reply_.tcp_endpoint().ip() << ":" << publisher_reply_.tcp_endpoint().port() << "\n";
                        std::thread publish_thread_ = std::thread([this, c_sock, freq]() {
                            Rate rate(freq);
                            while (1) {
                                std::string msg;
                                {
                                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                                    if (this->msg_queue.size() > 0)  {
                                        msg = this->msg_queue.back();
                                        if (!c_sock->Send(msg)) break;
                                    }
                                }
                                // c_sock->Send(msg);
                                rate.sleep();
                            }
                            c_sock->disconnect();
                        });
                        publish_thread_.detach();
                    }
                }
            }
        });
        master_stream_thread_.detach();
    }
    NodeHandler::NodeHandler() :
    stub_(Registration::NewStub(grpc::CreateChannel(std::string(getenv("CORE_MASTER_ADDR")), grpc::InsecureChannelCredentials()))) {
        signal(SIGPIPE, SIG_IGN);
        local_ip = std::string(getenv("CORE_LOCAL_IP")); // ip
        master_addr = std::string(getenv("CORE_MASTER_ADDR")); // 'ip:port'
        service = new ConnectionServiceImpl(this);
        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ServerBuilder builder;
        builder.AddListeningPort(local_ip + ":" + "0", grpc::InsecureServerCredentials(), &rpc_port);
        builder.RegisterService(service);
        server = std::unique_ptr<Server>(builder.BuildAndStart());
    }
}

#endif