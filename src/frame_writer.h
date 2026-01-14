#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pup {

/// 异步帧写入器
/// 职责: 将 BGRA 帧数据异步写入磁盘（生产者-消费者模式）
class FrameWriter {
 public:
  explicit FrameWriter(std::filesystem::path output_dir, int num_threads = 2);
  ~FrameWriter();

  FrameWriter(const FrameWriter&) = delete;
  FrameWriter& operator=(const FrameWriter&) = delete;

  /// 提交帧数据（非阻塞，会拷贝数据）
  void Write(int frame_id, const void* data, size_t size);

  /// 等待所有帧写入完成
  void Flush();

  /// 已写入帧数
  int GetWrittenCount() const { return written_count_.load(); }

 private:
  struct Frame {
    int id;
    std::vector<uint8_t> data;
  };

  void WorkerThread();

  std::filesystem::path output_dir_;
  std::vector<std::thread> workers_;
  std::queue<Frame> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable flush_cv_;
  std::atomic<bool> stop_{false};
  std::atomic<int> written_count_{0};
  std::atomic<int> pending_count_{0};
};

}  // namespace pup
