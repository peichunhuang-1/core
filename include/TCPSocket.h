#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#ifndef IPTOS_LOWDELAY
#define IPTOS_LOWDELAY 0x10
#endif

/* specific for protobuf sending */
namespace core{
    template<class T>
    class ServerSocket {
        public:
        ServerSocket() {}
        ServerSocket(int sock) : socket_(sock) {}
        inline bool SocketHandler(T &msg) {
            char buffer[4];
            int bytecount=0;
            memset(buffer, '\0', 4);
            if((bytecount = recv(this->socket_, buffer, 4, MSG_PEEK))== -1){
                std::cerr << "Error receiving data\n";
                return false;
            }
            else if (bytecount == 0) {
                std::cerr << "Error receiving empty data\n";
                return false;
            }
            // std::cout << "receving recv\n";
            return readBody(msg, readHdr(buffer));
        }
        void disconnect() {
            int result = close(this->socket_);
            if (result == 0) {
                std::cout << "Successfully close socket\n";
            } else {
                std::cerr << "Error closing socket\n";
            }
        }
        private:
        int socket_;
        inline google::protobuf::uint32 readHdr(char* buf) {
            google::protobuf::uint32 size;
            google::protobuf::io::ArrayInputStream ais(buf,4);
            google::protobuf::io::CodedInputStream coded_input(&ais);
            coded_input.ReadLittleEndian32(&size);
            return size;
        }
        inline bool readBody(T &payload, google::protobuf::uint32 siz) {
            int bytecount;
            // char buffer [siz+4];
            std::vector<char> buffer(siz+4);
            if((bytecount = recv(this->socket_, (void *)buffer.data(), 4+siz, MSG_WAITALL))== -1){
                std::cerr << "Error receiving data\n";
                return false;
            }
            google::protobuf::io::ArrayInputStream ais(buffer.data(),siz+4);
            google::protobuf::io::CodedInputStream coded_input(&ais);
            coded_input.ReadLittleEndian32(&siz);
            google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(siz);
            payload.ParseFromCodedStream(&coded_input);
            coded_input.PopLimit(msgLimit);
            return true;
        }
    };
    class ClientSocket {
        public:
        ClientSocket() {}
        ClientSocket(bool &ret) {
            this->socket_ = socket(AF_INET, SOCK_STREAM, 0);
            if(this->socket_ == -1){
                std::cerr << "Error create TCP client.\n";
            }
            int* sockopt_ptr = (int*)malloc(sizeof(int));
            *sockopt_ptr = 1;
            int tos_value = IPTOS_LOWDELAY;
            int sndbuf_size = 8. * 1024; // 1 MB
            int rcvbuf_size = 8. * 1024; // 1 MB
            if( (setsockopt(this->socket_, SOL_SOCKET, SO_REUSEADDR, (char*)sockopt_ptr, sizeof(int)) == -1 )||
            (setsockopt(this->socket_, SOL_SOCKET, SO_KEEPALIVE, (char*)sockopt_ptr, sizeof(int)) == -1 ) ||
            (setsockopt(this->socket_, IPPROTO_TCP, TCP_NODELAY, (char*)sockopt_ptr, sizeof(int)) == -1 ) ||
            (setsockopt(this->socket_, IPPROTO_IP, IP_TOS, &tos_value, sizeof(tos_value)) == -1) || 
            (setsockopt(this->socket_, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(sndbuf_size)) == -1) ||
            (setsockopt(this->socket_, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)) == -1)) {
                std::cerr << "Error setting TCP client.\n";
                ret = false;
                return;
            }
            ret = true;
        }
        void disconnect() {
            int result = close(this->socket_);
            if (result == 0) {
                std::cout << "Successfully close socket\n";
            } else {
                std::cerr << "Error closing socket\n";
            }
        }
        inline bool Connect(std::string server_ip, uint32_t server_port) {
            sockaddr_in server_addr;
            server_addr.sin_family = AF_INET ;
            server_addr.sin_port = htons(server_port);
            memset(&(server_addr.sin_zero), 0, 8);
            server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
            if( connect( this->socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == -1 ){
                std::cerr << "Error connecting to TCP server.\n";
                return false;
            }
            return true;
        }
        inline bool Send(std::string data) {
            int bytecount;
            try {
                bytecount = send(this->socket_, data.c_str(), data.length(), 0);
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << "\n";
                bytecount = -1;
            }
            if( bytecount == -1 ) {
                std::cerr << "Error Publishing.\n";
                return false;
            }
            return true;
        }
        private:
        int socket_;
    };
    class AcceptorSocket {
        public:
        AcceptorSocket() {}
        AcceptorSocket(std::string server_ip, uint32_t &server_port, bool &ret) {
            this->socket_ = socket(AF_INET, SOCK_STREAM, 0);
            if (this->socket_ < 0) {
                std::cerr << "Error creating tcp socket.\n";
                ret = false;
            }
            int* sockopt_ptr = (int*)malloc(sizeof(int));
            *sockopt_ptr = 1;
            int tos_value = IPTOS_LOWDELAY;
            int sndbuf_size = 8. * 1024; // 1 MB
            int rcvbuf_size = 8. * 1024; // 1 MB
            if( (setsockopt(this->socket_, SOL_SOCKET, SO_REUSEADDR, (char*)sockopt_ptr, sizeof(int)) == -1 )||
            (setsockopt(this->socket_, SOL_SOCKET, SO_KEEPALIVE, (char*)sockopt_ptr, sizeof(int)) == -1 ) ||
            (setsockopt(this->socket_, IPPROTO_TCP, TCP_NODELAY, (char*)sockopt_ptr, sizeof(int)) == -1 ) ||
            (setsockopt(this->socket_, IPPROTO_IP, IP_TOS, &tos_value, sizeof(tos_value)) == -1) || 
            (setsockopt(this->socket_, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(sndbuf_size)) == -1) ||
            (setsockopt(this->socket_, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)) == -1)){
                std::cerr << "Error setting TCP server.\n";
                ret = false;
            }
            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(0);
            serverAddr.sin_addr.s_addr = inet_addr(server_ip.c_str());
            if (::bind(this->socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                std::cerr << "Error binding.\n";
                close(this->socket_);
                ret = false;
            }
            sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            if (getsockname(this->socket_, (sockaddr*)&addr, &addr_len) == 0) {
                server_port = ntohs(addr.sin_port);
            }
            if(listen(this->socket_, 10) == -1 ){
                std::cerr << "Error Listening.\n";
                ret = false;
            }
            ret = true;
        }
        void disconnect() {
            int result = close(this->socket_);
            if (result == 0) {
                std::cout << "Successfully close acceptor socket\n";
            } else {
                std::cerr << "Error closing acceptor socket\n";
            }
        }
        inline bool Accept(int &s_sock) {
            int sock;
            socklen_t addr_size = sizeof(sockaddr_in);
            sockaddr_in sadr;
            if((sock = accept( this->socket_, (sockaddr*)&sadr, &addr_size))!= -1) {
                s_sock = sock;
                return true;
            }
            std::cerr << "Error Accepting.\n";
            return false;
        }
        private:
        int socket_;
    };
}

#endif