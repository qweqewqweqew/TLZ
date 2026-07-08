#ifndef SHMIMAGEREADER_H
#define SHMIMAGEREADER_H

// ---------------------------------------------------------------------------
// 生命周期:
//   RawSlice::data 是指向 mapping view 内部的裸指针，只在 Reader 实例存活
//   且 view 未被 close 之前有效。使用方要么立刻处理完，要么先自己 memcpy
//   出去。close() / 换 shmName / 析构 都会让 slice 失效。
//
// 依赖: C++17 + <string> <cstdint> + Windows SDK。无 Qt / 无 OpenCV / 无 ROS。
//
// 线程安全: 不是线程安全的。多线程共用同一实例请外部加锁。
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <string>

// 指向共享内存视图内部一段只读字节的引用。不拥有数据、不管释放。
struct RawSlice
{
    const std::uint8_t *data = nullptr;
    std::size_t         size = 0;

    bool empty() const { return data == nullptr || size == 0; }
};

class ShmImageReader
{
public:
    ShmImageReader();
    ~ShmImageReader();

    ShmImageReader(const ShmImageReader &) = delete;
    ShmImageReader &operator=(const ShmImageReader &) = delete;

    // 打开 (或复用已打开的) 命名共享内存。shmName 与 ROS 消息里的 shm_name
    // 一致，例如 "ivf_scan_192_168_88_150"；不需要调用方拼 "Local\\" 前缀，
    // 本类会自动补齐。
    bool open(const std::string &shmName, std::string *error = nullptr);

    // 关闭当前 view / mapping 句柄。析构会自动调。
    void close();

    // 从当前 view + offset 处取一段字节。不拷贝、不解释。失败返回空 slice
    // 并写 error。open() 尚未成功、越界、size==0 都算失败。
    RawSlice slice(std::uint64_t offset,
                   std::uint64_t size,
                   std::string  *error = nullptr) const;

    // 便利入口: 按 ScanResult 消息里的两段大小切出 Range + Intensity。
    // 约定的 offset:  Range   = 0
    //                 Intensity = rangeSize
    // 任一 size == 0 表示对应通道不存在，输出 slice 保持 empty()。
    // 返回 true 表示"至少切到了一段有效字节"。
    bool sliceScanFrame(std::uint64_t rangeSize,
                        std::uint64_t intensitySize,
                        RawSlice     *outRange,
                        RawSlice     *outIntensity,
                        std::string  *error = nullptr) const;

    // 当前 view 的字节数，未打开时为 0。
    std::size_t viewSize() const { return m_viewSize; }

    bool isOpen() const { return m_view != nullptr; }

private:
    std::string   m_currentName;
    void         *m_handle{nullptr};  // HANDLE on Windows
    std::uint8_t *m_view{nullptr};
    std::size_t   m_viewSize{0};
};

#endif // SHMIMAGEREADER_H
