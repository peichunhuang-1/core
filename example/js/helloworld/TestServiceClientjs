const ServiceClient = require('./../core/ServiceClient.js');
const protobuf = require('protobufjs');
const root = protobuf.loadSync(`${process.env.HOME}/Desktop/core/example/protos`+"/hello.proto");
function reply_call_back(reply) {
    console.log(reply);
}

const hello_clt = ServiceClient("hello", reply_call_back, root, 'hello.hellorequest', 'hello.helloreply');

intervalId = setInterval(() => {
    hello_clt.send({
        index: 101
    });
}, 1000);