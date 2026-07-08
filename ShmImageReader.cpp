#include "ShmImageReader.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <cstring>

namespace {

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

#ifdef _WIN32
std::wstring toWide(const std::string &s)
{
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()),
                                        nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}
#endif

constexpr char kLocalPrefix[] = "Local\\";

bool hasLocalPrefix(const std::string &name)
{
    return name.size() >= 6 && std::memcmp(name.data(), kLocalPrefix, 6) == 0;
}

} // namespace

ShmImageReader::ShmImageReader() = default;

ShmImageReader::~ShmImageReader()
{
    close();
}

void ShmImageReader::close()
{
#ifdef _WIN32
    if (m_view) {
        ::UnmapViewOfFile(m_view);
        m_view = nullptr;
    }
    if (m_handle) {
        ::CloseHandle(reinterpret_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
#endif
    m_viewSize = 0;
    m_currentName.clear();
}

bool ShmImageReader::open(const std::string &shmName, std::string *error)
{
#ifdef _WIN32
    if (shmName.empty()) {
        if (error) *error = "shm_name is empty";
        return false;
    }

    const std::string fullName = hasLocalPrefix(shmName)
                                     ? shmName
                                     : std::string(kLocalPrefix) + shmName;
    if (m_handle && m_currentName == fullName) {
        return true;   // 复用已打开的句柄
    }
    close();

    const std::wstring wideName = toWide(fullName);
    HANDLE h = ::OpenFileMappingW(FILE_MAP_READ, FALSE, wideName.c_str());
    if (!h) {
        if (error) {
            *error = concat({"OpenFileMappingW('", fullName,
                             "') failed, GetLastError=", toStr(::GetLastError())});
        }
        return false;
    }

    LPVOID v = ::MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    if (!v) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(h);
        if (error) {
            *error = concat({"MapViewOfFile('", fullName,
                             "') failed, GetLastError=", toStr(err)});
        }
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    const SIZE_T queried = ::VirtualQuery(v, &mbi, sizeof(mbi));
    if (queried == 0) {
        const DWORD err = ::GetLastError();
        ::UnmapViewOfFile(v);
        ::CloseHandle(h);
        if (error) {
            *error = concat({"VirtualQuery failed, GetLastError=", toStr(err)});
        }
        return false;
    }

    m_handle   = h;
    m_view     = static_cast<std::uint8_t *>(v);
    m_viewSize = static_cast<std::size_t>(mbi.RegionSize);
    m_currentName = fullName;
    return true;
#else
    (void)shmName;
    if (error) *error = "ShmImageReader currently only supports Windows";
    return false;
#endif
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
