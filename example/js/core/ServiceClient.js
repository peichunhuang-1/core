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

function ServiceClient(service, on_reply, proto_root, proto_request, proto_reply) {
    rpc_client = new RegistrationDescriptor.core.Registration(`${process.env.CORE_MASTER_ADDR}`, grpc.credentials.createInsecure());
    const request_proto_handler = proto_root.lookup(proto_request);
    const reply_proto_handler = proto_root.lookup(proto_reply);
    let service_client;
    const request = {
        service_name : service
    };
    const reply_stream = rpc_client.ServiceClients(request, (err, reply) => {
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
    let node_to_node_client;
    reply_stream.on('data', (reply) => {
        console.log(reply);
        node_to_node_client = new ServiceServingDescriptor.core.ServerClient(reply.endpoint.ip+":"+reply.endpoint.port, grpc.credentials.createInsecure());
        console.log("bind to service server");
        service_client = node_to_node_client;
    });
    const on_call = (call_request) => {
        if (node_to_node_client) {
            const send_request = {
                service_name: service,
                payload: {
                    type_url: "type.googleapis.com/"+proto_request,
                    value: 
                    request_proto_handler.encode(call_request).finish()
                }
            };
            node_to_node_client.Serving(send_request, (err, reply) => {
                if (err) {
                    console.error('Error:', err);
                }
                else {
                    const reply_message = reply_proto_handler.decode(reply.payload.value);
                    on_reply(reply_message);
                }
            }
            );
        }
    };
    return {
        grpc_client: rpc_client,
        send: on_call
    };
}

module.exports = ServiceClient;