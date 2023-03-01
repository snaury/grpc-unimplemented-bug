#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <cstdlib>

#include <grpcpp/grpcpp.h>

#include "hello.grpc.pb.h"

template<class Service>
void RunClient(std::atomic<int>& okcounter, std::atomic<int>& failcounter) {
    auto channel = grpc::CreateChannel("127.0.0.1:50051", grpc::InsecureChannelCredentials());
    auto stub = Service::NewStub(channel);
    int next_token = 1;
    while (true) {
        hello::HelloRequest request;
        request.set_token(next_token++);
        hello::HelloResponse response;
        grpc::ClientContext context;
        grpc::Status status = stub->Hello(&context, request, &response);
        if (status.ok()) {
            okcounter++;
        } else {
            if (0 == failcounter++) {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            }
        }
    }
}

template<class Service>
void RunClients(int threadsCount) {
    using namespace std::chrono_literals;

    std::atomic<int> okcounter{ 0 };
    std::atomic<int> failcounter{ 0 };

    std::vector<std::thread> threads;
    for (int i = 0; i < threadsCount; ++i) {
        threads.emplace_back([&]{
            RunClient<Service>(okcounter, failcounter);
        });
    }

    while (true) {
        std::this_thread::sleep_for(1000ms);
        int okcount = okcounter.exchange(0);
        int failcount = failcounter.exchange(0);
        std::cout << okcount << " ok/s " << failcount << " fail/s" << std::endl;
    }
}

int main(int argc, char** argv) {
    int threads = 10;
    if (argc > 2) {
        threads = atoi(argv[2]);
    }
    if (argc > 1) {
        std::string mode(argv[1]);
        if (mode == "normal") {
            RunClients<hello::HelloService>(threads);
            return 0;
        }
        if (mode == "unknown") {
            RunClients<hello::UnknownService>(threads);
            return 0;
        }
    }
    std::cerr << "Usage: " << argv[0] << " {normal|unknown} [threads]" << std::endl;
    return 1;
}
