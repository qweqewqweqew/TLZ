#include "ShmImageReader.h"

#include <boost/interprocess/exceptions.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <algorithm>
#include <exception>

namespace {

namespace bip = boost::interprocess;

// 轻量字符串拼接，避免引入 <sstream>/<format>。
std::string concat(std::initializer_list<std::string> parts)
{
    std::string out;
    std::size_t total = 0;
    for (const auto &p : parts) total += p.size();
    out.reserve(total);
    for (const auto &p : parts) out.append(p);
    return out;
}

std::string toStr(std::uint64_t v) { return std::to_string(v); }

std::string normalizeBoostShmName(const std::string &name)
{
    std::string result = name;

    const std::string localPrefix = "Local\\";
    const std::string globalPrefix = "Global\\";
    if (result.compare(0, localPrefix.size(), localPrefix) == 0) {
        result.erase(0, localPrefix.size());
    } else if (result.compare(0, globalPrefix.size(), globalPrefix) == 0) {
        result.erase(0, globalPrefix.size());
    }

    while (!result.empty() && (result.front() == '/' || result.front() == '\\')) {
        result.erase(result.begin());
    }
    std::replace(result.begin(), result.end(), '/', '_');
    std::replace(result.begin(), result.end(), '\\', '_');
    return result;
}

} // namespace

struct ShmImageReader::Impl
{
    std::unique_ptr<bip::shared_memory_object> shmObject;
    std::unique_ptr<bip::mapped_region> mappedRegion;
};

ShmImageReader::ShmImageReader() = default;

ShmImageReader::~ShmImageReader()
{
    close();
}

void ShmImageReader::close()
{
    if (m_impl) {
        m_impl->mappedRegion.reset();
        m_impl->shmObject.reset();
    }
    m_view = nullptr;
    m_viewSize = 0;
    m_currentName.clear();
}

bool ShmImageReader::open(const std::string &shmName, std::string *error)
{
    const std::string normalizedName = normalizeBoostShmName(shmName);
    if (normalizedName.empty()) {
        if (error) *error = "shm_name is empty";
        return false;
    }

    if (m_impl && m_impl->mappedRegion && m_currentName == normalizedName) {
        return true;   // 复用已打开的句柄
    }
    close();

    try {
        if (!m_impl) {
            m_impl = std::make_unique<Impl>();
        }
        m_impl->shmObject = std::make_unique<bip::shared_memory_object>(
            bip::open_only,
            normalizedName.c_str(),
            bip::read_only);
        m_impl->mappedRegion = std::make_unique<bip::mapped_region>(
            *m_impl->shmObject,
            bip::read_only);
    } catch (const bip::interprocess_exception &ex) {
        close();
        if (error) {
            *error = concat({"open Boost shared memory '", normalizedName,
                             "' failed: ", ex.what()});
        }
        return false;
    }

    m_view = static_cast<const std::uint8_t *>(m_impl->mappedRegion->get_address());
    m_viewSize = m_impl->mappedRegion->get_size();
    m_currentName = normalizedName;
    if (!m_view || m_viewSize == 0) {
        close();
        if (error) *error = concat({"Boost shared memory '", normalizedName, "' mapped empty"});
        return false;
    }
    return true;
}

RawSlice ShmImageReader::slice(std::uint64_t offset,
                               std::uint64_t size,
                               std::string  *error) const
{
    if (!m_view) {
        if (error) *error = "shared memory not opened";
        return {};
    }
    if (size == 0) {
        if (error) *error = "slice size is 0";
        return {};
    }
    if (offset > m_viewSize || size > std::uint64_t(m_viewSize) - offset) {
        if (error) {
            *error = concat({"slice out of view: offset=", toStr(offset),
                             " size=", toStr(size),
                             " view=", toStr(std::uint64_t(m_viewSize))});
        }
        return {};
    }
    RawSlice s;
    s.data = m_view + offset;
    s.size = static_cast<std::size_t>(size);
    return s;
}

bool ShmImageReader::sliceScanFrame(std::uint64_t rangeSize,
                                    std::uint64_t intensitySize,
                                    RawSlice     *outRange,
                                    RawSlice     *outIntensity,
                                    std::string  *error) const
{
    if (outRange)     *outRange     = RawSlice{};
    if (outIntensity) *outIntensity = RawSlice{};

    bool any = false;
    std::string localErr;

    if (rangeSize > 0 && outRange) {
        RawSlice s = slice(0, rangeSize, &localErr);
        if (!s.empty()) {
            *outRange = s;
            any = true;
        } else if (error && !localErr.empty()) {
            *error = concat({"[Range] ", localErr});
        }
    }

    if (intensitySize > 0 && outIntensity) {
        localErr.clear();
        RawSlice s = slice(rangeSize, intensitySize, &localErr);
        if (!s.empty()) {
            *outIntensity = s;
            any = true;
        } else if (error && !localErr.empty()) {
            if (!error->empty()) error->append("; ");
            error->append("[Intensity] ").append(localErr);
        }
    }

    return any;
}
