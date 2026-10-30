#include "littlechat/protocol/envelope.h"
#include "littlechat/protocol/frame.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace littlechat
{
namespace protocol
{
RequestValidation validateDiagnosticRequest(const wire::Envelope &request)
{
    if (!wire::MessageType_IsValid(request.type()) || request.type() == wire::MESSAGE_TYPE_UNSPECIFIED ||
        request.request_id() == 0 || request.error_code() != wire::OK || request.timestamp_ms() < 0 ||
        (!request.message_id().empty() && request.message_id().size() != 16))
        return RequestValidation::Malformed;
    if (request.sender_id() != 0 || request.receiver_id() != 0 || request.conversation_id() != 0)
        return RequestValidation::Unauthenticated;
    switch (request.type())
    {
    case wire::HELLO_REQUEST:
        return request.has_hello() && !request.hello().client_name().empty() &&
                       request.hello().client_name().size() <= 64
                   ? RequestValidation::Valid
                   : RequestValidation::Malformed;
    case wire::PING:
        return request.has_ping() ? RequestValidation::Valid : RequestValidation::Malformed;
    case wire::ECHO_REQUEST:
        return request.has_echo_request() && request.echo_request().text().size() <= kMaxTextBytes
                   ? RequestValidation::Valid
                   : RequestValidation::Malformed;
    case wire::REGISTER:
    case wire::LOGIN:
    case wire::LOGOUT:
    case wire::SEND_TEXT:
    case wire::DELIVERY_ACK:
    case wire::READ_ACK:
    case wire::SYNC_EVENTS:
        return RequestValidation::Unsupported;
    default:
        return RequestValidation::Malformed;
    }
}
bool parseEnvelope(const std::string &payload, wire::Envelope &message)
{
    if (payload.empty() || payload.size() > kMaxFrameLength - kFrameMetadataSize)
        return false;
    message.Clear();
    google::protobuf::io::ArrayInputStream source(payload.data(), int(payload.size()));
    google::protobuf::io::CodedInputStream input(&source);
    input.SetTotalBytesLimit(int(kMaxFrameLength));
    input.SetRecursionLimit(64);
    return message.ParseFromCodedStream(&input) && input.ConsumedEntireMessage();
}
bool serializeEnvelope(const wire::Envelope &message, std::string &frame)
{
    if (message.ByteSizeLong() > kMaxFrameLength - kFrameMetadataSize)
        return false;
    std::string payload;
    return message.SerializeToString(&payload) && encodeFrame(payload, frame);
}
} // namespace protocol
} // namespace littlechat
