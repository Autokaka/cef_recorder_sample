#include "frame_writer.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace pup {

FrameWriter::FrameWriter(std::filesystem::path output_dir, size_t frame_size, int pool_size, int num_threads)
    : output_dir_(std::move(output_dir)), frame_size_(frame_size) {
  std::filesystem::create_directories(output_dir_);

  // 预分配内存池
  all_buffers_.reserve(pool_size);
  for (int i = 0; i < pool_size; ++i) {
    auto buf = std::make_unique<FrameBuffer>(frame_size_);
    free_pool_.push(buf.get());
    all_buffers_.push_back(std::move(buf));
  }

  // 启动工作线程
  for (int i = 0; i < num_threads; ++i) {
    workers_.emplace_back(&FrameWriter::WorkerThread, this);
  }
}

FrameWriter::~FrameWriter() {
  {
    std::scoped_lock lock(work_mutex_);
    stop_ = true;
  }
  work_cv_.notify_all();
  pool_cv_.notify_all();
  for (auto& w : workers_) {
    if (w.joinable())
      w.join();
  }
}

FrameBuffer* FrameWriter::Acquire() {
  std::unique_lock lock(pool_mutex_);
  pool_cv_.wait(lock, [this] { return stop_ || !free_pool_.empty(); });
  if (stop_ && free_pool_.empty()) {
    return nullptr;
  }
  auto* buf = free_pool_.front();
  free_pool_.pop();
  return buf;
}

void FrameWriter::Submit(const void* buffer, int frame_id, size_t size) {
  auto* frame_buffer = Acquire();
  if (!frame_buffer) {
    return;
  }
  std::memcpy(frame_buffer->GetPtr(), buffer, size);
  frame_buffer->id = frame_id;
  frame_buffer->size = size;
  {
    std::scoped_lock lock(work_mutex_);
    work_queue_.push(frame_buffer);
    pending_count_.fetch_add(1);
  }
  work_cv_.notify_one();
}

void FrameWriter::Release(FrameBuffer* buffer) {
  {
    std::scoped_lock lock(pool_mutex_);
    free_pool_.push(buffer);
  }
  pool_cv_.notify_one();
}

void FrameWriter::Flush() {
  std::unique_lock lock(work_mutex_);
  flush_cv_.wait(lock, [this] { return work_queue_.empty() && pending_count_.load() == 0; });
}

void FrameWriter::WorkerThread() {
  while (true) {
    FrameBuffer* buffer = nullptr;
    {
      std::unique_lock lock(work_mutex_);
      work_cv_.wait(lock, [this] { return stop_ || !work_queue_.empty(); });
      if (stop_ && work_queue_.empty())
        return;
      buffer = work_queue_.front();
      work_queue_.pop();
    }

    // 写入文件
    std::ostringstream filename;
    filename << "frame-" << std::setw(6) << std::setfill('0') << buffer->id << ".bgra";
    std::ofstream file(output_dir_ / filename.str(), std::ios::binary | std::ios::trunc);
    if (file) {
      file.write(reinterpret_cast<const char*>(buffer->data.get()), static_cast<std::streamsize>(buffer->size));
    }

    // 归还缓冲区到池
    Release(buffer);

    written_count_.fetch_add(1);
    pending_count_.fetch_sub(1);
    flush_cv_.notify_all();
  }
}

}  // namespace pup
