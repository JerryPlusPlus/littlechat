#include "littlechat/protocol/frame.h"

namespace littlechat
{
namespace protocol
{
namespace
{
std::uint32_t read32(const unsigned char *p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) |
           std::uint32_t(p[3]);
}
std::uint16_t read16(const unsigned char *p)
{
    return std::uint16_t((std::uint16_t(p[0]) << 8) | std::uint16_t(p[1]));
}
DecodeResult invalid(FrameError error)
{
    return {DecodeStatus::Invalid, 0, error};
}
} // namespace
DecodeResult decodeFrame(const void *data, std::size_t size, Frame &frame)
{
    if (!data && size != 0)
        return invalid(FrameError::InvalidPointer);
    if (size < kFramePrefixSize)
        return {DecodeStatus::NeedMore, 0, FrameError::None};
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    const std::uint32_t length = read32(bytes);
    if (length <= kFrameMetadataSize || length > kMaxFrameLength)
        return invalid(FrameError::InvalidLength);
    if (size < kFramePrefixSize + kFrameMetadataSize)
        return {DecodeStatus::NeedMore, 0, FrameError::None};
    if (read16(bytes + 4) != kProtocolVersion)
        return invalid(FrameError::UnsupportedVersion);
    if (read16(bytes + 6) != 0)
        return invalid(FrameError::UnsupportedFlags);
    const std::size_t total = kFramePrefixSize + std::size_t(length);
    if (size < total)
        return {DecodeStatus::NeedMore, 0, FrameError::None};
    frame.payload.assign(reinterpret_cast<const char *>(bytes + 8), length - kFrameMetadataSize);
    return {DecodeStatus::Complete, total, FrameError::None};
}
bool encodeFrame(const std::string &payload, std::string &output)
{
    if (payload.empty() || payload.size() > kMaxFrameLength - kFrameMetadataSize)
        return false;
    const std::uint32_t length = std::uint32_t(payload.size() + kFrameMetadataSize);
    output.clear();
    output.reserve(kFramePrefixSize + length);
    for (int shift = 24; shift >= 0; shift -= 8)
        output.push_back(char((length >> shift) & 0xff));
    output.push_back(0);
    output.push_back(char(kProtocolVersion));
    output.push_back(0);
    output.push_back(0);
    output.append(payload);
    return true;
}
} // namespace protocol
} // namespace littlechat
