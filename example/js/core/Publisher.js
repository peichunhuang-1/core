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

function Publisher(topic, pb_handler) {
    class TCP_Clients {
        constructor (pb_handler) {
            this.data_pool = [];
            this.clients = new Map();
            this.pb_handler = pb_handler;
        }
        add_client(freq, port, ip) {
            const client = new net.Socket();
            const intervalTime = (1000.0 / freq >= 1) ? (1000 / freq) : 1;
            let intervalId;
            client.connect(port, ip, () =>{
                intervalId = setInterval(() => {
                    if (this.data_pool.length > 0){
                        const lastData = this.data_pool[this.data_pool.length - 1];
                        if (lastData) {
                            client.write(lastData);
                        } else {
                            console.log(lastData);
                            console.error('Last data in this.data_pool is undefined.');
                        }
                    }
                }, intervalTime);
            });
            client.on('error', (err) => {
                clearInterval(intervalId);
                this.clients.delete(intervalId);
                console.error('Client error:', err);
            });
            this.clients.set(intervalId, client);
        }
        publish(msg) {
            console.log(msg);
            if (this.data_pool.length > 10) {
                this.data_pool.pop();
            }
            const buffer = this.pb_handler.encode(msg).finish();
            const messageLength = buffer.length;
            const prefixedBuffer = Buffer.alloc(4 + messageLength);
            prefixedBuffer.writeUInt32LE(messageLength, 0);
            buffer.copy(prefixedBuffer, 4);
            this.data_pool.push(prefixedBuffer);
        }
    };
    const tcp_clients = new TCP_Clients(pb_handler);
    const client = new RegistrationDescriptor.core.Registration(`${process.env.CORE_MASTER_ADDR}`, grpc.credentials.createInsecure());
    let rpc_port;
    const server = new grpc.Server();
    server.addService(ConnectionDescriptor.core.Connection.service, 
        {
            Subscriber :  (call, callback) => {
                    const response = {
                    };
                    console.log(call.request);
                    tcp_clients.add_client(call.request.rate, call.request.tcp_endpoint.port, call.request.tcp_endpoint.ip);
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
        const reply_stream = client.Publish(request, (err, reply) => {
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
            const from_publisher_request = {
                topic_name : topic
            };
            node_to_node_client.Publisher(from_publisher_request, (err, reply) => {
                if (err) {
                    console.error('Error:', error);
                    return;
                } else {
                    tcp_clients.add_client(reply.rate, reply.tcp_endpoint.port, reply.tcp_endpoint.ip);
                    console.log(reply);
                }
            });
        });
    });
    
    return {
        rpc_clent: client,
        grpc_server: server,
        pools: tcp_clients
    };
}

module.exports = Publisher;