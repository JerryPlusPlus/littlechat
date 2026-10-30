#pragma once
#include "chat.pb.h"

namespace littlechat
{
namespace protocol
{
enum class RequestValidation
{
    Valid,
    Malformed,
    Unauthenticated,
    Unsupported
};
// The P2 endpoint has no authenticated user. Client-provided identity is never accepted.
RequestValidation validateDiagnosticRequest(const wire::Envelope &request);
bool parseEnvelope(const std::string &payload, wire::Envelope &message);
bool serializeEnvelope(const wire::Envelope &message, std::string &frame);
} // namespace protocol
} // namespace littlechat
