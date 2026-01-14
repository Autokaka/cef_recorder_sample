#pragma once

#include <include/base/cef_macros.h>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pup {

/// 预分配的帧缓冲区
struct FrameBuffer {
  int id = 0;
  size_t size = 0;
  std::unique_ptr<uint8_t[]> data;

  FrameBuffer() = default;
  explicit FrameBuffer(size_t capacity) : data(std::make_unique<uint8_t[]>(capacity)) {}

  uint8_t* GetPtr() { return data.get(); }

  DISALLOW_COPY(FrameBuffer);
};

/// 异步帧写入器（使用内存池避免频繁分配）
/// 职责: 将 BGRA 帧数据异步写入磁盘（生产者-消费者模式）
class FrameWriter {
 public:
  explicit FrameWriter(std::filesystem::path output_dir, size_t frame_size, int pool_size = 8, int num_threads = 3);
  ~FrameWriter();

  FrameWriter(const FrameWriter&) = delete;
  FrameWriter& operator=(const FrameWriter&) = delete;

  /// 提交已填充的缓冲区
  void Submit(const void* buffer, int frame_id, size_t size);

  /// 等待所有帧写入完成
  void Flush();

  /// 已写入帧数
  int GetWrittenCount() const { return written_count_.load(); }

 private:
  void WorkerThread();
  FrameBuffer* Acquire();
  void Release(FrameBuffer* buffer);

  std::filesystem::path output_dir_;
  size_t frame_size_;

  // 内存池：空闲缓冲区
  std::vector<std::unique_ptr<FrameBuffer>> all_buffers_;
  std::queue<FrameBuffer*> free_pool_;
  std::mutex pool_mutex_;
  std::condition_variable pool_cv_;

  // 工作队列：待写入的缓冲区
  std::vector<std::thread> workers_;
  std::queue<FrameBuffer*> work_queue_;
  std::mutex work_mutex_;
  std::condition_variable work_cv_;
  std::condition_variable flush_cv_;

  std::atomic<bool> stop_{false};
  std::atomic<int> written_count_{0};
  std::atomic<int> pending_count_{0};
};

}  // namespace pup
