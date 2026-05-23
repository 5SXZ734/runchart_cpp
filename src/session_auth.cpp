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
    return std::string(it->second.data(), it->second.length()) == secret_;
}
