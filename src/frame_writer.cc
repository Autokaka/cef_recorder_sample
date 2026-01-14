#include "frame_writer.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace pup {

FrameWriter::FrameWriter(std::filesystem::path output_dir, int num_threads) : output_dir_(std::move(output_dir)) {
  std::filesystem::create_directories(output_dir_);

  // 启动工作线程
  for (int i = 0; i < num_threads; ++i) {
    workers_.emplace_back(&FrameWriter::WorkerThread, this);
  }
}

FrameWriter::~FrameWriter() {
  // 停止所有工作线程
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();

  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void FrameWriter::Submit(RawFrame frame) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(frame));
    pending_count_.fetch_add(1);
  }
  cv_.notify_one();
}

void FrameWriter::Flush() {
  std::unique_lock<std::mutex> lock(mutex_);
  flush_cv_.wait(lock, [this] { return queue_.empty() && pending_count_.load() == 0; });
}

size_t FrameWriter::GetPendingCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}

void FrameWriter::WorkerThread() {
  while (true) {
    RawFrame frame;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });

      if (stop_ && queue_.empty()) {
        return;
      }

      frame = std::move(queue_.front());
      queue_.pop();
    }

    WriteFrame(frame);

    written_count_.fetch_add(1);
    pending_count_.fetch_sub(1);
    flush_cv_.notify_all();
  }
}

void FrameWriter::WriteFrame(const RawFrame& frame) {
  // 生成文件名: frame-XXXX.bgra
  std::ostringstream filename;
  filename << "frame-" << std::setw(6) << std::setfill('0') << frame.frame_id << ".bgra";

  const auto path = output_dir_ / filename.str();

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    std::cerr << "Failed to open file: " << path << '\n';
    return;
  }

  // 直接写入裸 BGRA 数据，无头部
  file.write(reinterpret_cast<const char*>(frame.data.data()), static_cast<std::streamsize>(frame.data.size()));
}

}  // namespace pup
