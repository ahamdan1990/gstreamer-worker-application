#ifndef GSTREAMER_WORKER_CALLBACK_QUEUE_H
#define GSTREAMER_WORKER_CALLBACK_QUEUE_H

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <gst/gst.h>

namespace gstreamer_worker {

/**
 * @brief Thread-safe queue for processing GStreamer callbacks safely
 *
 * This queue prevents race conditions when GStreamer callbacks execute in
 * library threads while the main thread is destroying objects. Instead of
 * executing work directly in callbacks, we queue it and process from a safe context.
 *
 * Key Features:
 * - Thread-safe enqueue/dequeue operations
 * - Non-blocking enqueue for real-time callbacks
 * - Batch processing for efficiency
 * - Automatic cleanup of pending items
 * - GstSample reference counting support
 */
class CallbackQueue {
public:
    /**
     * @brief Sample task with automatic GstSample reference management
     */
    struct SampleTask {
        GstSample* sample;
        std::function<void(GstSample*)> callback;

        SampleTask(GstSample* s, std::function<void(GstSample*)> cb)
            : sample(s), callback(std::move(cb)) {
            if (sample) {
                gst_sample_ref(sample);  // Add reference for queue ownership
            }
        }

        ~SampleTask() {
            if (sample) {
                gst_sample_unref(sample);  // Release our reference
            }
        }

        // Delete copy, allow move
        SampleTask(const SampleTask&) = delete;
        SampleTask& operator=(const SampleTask&) = delete;

        SampleTask(SampleTask&& other) noexcept
            : sample(other.sample), callback(std::move(other.callback)) {
            other.sample = nullptr;
        }

        SampleTask& operator=(SampleTask&& other) noexcept {
            if (this != &other) {
                if (sample) {
                    gst_sample_unref(sample);
                }
                sample = other.sample;
                callback = std::move(other.callback);
                other.sample = nullptr;
            }
            return *this;
        }
    };

    CallbackQueue() : running_(true) {}

    ~CallbackQueue() {
        clear();
    }

    /**
     * @brief Enqueue a sample processing task (called from GStreamer thread)
     * @param sample GstSample to process (will be ref-counted)
     * @param callback Function to execute with the sample (in safe context)
     * @return true if enqueued successfully, false if queue is shutting down
     */
    bool enqueue_sample(GstSample* sample, std::function<void(GstSample*)> callback) {
        if (!running_.load(std::memory_order_acquire) || !sample) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_tasks_.emplace(sample, std::move(callback));
        }
        cv_.notify_one();
        return true;
    }

    /**
     * @brief Process all pending tasks (called from safe thread context)
     * @param max_tasks Maximum number of tasks to process (0 = all)
     * @return Number of tasks processed
     */
    size_t process_pending(size_t max_tasks = 0) {
        size_t processed = 0;
        std::unique_lock<std::mutex> lock(mutex_);

        while (!pending_tasks_.empty() && (max_tasks == 0 || processed < max_tasks)) {
            auto task = std::move(pending_tasks_.front());
            pending_tasks_.pop();
            lock.unlock();  // Release lock before executing callback

            // Execute the callback with the sample
            if (task.callback && task.sample) {
                try {
                    task.callback(task.sample);
                } catch (const std::exception& e) {
                    // Log error but don't crash
                    std::cerr << "[CallbackQueue] Task execution failed: " << e.what() << std::endl;
                }
            }

            processed++;
            lock.lock();  // Re-acquire for next iteration
        }

        return processed;
    }

    /**
     * @brief Try to process one task with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return true if a task was processed, false if timeout or empty
     */
    bool try_process_one(int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (pending_tasks_.empty()) {
            cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                [this] { return !pending_tasks_.empty() || !running_.load(); });
        }

        if (pending_tasks_.empty()) {
            return false;
        }

        auto task = std::move(pending_tasks_.front());
        pending_tasks_.pop();
        lock.unlock();

        if (task.callback && task.sample) {
            try {
                task.callback(task.sample);
            } catch (const std::exception& e) {
                std::cerr << "[CallbackQueue] Task execution failed: " << e.what() << std::endl;
            }
        }

        return true;
    }

    /**
     * @brief Get number of pending tasks
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_tasks_.size();
    }

    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_tasks_.empty();
    }

    /**
     * @brief Clear all pending tasks and stop accepting new ones
     */
    void clear() {
        running_.store(false, std::memory_order_release);
        cv_.notify_all();

        std::lock_guard<std::mutex> lock(mutex_);
        while (!pending_tasks_.empty()) {
            pending_tasks_.pop();  // Destructors will handle cleanup
        }
    }

    /**
     * @brief Check if queue is running (accepting tasks)
     */
    bool is_running() const {
        return running_.load(std::memory_order_acquire);
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<SampleTask> pending_tasks_;
    std::atomic<bool> running_;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_CALLBACK_QUEUE_H
