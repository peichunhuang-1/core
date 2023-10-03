# core
This project is a light-weight publisher-subscriber communicate protocol based on gRPC. 

# preliminary
Install gRPC. 

# compile
    $ cd ${worker space}/core 
    $ mkdir build 
    $ cd build 
    $ cmake .. -DCMAKE_PREFIX_PATH=${grpc-install-prefix} -DCMAKE_INSTALL_PREFIX=${grpc-install-prefix} 
    $ make -j8 
    $ make install 

# local environment setting

    $ export PATH="${worker space}/bin:$PATH" 
    $ echo export CORE_LOCAL_IP="127.0.0.1" >> ~/.bashrc 
    $ echo export CORE_MASTER_ADDR="127.0.0.1:10010" >> ~/.bashrc 
    $ echo export PROTO_PATH=/${HOME}/.local/include >> ~/.bashrc

You can also choose your setting bash file. **"CORE_LOCAL_IP"** should be your local device IP, and **"CORE_MASTER_ADDR"** should be set to same as your local/remote **NodeCore** 
launched device.

For example, if you have device A (**192.168.0.106**) and device B (**192.168.0.172**). And you run your core master node on device A on port **10010**, then on each device A and B, the 
**"CORE_MASTER_ADDR"** should be **"192.168.0.106:10010"**, and **"CORE_LOCAL_IP"** on device A is **"192.168.0.106"**, **"CORE_LOCAL_IP"** on device B is **"192.168.0.172"**. 

# example compile
    $ cd ${worker space}/core/example/c++/helloworld 
    $ mkdir build 
    $ cd build 
    $ cmake .. -DCMAKE_PREFIX_PATH=${grpc-install-prefix} 

For me ${grpc-install-prefix} is /${HOME}/.local

# run
    $ source ~/.bashrc 
    $ NodeCore 
    $ ./NodeTestPub // terminal 1
    $ ./NodeTestSub // terminal 2

This is the basic Publisher/Subscriber protocol, it support multiple subscribers subscribe to one topic, and also multiple publishers publish to a topic is legal but not recommended.

    $ NodeCore 
    $ ./NodeTestServiceServer // terminal 1
    $ ./NodeTestServiceClient // terminal 2

This is the basic ServiceServer/Client protocol, if you launch multiple Server on one service, only the last one works functionally.

# JavaScript preliminary

First you should install node and npm.

    $ cd ${worker space}/core/example/js
    $ npm init
    $ npm install @grpc/grpc-js
    $ npm install protobufjs
    
# JavaScript example
    $ NodeCore
    $ cd ${worker space}/core/example/js/helloworld 
    $ node TestPublisher // terminal 1 or you can also use c++ helloworld example publisher with js subscriber
    $ node TestSubscriber // terminal 2 or you can also use c++ helloworld example subscriber with js publisher
    $ node TestServiceServer // terminal 3 ...
    $ node TestServiceClient // terminal 4 ...

All these node should be able to cooperate with c++ program. if you got some error, it might mostly because the message definition. The js message with _{alphabet} would become upper case in nested structure. For exmaple if you use hello.hello: 
string_field in js declaration should be : **{stringField: "hello"}** instead of : **{string_field: "hello"}**

    
