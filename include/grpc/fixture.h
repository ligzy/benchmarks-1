#pragma once

#ifndef FIXTURE_H_EQKIMOI3
#define FIXTURE_H_EQKIMOI3

#include <memory>

#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/security/server_credentials.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "benchmark/benchmark.h"

#include "grpc/grpc_service.grpc.pb.h"
#include "grpc/struct_helpers.h"

#include "target_code.h"


static constexpr uint16_t grpc_port = 8084;

class grpc_service : public grpc_code::GrpcServiceBenchmark::Service {
public:
  ::grpc::Status get_answer(::grpc::ServerContext *context,
                            const grpc_code::AnswerRequest *request,
                            grpc_code::AnswerReply *response) override {
    auto num = request->number();
    response->set_result(::get_answer(num));
    return ::grpc::Status::OK;
  }
  ::grpc::Status get_blob(::grpc::ServerContext *context,
                          const grpc_code::EmptyRequest *request,
                          grpc_code::BlobResponse *response) override {
    response->set_data(::get_blob(blob_size_));
    return ::grpc::Status::OK;
  }
  ::grpc::Status
  get_structs(::grpc::ServerContext *context,
              const grpc_code::EmptyRequest *request,
              grpc_code::StudentDataResponse *response) override {
    auto &students = grpc_code::get_structs();
    for (auto &s : students) {
      auto new_s = response->add_students();
      // TODO: is there a more efficient way (to avoid copies?)
      new_s->CopyFrom(s);
    }
    return ::grpc::Status::OK;
  }
  int blob_size_;
};

class grpc_bench : public benchmark::Fixture {
public:
  grpc_bench()
      : channel_(grpc::CreateChannel(server_addr_,
                                     grpc::InsecureChannelCredentials())),
        client_(channel_) {
    grpc::ServerBuilder b;
    b.AddListeningPort(server_addr_, grpc::InsecureServerCredentials());
    b.RegisterService(&service_impl_);
    server_ = b.BuildAndStart();
  }

  void get_answer(int param) {
    (void)param;
    grpc::ClientContext client_context;
    grpc_code::AnswerRequest request;
    grpc_code::AnswerReply response;
    request.set_number(23);
    auto status = client_.get_answer(&client_context, request, &response);
    int a;
    benchmark::DoNotOptimize(a = response.result());
  }

  void get_blob(int param) {
    service_impl_.blob_size_ = param;
    grpc::ClientContext client_context;
    grpc_code::EmptyRequest request;
    grpc_code::BlobResponse response;
    auto status = client_.get_blob(&client_context, request, &response);
    std::string s;
    benchmark::DoNotOptimize(s = response.data());
    std::size_t size;
    benchmark::DoNotOptimize(size = s.size());
  }

  void get_structs(int param) {
    (void)param;
    grpc::ClientContext client_context;
    grpc_code::EmptyRequest request;
    grpc_code::StudentDataResponse response;
    auto status = client_.get_structs(&client_context, request, &response);
    std::size_t count;
    benchmark::DoNotOptimize(count = response.students_size());
  }

  // this variant insist on getting a std::vector after the call.
  // The only way to get it with grpc is to copy the elements.
  // The protobuf-generated repeated field is already iterable,
  // so most real-life user code will just use that.
  void get_structs_strict(int param) {
    (void)param;
    grpc::ClientContext client_context;
    grpc_code::EmptyRequest request;
    grpc_code::StudentDataResponse response;
    std::vector<grpc_code::Student> students;
    auto status = client_.get_structs(&client_context, request, &response);
    for (auto s : response.students()) {
      students.push_back(s);
    }
    std::size_t count;
    benchmark::DoNotOptimize(count = students.size());
  }

  ~grpc_bench() noexcept { server_->Shutdown(); }

  std::shared_ptr<grpc::ChannelInterface> channel_;
  grpc_code::GrpcServiceBenchmark::Stub client_;
  std::unique_ptr<grpc::Server> server_;
  grpc_service service_impl_;
  static constexpr const char *server_addr_ = "localhost:8082";
};

#endif /* end of include guard: FIXTURE_H_EQKIMOI3 */
