#include "session_auth.h"

bool SessionAuth::isAuthorized(const grpc::ServerContext* context) const {
    if (secret_.empty()) {
        return true;
    }
    if (context == nullptr) {
        return false;
    }
    const auto& md = context->client_metadata();
    const auto it = md.find("x-auth-token");
    if (it == md.end()) {
        return false;
    }
    return isAuthorizedToken(std::string(it->second.data(), it->second.length()));
}

bool SessionAuth::isAuthorizedToken(const std::string& token) const {
    if (secret_.empty()) {
        return true;
    }
    return token == secret_;
}
