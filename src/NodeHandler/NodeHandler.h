#ifndef NODEHANDLER_HPP
#define NODEHANDLER_HPP

#define START_BYTE '\xff'
#define END_CODE "biorolab"
#define END_CODE_LEN 8

#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/any.pb.h>

#include "registration.grpc.pb.h"
#include "topic_message.grpc.pb.h"
#include "service_message.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace core
{
    using namespace boost::asio;
    typedef std::shared_ptr<ip::tcp::socket> sock_ptr;
    typedef std::shared_ptr<std::string> buffer_ptr;
    class Publisher;
    class Subscriber;
    class serviceServer;
    class serviceClient;
    class TopicMessageServiceImpl;
    class NodeHandler 
    {
        public: 
            NodeHandler(std::string master_ip, uint16_t port, std::string local_ip, io_context &node_ios);
            friend class Publisher;
            friend class Subscriber;
            friend class serviceServer;
            friend class serviceClient;
            friend class TopicMessageServiceImpl;
            Publisher publisher(std::string topic_name) ;
            Subscriber subscriber(std::string topic_name, uint32_t F) ;
            serviceServer servicepublisher(std::string service_name);
            serviceClient servicesubscriber(std::string service_name);
        protected:
            std::string master_ip_;
            uint16_t master_port_;
            std::mutex _mutex;
            std::string _local_ip;
            uint32_t _local_tcp_port;
            int _local_http_port;
            ip::tcp::acceptor _node_acceptor;
            std::string _node_name;
            std::unordered_map<std::string, std::string> subscribe_data;
            std::unordered_map<std::string, std::string> publish_data;
        private:
            std::unique_ptr<Registration::Stub> stub_;
            std::unique_ptr<Server> server;
            TopicMessageServiceImpl *service;
            io_context &_node_ios;
            std::thread node_thread_;
            void accept_handler(const boost::system::error_code &ec, sock_ptr sock, const std::string topic_name);
            void read_handler(const boost::system::error_code &ec, sock_ptr sock, buffer_ptr buf, size_t size_of_buf, const std::string topic_name);
    };
    class Subscriber
    {
        public:
        Subscriber(std::string topic_name, uint32_t freq, NodeHandler *nh)
        : topic_name_(topic_name), nh_(nh) {}
        template <class T>
        void spinOnce(void (*func)(T))
        {
            std::lock_guard<std::mutex> lock_guard(nh_->_mutex);
            if (nh_->subscribe_data.find(topic_name_) != nh_->subscribe_data.end())
            {
                T msg;
                msg.ParseFromString(nh_->subscribe_data[topic_name_]);
                update_ = true;
                func(msg);
            }
        }
        template <class T>
        void spinOnce(std::function<void(T)> func)
        {
            std::lock_guard<std::mutex> lock_guard(nh_->_mutex);
            if (nh_->subscribe_data.find(topic_name_) != nh_->subscribe_data.end())
            {
                T msg;
                msg.ParseFromString(nh_->subscribe_data[topic_name_]);
                update_ = true;
                func(msg);
            }
        }
        bool update() 
        {
            std::lock_guard<std::mutex> lock_guard(nh_->_mutex);
            return update_;
        }
        private:
        NodeHandler *nh_;
        std::string topic_name_;
        bool update_ = false;
    };
    class Publisher
    {
        public:
        Publisher(std::string topic_name, NodeHandler *nh)
        : topic_name_(topic_name), nh_(nh) {}
        template <class T>
        void publish(T msg)
        {
            std::string str_msg;
            msg.SerializeToString(&str_msg);
            // std::cout << "Try publish lock\n";
            std::lock_guard<std::mutex> lock(nh_->_mutex);
            nh_->publish_data[topic_name_] = (START_BYTE+str_msg+END_CODE);
            // std::cout << "publish unlock\n";
        }
        private:
        NodeHandler *nh_;
        std::string topic_name_;
    };
    class serviceServer
    {
        public:
            serviceServer(std::string service_name, NodeHandler* nh) 
            : service_name_(service_name), nh_(nh), context(std::make_shared<ClientContext>())
            {
                stub_ = 
                ServiceMessage::NewStub(
                grpc::CreateChannel(nh_->master_ip_+":"+std::to_string(nh_->master_port_), 
                grpc::InsecureChannelCredentials()));
                writer_ = std::shared_ptr<grpc::ClientWriter<ServiceServerMessageRequest> >(
                stub_->ServiceServer(context.get(), &reply));
            }
            ~serviceServer() 
            {
                writer_->WritesDone();
                writer_->Finish();
            }
            template<class T>
            void send(T msg)
            {
                ServiceServerMessageRequest request;
                request.set_service_name(service_name_);
                google::protobuf::Any data_any;
                data_any.PackFrom(msg);
                *request.mutable_data() = data_any;
                writer_->Write(request);
            }
        private:
            std::string service_name_;
            NodeHandler *nh_;
            std::shared_ptr<grpc::ClientWriter<ServiceServerMessageRequest>> writer_;
            std::shared_ptr<ServiceMessage::Stub> stub_;
            ServiceServerMessageReply reply; 
            std::shared_ptr<ClientContext> context;
    };
    class serviceClient
    {
        public:
            serviceClient(std::string service_name, NodeHandler* nh) 
            : service_name_(service_name), nh_(nh), context(std::make_shared<ClientContext>()), mutex_(std::make_shared<std::mutex>())
            {
                stub_ = 
                ServiceMessage::NewStub(
                grpc::CreateChannel(nh_->master_ip_+":"+std::to_string(nh_->master_port_), 
                grpc::InsecureChannelCredentials()));
                ServiceClientMessageRequest request;
                request.set_service_name(service_name);
                reader_ = std::shared_ptr<grpc::ClientReader<ServiceClientMessageReply>>(
                stub_->ServiceClient(context.get(), request));
                receive_thread_ = std::thread(  
                    [this] ()
                    {
                        ServiceClientMessageReply reply;
                        while(reader_->Read(&reply))
                        {
                            std::lock_guard<std::mutex> lock(*mutex_);
                            receive_data = reply.data();
                            this->update_ = true;
                        }
                    }
                );
            }
            template<class T>
            void  get(T &get_data)
            {
                std::lock_guard<std::mutex> lock(*mutex_);
                receive_data.UnpackTo(&get_data);
            }
            bool update() {return update_;}
        private:
            std::string service_name_;
            NodeHandler *nh_;
            std::shared_ptr<grpc::ClientReader<ServiceClientMessageReply>> reader_;
            google::protobuf::Any receive_data; 
            std::shared_ptr<ServiceMessage::Stub> stub_;
            std::shared_ptr<std::mutex> mutex_;
            std::shared_ptr<ClientContext> context;
            std::thread receive_thread_;
            bool update_ = false;
    };
    class TopicMessageServiceImpl final : public TopicMessage::Service {
        public:
        TopicMessageServiceImpl(NodeHandler *nh) : nh_(nh) {}
        Status Message(ServerContext* context, const TopicMessageRequest* request,
                  TopicMessageReply* reply) override
        {
            std::string topic_name = request->topic_name();
            uint32_t sleep_us =  1e6 / request->freq();
            std::string to_node_ip = request->tcp_endpoint().node_ip();
            uint32_t to_port = request->tcp_endpoint().node_port();
            sock_ptr sock(new ip::tcp::socket(nh_->_node_ios));
            boost::system::error_code ec;
            sock->connect(ip::tcp::endpoint
                (ip::address::from_string(to_node_ip), to_port), ec
            );
            sock->set_option(ip::tcp::no_delay(true));
            if (ec) std::cout << "Connection Failed: " << ec.message() << "\n";
            std::thread publish_thread(
                [this, sock, topic_name, sleep_us]()
                {
                    steady_timer timer(nh_->_node_ios);
                    while (1)
                    {
                        std::lock_guard<std::mutex> lock(nh_->_mutex);
                        if (nh_->publish_data.find(topic_name) != nh_->publish_data.end()) break;
                        usleep(10000);
                    }
                    while (1)
                    {
                        timer.expires_from_now(std::chrono::microseconds(sleep_us));
                        try
                        {
                            write(*sock, boost::asio::buffer(nh_->publish_data[topic_name]));
                        }
                        catch(const std::exception& e)
                        {
                            std::cerr << e.what() << '\n';
                            break;
                        }
                        timer.wait();
                    }
                }
            );
            publish_thread.detach();
            return Status::OK;
        }
        private:
            NodeHandler* nh_;
    };
}

#endif