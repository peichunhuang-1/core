const proto_Path = `${process.env.PROTO_PATH}`;
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const net = require('net');
const Registration = protoLoader.loadSync(proto_Path+"/registration.proto", {
    keepCase: true,
    defaults: true,
    oneofs: true,
});
const RegistrationDescriptor = grpc.loadPackageDefinition(Registration);

const Connection = protoLoader.loadSync(proto_Path+"/connection.proto", {
    keepCase: true,
    defaults: true,
    oneofs: true,
});
const ConnectionDescriptor = grpc.loadPackageDefinition(Connection);


function Subscriber(topic, freq, cb_func, pb_handler) {
    const client = new RegistrationDescriptor.core.Registration(`${process.env.CORE_MASTER_ADDR}`, grpc.credentials.createInsecure());
    let rpc_port;
    let tcp_server;
    tcp_server = net.createServer((socket) => {
        console.log('Client connected:', socket.remoteAddress, socket.remotePort);
        socket.on('data', (data) => {
            const messageLength = data.readUInt32LE(0);
            const messageData = data.slice(4, 4 + messageLength);
            try {
                const decodedMessage = pb_handler.decode(messageData);
                cb_func(decodedMessage);
            } catch (error) {
                console.error('Error decoding message:', error);
            }
        });
        socket.on('end', () => {
            console.log('Client disconnected.');
        });
    });
    tcp_server.listen(0, `${process.env.CORE_LOCAL_IP}`, () => {
    });
    const server = new grpc.Server();
    server.addService(ConnectionDescriptor.core.Connection.service, 
        {
            Subscriber :  (call, callback) => {
                    const response = {
                    };
                    console.log(call.request);
                    callback(null, response);
            },
            Publisher :  (call, callback) => {
                    console.log(call.request);

                    const response = {
                        topic_name : topic,
                        tcp_endpoint : {ip: `${process.env.CORE_LOCAL_IP}`, port : tcp_server.address().port},
                        rate : freq,
                    };
                    callback(null, response);
            }
        }
    );

    server.bindAsync(`${process.env.CORE_LOCAL_IP}:0`, grpc.ServerCredentials.createInsecure(), (err, port) => {
        if (err) {
        console.error('Server bind failed:', err);
        return;
        }
        server.start();
        rpc_port = port;
        console.log('Server is listening on port:', rpc_port);
        const request = {
            endpoint : {
                ip : `${process.env.CORE_LOCAL_IP}`,
                port : rpc_port
            },
            topic_name : topic
        };
        const reply_stream = client.Subscribe(request, (err, reply) => {
            if (err) {
                console.error('Error:', error);
                return;
            }
        });
        reply_stream.on('error', (err) => {
            console.error('Error:', err);
        });
        reply_stream.on('end', () => {
            console.log('Reply Stream Ended');
        });
        reply_stream.on('data', (reply) => {
            console.log(reply);
            const node_to_node_client = new ConnectionDescriptor.core.Connection(reply.endpoint.ip+":"+reply.endpoint.port, grpc.credentials.createInsecure());
            const from_subscriber_request = {
                tcp_endpoint : {
                    ip : `${process.env.CORE_LOCAL_IP}`,
                    port : tcp_server.address().port,
                },
                topic_name : topic,
                rate : freq
            };
            node_to_node_client.Subscriber(from_subscriber_request, (err, reply) => {
                if (err) {
                    console.error('Error:', error);
                    return;
                } else {
                    console.log(reply);
                }
            });
        });
    });
    
    return {
        rpc_clent: client,
        tcp_acceptor: tcp_server,
        grpc_server: server
    };
}

module.exports = Subscriber;