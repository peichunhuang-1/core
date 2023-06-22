#include "NodeHandler.h"

namespace core
{
    NodeHandler::NodeHandler(std::string ip, uint16_t port, std::string local_ip, io_context &node_ios)
    : 
    _node_ios(node_ios), 
    stub_(Registration::NewStub(grpc::CreateChannel(ip+":"+std::to_string(port), grpc::InsecureChannelCredentials()))), 
    _local_ip(local_ip),
    _node_acceptor(node_ios, ip::tcp::endpoint(ip::address::from_string(local_ip), 0))
    {
        srand(time(NULL));
        _node_name = ip + ":" + std::to_string(port) + "#" + std::to_string(rand());
        _local_tcp_port = _node_acceptor.local_endpoint().port();
        std::string server_address = local_ip + ":" + "0";
        service = new TopicMessageServiceImpl(this);
        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(), &_local_http_port);
        builder.RegisterService(service);
        server = std::unique_ptr<Server>(builder.BuildAndStart());
    }
    Publisher NodeHandler::publisher(std::string topic_name) 
    {
        PublishRequest request;
        PublishReply reply;
        request.set_topic_name(topic_name);
        core::EndPoint* endpoint = request.mutable_endpoint();
        endpoint->set_node_name(_node_name);
        endpoint->set_node_ip(_local_ip);
        endpoint->set_node_port(_local_http_port);
        ClientContext context;
        Status status = stub_->Publish(&context, request, &reply);
        return Publisher(topic_name, this);
    }

    Subscriber NodeHandler::subscriber(std::string topic_name, uint32_t F) 
    {
        sock_ptr _sock(new ip::tcp::socket(_node_ios)); 
        this->_node_acceptor.async_accept(
            *_sock, 
            bind(&NodeHandler::accept_handler,this, placeholders::error, _sock, topic_name)
        );
        SubscribeRequest request;
        request.set_topic_name(topic_name);
        core::EndPoint* endpoint = request.mutable_endpoint();
        endpoint->set_node_name(_node_name);
        endpoint->set_node_ip(_local_ip);
        endpoint->set_node_port(_local_http_port);

        std::thread sub_stream_thread_ = std::thread([this, request, topic_name, F]() {
            ClientContext context;
            std::shared_ptr<grpc::ClientReader<core::SubscribeReply> > stream(
                this->stub_->Subscribe(&context, request));
            SubscribeReply response;
            while (stream->Read(&response)) {
                // std::lock_guard<std::mutex> lock(this->_mutex);
                {
                    TopicMessageRequest topic_request_;
                    topic_request_.set_topic_name(topic_name);
                    topic_request_.set_freq(F);
                    core::EndPointTCP* tcp_endpoint = topic_request_.mutable_tcp_endpoint();
                    tcp_endpoint->set_node_ip(this->_local_ip);
                    tcp_endpoint->set_node_port(this->_local_tcp_port);
                    TopicMessageReply topic_reply_;
                    ClientContext topic_context_;
                    std::unique_ptr<TopicMessage::Stub> stub = 
                    TopicMessage::NewStub(
                        grpc::CreateChannel(response.endpoint().node_ip()+":"+std::to_string(response.endpoint().node_port()), 
                        grpc::InsecureChannelCredentials()));
                    Status status = stub->Message(&topic_context_, topic_request_, &topic_reply_);
                }
            }
        });
        sub_stream_thread_.detach();
        return Subscriber(topic_name, F, this);
    }
    void NodeHandler::accept_handler(const boost::system::error_code &ec, sock_ptr sock, const std::string topic_name)
    {
        if (ec) return;
        sock->set_option(ip::tcp::no_delay(true));
        sock_ptr _sock(new ip::tcp::socket(_node_ios)); 
        this->_node_acceptor.async_accept(
            *_sock, 
            bind(&NodeHandler::accept_handler,this, placeholders::error, _sock, topic_name)
        );
        buffer_ptr new_buf(new std::string);
        async_read_until(
            *sock, boost::asio::dynamic_buffer(*new_buf), END_CODE, 
            boost::bind(&NodeHandler::read_handler, this, boost::asio::placeholders::error, sock, new_buf, boost::asio::placeholders::bytes_transferred, topic_name) 
        );
    }
    void NodeHandler::read_handler(const boost::system::error_code &ec, sock_ptr sock, buffer_ptr buf, size_t size_of_buf, const std::string topic_name)
    {
        if (ec) return;
        buffer_ptr new_buf(new std::string);
        async_read_until(
            *sock, boost::asio::dynamic_buffer(*new_buf), END_CODE, 
            boost::bind(&NodeHandler::read_handler, this, boost::asio::placeholders::error, sock, new_buf, boost::asio::placeholders::bytes_transferred, topic_name) 
        );
        // std::cout << "Enter Reader\n";
        std::string buffer_raw(*buf, 0, size_of_buf - END_CODE_LEN);
        if (buffer_raw.front() == START_BYTE)
        {
            std::string msg = buffer_raw.substr(1);
            std::lock_guard<std::mutex> lock(this->_mutex);
            subscribe_data[topic_name] = msg;
        }
    }
}