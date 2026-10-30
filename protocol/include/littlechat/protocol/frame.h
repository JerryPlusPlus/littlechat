#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace littlechat
{
namespace protocol
{
constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint32_t kMaxFrameLength = 1024U * 1024U;
constexpr std::size_t kFramePrefixSize = 4;
constexpr std::size_t kFrameMetadataSize = 4;
constexpr std::size_t kMaxTextBytes = 16U * 1024U;

enum class DecodeStatus
{
    NeedMore,
    Complete,
    Invalid
};
enum class FrameError
{
    None,
    InvalidPointer,
    InvalidLength,
    UnsupportedVersion,
    UnsupportedFlags
};
struct Frame
{
    std::string payload;
};
struct DecodeResult
{
    DecodeStatus status;
    std::size_t consumed;
    FrameError error;
};
// Reads exactly one frame. It never consumes input or allocates from an unvalidated length.
DecodeResult decodeFrame(const void *data, std::size_t size, Frame &frame);
bool encodeFrame(const std::string &payload, std::string &output);
} // namespace protocol
} // namespace littlechat
