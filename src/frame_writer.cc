#include "frame_writer.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace pup {

FrameWriter::FrameWriter(std::filesystem::path output_dir, int num_threads) : output_dir_(std::move(output_dir)) {
  std::filesystem::create_directories(output_dir_);
  for (int i = 0; i < num_threads; ++i) {
    workers_.emplace_back(&FrameWriter::WorkerThread, this);
  }
}

FrameWriter::~FrameWriter() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& w : workers_) {
    if (w.joinable())
      w.join();
  }
}

void FrameWriter::Write(int frame_id, const void* data, size_t size) {
  Frame frame;
  frame.id = frame_id;
  frame.data.resize(size);
  std::memcpy(frame.data.data(), data, size);

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

void FrameWriter::WorkerThread() {
  while (true) {
    Frame frame;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
      if (stop_ && queue_.empty())
        return;
      frame = std::move(queue_.front());
      queue_.pop();
    }

    // 写入文件
    std::ostringstream filename;
    filename << "frame-" << std::setw(6) << std::setfill('0') << frame.id << ".bgra";
    std::ofstream file(output_dir_ / filename.str(), std::ios::binary | std::ios::trunc);
    if (file) {
      file.write(reinterpret_cast<const char*>(frame.data.data()), static_cast<std::streamsize>(frame.data.size()));
    }

    written_count_.fetch_add(1);
    pending_count_.fetch_sub(1);
    flush_cv_.notify_all();
  }
}

}  // namespace pup
