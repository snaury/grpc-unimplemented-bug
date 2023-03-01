#!/bin/sh
set -e
protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` hello.proto
protoc --cpp_out=. hello.proto
clang++ hello-server.cpp hello.grpc.pb.cc hello.pb.cc -std=c++17 -o hello-server -I/opt/homebrew/include -L/opt/homebrew/lib -lgrpc++ -lgrpc -lgpr -lprotobuf
clang++ hello-client.cpp hello.grpc.pb.cc hello.pb.cc -std=c++17 -o hello-client -I/opt/homebrew/include -L/opt/homebrew/lib -lgrpc++ -lgrpc -lgpr -lprotobuf
