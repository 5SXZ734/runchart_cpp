#pragma once

#include <string>

#include <grpcpp/grpcpp.h>

class SessionAuth {
public:
    explicit SessionAuth(std::string secret) : secret_(std::move(secret)) {}
    bool isAuthorized(const grpc::ServerContext* context) const;
    bool isAuthorizedToken(const std::string& token) const;

private:
    std::string secret_;
};
