const Publisher = require('./../core/Publisher.js');
const protobuf = require('protobufjs');
const root = protobuf.loadSync(`${process.env.HOME}/Desktop/core/example/protos`+"/hello.proto");
const hello_proto = root.lookup('hello.hello');
const hello_pub = Publisher("hello", hello_proto);
hello_pub.pools.publish({
    stringField: "hello"
});