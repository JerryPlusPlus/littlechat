#include "littlechat/protocol/envelope.h"
#include "littlechat/protocol/frame.h"
#include <gtest/gtest.h>
#include <limits>
#include <string>

using namespace littlechat;
using namespace littlechat::protocol;

TEST(Frame, RoundTripsBinaryAndEveryPartialLength)
{
    const std::string payload("hello\0world", 11);
    std::string encoded;
    ASSERT_TRUE(encodeFrame(payload, encoded));
    Frame result;
    for (std::size_t size = 0; size < encoded.size(); ++size)
    {
        const DecodeResult partial = decodeFrame(encoded.data(), size, result);
        EXPECT_EQ(partial.status, DecodeStatus::NeedMore) << size;
        EXPECT_EQ(partial.consumed, 0U);
    }
    const DecodeResult complete = decodeFrame(encoded.data(), encoded.size(), result);
    EXPECT_EQ(complete.status, DecodeStatus::Complete);
    EXPECT_EQ(complete.consumed, encoded.size());
    EXPECT_EQ(result.payload, payload);
}
TEST(Frame, HandlesMergedFramesWithIncompleteTail)
{
    std::string first, second;
    ASSERT_TRUE(encodeFrame("one", first));
    ASSERT_TRUE(encodeFrame("two", second));
    std::string input = first + second + first.substr(0, 6);
    Frame frame;
    std::size_t offset = 0;
    for (const std::string &expected : {std::string("one"), std::string("two")})
    {
        const DecodeResult result = decodeFrame(input.data() + offset, input.size() - offset, frame);
        ASSERT_EQ(result.status, DecodeStatus::Complete);
        EXPECT_EQ(frame.payload, expected);
        offset += result.consumed;
    }
    EXPECT_EQ(decodeFrame(input.data() + offset, input.size() - offset, frame).status,
              DecodeStatus::NeedMore);
}
TEST(Frame, AcceptsMaximumAndRejectsOversizedPayload)
{
    std::string encoded;
    const std::string maximum(kMaxFrameLength - kFrameMetadataSize, 'x');
    ASSERT_TRUE(encodeFrame(maximum, encoded));
    Frame frame;
    EXPECT_EQ(decodeFrame(encoded.data(), encoded.size(), frame).status, DecodeStatus::Complete);
    EXPECT_EQ(frame.payload, maximum);
    EXPECT_FALSE(encodeFrame(maximum + "x", encoded));
    EXPECT_FALSE(encodeFrame("", encoded));
}
TEST(Frame, RejectsLengthsBeforePayloadAllocation)
{
    Frame frame;
    for (const std::uint32_t length :
         {0U, 1U, 4U, kMaxFrameLength + 1, std::numeric_limits<std::uint32_t>::max()})
    {
        std::string input;
        for (int shift = 24; shift >= 0; shift -= 8)
            input.push_back(char((length >> shift) & 255));
        const DecodeResult result = decodeFrame(input.data(), input.size(), frame);
        EXPECT_EQ(result.status, DecodeStatus::Invalid);
        EXPECT_EQ(result.error, FrameError::InvalidLength);
        EXPECT_TRUE(frame.payload.empty());
    }
}
TEST(Frame, RejectsVersionFlagsAndInvalidPointer)
{
    Frame frame;
    std::string encoded;
    ASSERT_TRUE(encodeFrame("x", encoded));
    encoded[5] = 2;
    EXPECT_EQ(decodeFrame(encoded.data(), encoded.size(), frame).error, FrameError::UnsupportedVersion);
    encoded[5] = 1;
    encoded[7] = 1;
    EXPECT_EQ(decodeFrame(encoded.data(), encoded.size(), frame).error, FrameError::UnsupportedFlags);
    EXPECT_EQ(decodeFrame(nullptr, 1, frame).error, FrameError::InvalidPointer);
    EXPECT_EQ(decodeFrame(nullptr, 0, frame).status, DecodeStatus::NeedMore);
}
TEST(Envelope, SerializesAndValidatesHello)
{
    wire::Envelope request;
    request.set_type(wire::HELLO_REQUEST);
    request.set_request_id(42);
    request.mutable_hello()->set_client_name("test-client");
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Valid);
    std::string encoded;
    ASSERT_TRUE(serializeEnvelope(request, encoded));
    Frame frame;
    ASSERT_EQ(decodeFrame(encoded.data(), encoded.size(), frame).status, DecodeStatus::Complete);
    wire::Envelope parsed;
    ASSERT_TRUE(parsed.ParseFromString(frame.payload));
    EXPECT_EQ(parsed.request_id(), 42U);
    EXPECT_EQ(parsed.hello().client_name(), "test-client");
}
TEST(Envelope, RejectsUnknownTypesBadBodiesAndUntrustedIdentity)
{
    wire::Envelope request;
    request.set_type(wire::PING);
    request.set_request_id(1);
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Malformed);
    request.mutable_ping()->set_nonce(7);
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Valid);
    request.set_sender_id(123);
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Unauthenticated);
    request.clear_sender_id();
    request.set_request_id(0);
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Malformed);
    request.set_request_id(1);
    request.set_type(static_cast<wire::MessageType>(999));
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Malformed);
}
TEST(Envelope, TextIsBoundedAndBusinessIsExplicitlyUnimplemented)
{
    wire::Envelope request;
    request.set_type(wire::ECHO_REQUEST);
    request.set_request_id(1);
    request.mutable_echo_request()->set_text(std::string(kMaxTextBytes, 'x'));
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Valid);
    request.mutable_echo_request()->set_text(std::string(kMaxTextBytes + 1, 'x'));
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Malformed);
    request.set_type(wire::SEND_TEXT);
    EXPECT_EQ(validateDiagnosticRequest(request), RequestValidation::Unsupported);
}
TEST(Envelope, RejectsMalformedProtobuf)
{
    wire::Envelope parsed;
    EXPECT_FALSE(parseEnvelope(std::string("\xff\xff\xff", 3), parsed));
}

TEST(Envelope, BoundsUnknownGroupRecursion)
{
    wire::Envelope message;
    const std::string nested = std::string(100, '\x0b') + std::string(100, '\x0c');
    EXPECT_FALSE(parseEnvelope(nested, message));
    EXPECT_FALSE(parseEnvelope("", message));
}
