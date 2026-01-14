#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pup {

// 裸帧数据结构
struct RawFrame {
  std::vector<uint8_t> data;  // BGRA 像素数据
  int width;
  int height;
  int frame_id;
};

// 异步帧写入器 - 使用生产者-消费者模式
class FrameWriter {
 public:
  explicit FrameWriter(std::filesystem::path output_dir, int num_threads = 2);
  ~FrameWriter();

  // 禁止拷贝
  FrameWriter(const FrameWriter&) = delete;
  FrameWriter& operator=(const FrameWriter&) = delete;

  // 提交帧到写入队列（非阻塞）
  void Submit(RawFrame frame);

  // 等待所有帧写入完成
  void Flush();

  // 获取已写入的帧数
  int GetWrittenCount() const { return written_count_.load(); }

  // 获取队列中待处理的帧数
  size_t GetPendingCount() const;

 private:
  void WorkerThread();
  void WriteFrame(const RawFrame& frame);

  std::filesystem::path output_dir_;
  std::vector<std::thread> workers_;
  std::queue<RawFrame> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable flush_cv_;
  std::atomic<bool> stop_{false};
  std::atomic<int> written_count_{0};
  std::atomic<int> pending_count_{0};
};

}  // namespace pup
