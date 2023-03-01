#include <vector>
#include <thread>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <grpcpp/grpcpp.h>

#include "hello.grpc.pb.h"

class ServerImpl final {
public:
    void Run(int threadsCount) {
        grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:50051", grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        std::vector<std::thread> threads;
        for (int i = 0; i < threadsCount; ++i) {
            threads.emplace_back([this]{
                this->RunWorker();
            });
        }

        // Start a normal handler
        (new HelloHandler(&service_, cq_.get()))->Start();

        std::cout << "Accepting requests" << std::endl;

        // Join threads, will never return
        for (auto& thread : threads) {
            thread.join();
        }
    }

private:
    class ICompletionTag {
    protected:
        ~ICompletionTag() = default;

    public:
        virtual void Complete(bool ok) = 0;
    };

    class HelloHandler : public ICompletionTag {
        enum class EState {
            Initial,
            WaitRequest,
            SendResponse,
        };

    public:
        HelloHandler(hello::HelloService::AsyncService* service, grpc::ServerCompletionQueue* cq)
            : service_(service)
            , cq_(cq)
            , writer_(&context_)
        { }

        void Start() {
            assert(state_ == EState::Initial);
            service_->RequestHello(&context_, &request_, &writer_, cq_, cq_, GetWaitRequestTag());
        }

    private:
        void Complete(bool ok) override {
            switch (state_) {
                case EState::WaitRequest: {
                    // This example doesn't handle errors
                    assert(ok);

                    // Accept another request
                    Clone();

                    response_.set_token(request_.token());
                    writer_.Finish(response_, grpc::Status::OK, GetSendResponseTag());
                    return;
                }
                case EState::SendResponse: {
                    delete this;
                    return;
                }
                default: {
                    assert(false);
                }
            }
        }

    private:
        void Clone() {
            (new HelloHandler(service_, cq_))->Start();
        }

    private:
        ICompletionTag* GetWaitRequestTag() {
            state_ = EState::WaitRequest;
            return this;
        }

        ICompletionTag* GetSendResponseTag() {
            state_ = EState::SendResponse;
            return this;
        }

    private:
        hello::HelloService::AsyncService* service_;
        grpc::ServerCompletionQueue* cq_;
        EState state_ = EState::Initial;
        grpc::ServerContext context_;
        hello::HelloRequest request_;
        grpc::ServerAsyncResponseWriter<hello::HelloResponse> writer_;
        hello::HelloResponse response_;
    };

private:
    void RunWorker() {
        void* tag;
        bool ok;
        while (cq_->Next(&tag, &ok)) {
            static_cast<ICompletionTag*>(tag)->Complete(ok);
        }
    }

private:
    hello::HelloService::AsyncService service_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    std::unique_ptr<grpc::Server> server_;
};

int main(int argc, char** argv) {
    int threads = 2;
    if (argc > 1) {
        threads = atoi(argv[1]);
    }
    ServerImpl server;
    server.Run(threads);
    return 0;
}
