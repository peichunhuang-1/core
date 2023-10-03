const proto_Path = `${process.env.PROTO_PATH}`;
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const Registration = protoLoader.loadSync(proto_Path+"/registration.proto", {
    keepCase: true,
    defaults: true,
    oneofs: true,
});
const RegistrationDescriptor = grpc.loadPackageDefinition(Registration);

const ServiceServing = protoLoader.loadSync(proto_Path+"/serviceserving.proto", {
    keepCase: true,
    defaults: true,
    oneofs: true,
});
const ServiceServingDescriptor = grpc.loadPackageDefinition(ServiceServing);

function ServiceServer(service, server_cb, proto_root, proto_request, proto_reply) {
    rpc_client = new RegistrationDescriptor.core.Registration(`${process.env.CORE_MASTER_ADDR}`, grpc.credentials.createInsecure());
    rpc_server = new grpc.Server();
    const request_proto_handler = proto_root.lookup(proto_request);
    const reply_proto_handler = proto_root.lookup(proto_reply);
    rpc_server.addService(ServiceServingDescriptor.core.ServerClient.service, 
        {
            Serving:  (call, callback) => {
                    console.log(call.request);
                    const response_payload = reply_proto_handler.encode(server_cb(request_proto_handler.decode(call.request.payload.value))).finish();
                    const response = {payload:{
                        type_url: 'type.googleapis.com/' + proto_reply,
                        value: response_payload
                    } };
                    callback(null, response);
            }
        }
    );
    rpc_server.bindAsync(`${process.env.CORE_LOCAL_IP}:0`, grpc.ServerCredentials.createInsecure(), (err, port) => {
        if (err) {
        console.error('Server bind failed:', err);
        return;
        }
        rpc_server.start();
        const rpc_port = port;
        const request = {
            endpoint: {ip: `${process.env.CORE_LOCAL_IP}`,
                        port: rpc_port},
            service_name: service
        };
        rpc_client.ServiceServers(request, (err, reply) => {
            if (err) {
                console.error('Error:', error);
                return;
            }
        });
        }
    );
    return {
        grpc_client: rpc_client,
        grpc_server: rpc_server
    };
}

module.exports = ServiceServer;