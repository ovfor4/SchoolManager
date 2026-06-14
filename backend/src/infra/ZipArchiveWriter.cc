#include "schoolmanager/infra/ZipArchiveWriter.h"

#include "schoolmanager/config/Constants.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace schoolmanager::infra {

namespace {

struct CentralDirectoryEntry {
    std::string name;
    std::uint32_t crc32{};
    std::uint32_t compressed_size{};
    std::uint32_t uncompressed_size{};
    std::uint32_t local_header_offset{};
};

void ensureZip32Size(std::size_t size, std::string_view field)
{
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string(field) + " exceeds ZIP32 size limit");
    }
}

void ensureZip16Size(std::size_t size, std::string_view field)
{
    if (size > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error(std::string(field) + " exceeds ZIP field size limit");
    }
}

void appendLe16(std::string& output, std::uint16_t value)
{
    output.push_back(static_cast<char>(value & 0xffU));
    output.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void appendLe32(std::string& output, std::uint32_t value)
{
    output.push_back(static_cast<char>(value & 0xffU));
    output.push_back(static_cast<char>((value >> 8U) & 0xffU));
    output.push_back(static_cast<char>((value >> 16U) & 0xffU));
    output.push_back(static_cast<char>((value >> 24U) & 0xffU));
}

void validateEntryName(std::string_view name)
{
    if (name.empty()) {
        throw std::runtime_error("zip entry name is required");
    }
    if (name == "." || name == ".." || name.front() == '/' ||
        name.find('\\') != std::string_view::npos ||
        name.find('/') != std::string_view::npos) {
        throw std::runtime_error("zip entry name is invalid");
    }
}

std::uint32_t entryCrc32(std::string_view content)
{
    auto crc = ::crc32(0L, Z_NULL, 0);
    crc = ::crc32(crc,
                  reinterpret_cast<const Bytef*>(content.data()),
                  static_cast<uInt>(content.size()));
    return static_cast<std::uint32_t>(crc);
}

std::string deflateRaw(std::string_view content)
{
    ensureZip32Size(content.size(), "zip entry");

    z_stream stream{};
    const auto init = ::deflateInit2(&stream,
                                     config::zipCompressionLevel,
                                     Z_DEFLATED,
                                     -MAX_WBITS,
                                     8,
                                     Z_DEFAULT_STRATEGY);
    if (init != Z_OK) {
        throw std::runtime_error("failed to initialize zip compression");
    }

    std::string compressed;
    compressed.resize(std::max<std::size_t>(64, ::compressBound(content.size())));

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(content.data()));
    stream.avail_in = static_cast<uInt>(content.size());
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data());
    stream.avail_out = static_cast<uInt>(compressed.size());

    const auto result = ::deflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        ::deflateEnd(&stream);
        throw std::runtime_error("failed to compress zip entry");
    }

    compressed.resize(stream.total_out);
    ::deflateEnd(&stream);
    ensureZip32Size(compressed.size(), "compressed zip entry");
    return compressed;
}

void appendLocalFileHeader(std::string& output,
                           const ZipArchiveEntry& entry,
                           std::string_view compressed,
                           std::uint32_t crc)
{
    appendLe32(output, 0x04034b50U);
    appendLe16(output, 20);
    appendLe16(output, 0);
    appendLe16(output, config::zipCompressionMethodDeflate);
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe32(output, crc);
    appendLe32(output, static_cast<std::uint32_t>(compressed.size()));
    appendLe32(output, static_cast<std::uint32_t>(entry.content.size()));
    appendLe16(output, static_cast<std::uint16_t>(entry.name.size()));
    appendLe16(output, 0);
    output.append(entry.name);
}

void appendCentralDirectoryHeader(std::string& output, const CentralDirectoryEntry& entry)
{
    appendLe32(output, 0x02014b50U);
    appendLe16(output, 20);
    appendLe16(output, 20);
    appendLe16(output, 0);
    appendLe16(output, config::zipCompressionMethodDeflate);
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe32(output, entry.crc32);
    appendLe32(output, entry.compressed_size);
    appendLe32(output, entry.uncompressed_size);
    appendLe16(output, static_cast<std::uint16_t>(entry.name.size()));
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe32(output, 0);
    appendLe32(output, entry.local_header_offset);
    output.append(entry.name);
}

void appendEndOfCentralDirectory(std::string& output,
                                 std::uint16_t entryCount,
                                 std::uint32_t centralDirectorySize,
                                 std::uint32_t centralDirectoryOffset)
{
    appendLe32(output, 0x06054b50U);
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe16(output, entryCount);
    appendLe16(output, entryCount);
    appendLe32(output, centralDirectorySize);
    appendLe32(output, centralDirectoryOffset);
    appendLe16(output, 0);
}

}  // namespace

std::string ZipArchiveWriter::write(const std::vector<ZipArchiveEntry>& entries) const
{
    if (entries.empty()) {
        throw std::runtime_error("zip entries are required");
    }
    if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("too many zip entries");
    }

    std::string output;
    std::vector<CentralDirectoryEntry> centralDirectory;
    centralDirectory.reserve(entries.size());

    for (const auto& entry : entries) {
        validateEntryName(entry.name);
        ensureZip32Size(entry.name.size(), "zip entry name");
        ensureZip16Size(entry.name.size(), "zip entry name");
        ensureZip32Size(output.size(), "zip archive");

        const auto localHeaderOffset = static_cast<std::uint32_t>(output.size());
        const auto compressed = deflateRaw(entry.content);
        const auto crc = entryCrc32(entry.content);

        appendLocalFileHeader(output, entry, compressed, crc);
        output.append(compressed);

        centralDirectory.push_back(CentralDirectoryEntry{
            .name = entry.name,
            .crc32 = crc,
            .compressed_size = static_cast<std::uint32_t>(compressed.size()),
            .uncompressed_size = static_cast<std::uint32_t>(entry.content.size()),
            .local_header_offset = localHeaderOffset,
        });
    }

    ensureZip32Size(output.size(), "zip archive");
    const auto centralDirectoryOffset = static_cast<std::uint32_t>(output.size());
    for (const auto& entry : centralDirectory) {
        appendCentralDirectoryHeader(output, entry);
    }

    ensureZip32Size(output.size() - centralDirectoryOffset, "zip central directory");
    appendEndOfCentralDirectory(
        output,
        static_cast<std::uint16_t>(entries.size()),
        static_cast<std::uint32_t>(output.size() - centralDirectoryOffset),
        centralDirectoryOffset);
    return output;
}

}  // namespace schoolmanager::infra
