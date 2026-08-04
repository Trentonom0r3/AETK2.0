#pragma once

#include <aetk/aegp/hooks.hpp>
#include <aetk/core/suite.hpp>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>

namespace aetk::aegp {

/**
 * @brief Thread-safe background task scheduler for executing AE SDK operations
 * on the main UI thread.
 *
 * @details Because the After Effects SDK is not thread-safe, background threads
 * (such as rendering threads, asset download tasks, or ONNX model inference
 * sessions) cannot invoke suite functions directly without causing instant host
 * crashes. The scheduler solves this by holding tasks in a locked queue and
 * dispatching them during After Effects' idle loop callbacks.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Automates background-to-main thread
 * dispatching and triggers immediate execution using
 * `AEGP_CauseIdleRoutinesToBeCalled`.
 */
class scheduler {
private:
  std::mutex m_mutex;
  std::queue<std::function<void()>> m_tasks;

  scheduler() = default;

public:
  /**
   * @brief Singleton instance getter.
   * @return The single scheduler instance.
   */
  static scheduler &instance() {
    static scheduler inst;
    return inst;
  }

  scheduler(const scheduler &) = delete;
  scheduler &operator=(const scheduler &) = delete;

  /**
   * @brief Schedules a task with a return value and returns a future.
   *
   * @param func The lambda or function to execute on the main thread.
   * @param call_idle If true, prompts After Effects to process idle routines
   * immediately.
   * @return std::future representing the eventual result of the task.
   */
  template <typename Func> auto schedule(Func &&func, bool call_idle = true) {
    using return_type = std::invoke_result_t<Func>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<Func>(func));
    auto fut = task->get_future();

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_tasks.push([task]() { (*task)(); });
    }

    if (call_idle) {
      trigger_idle();
    }

    return fut;
  }

  /**
   * @brief Executes all currently queued tasks.
   * @warning Must only be called on After Effects' main UI thread.
   */
  void execute_tasks() {
    std::queue<std::function<void()>> local_tasks;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      std::swap(m_tasks, local_tasks);
    }

    while (!local_tasks.empty()) {

      local_tasks.front()();

      local_tasks.pop();
    }
  }

  /**
   * @brief Triggers After Effects to call its idle routines immediately.
   */
  void trigger_idle() {
    using utility_suite =
        aetk::core::suite<AEGP_UtilitySuite6,
                          aetk::core::fixed_string(kAEGPUtilitySuite),
                          kAEGPUtilitySuiteVersion6>;
    utility_suite::call<
        &AEGP_UtilitySuite6::AEGP_CauseIdleRoutinesToBeCalled>();
  }

  /**
   * @brief Automatically initializes the scheduler by hooking into AETK's idle
   * loop.
   */
  static void init() {
    on_idle([]() { instance().execute_tasks(); });
  }
};

/**
 * @brief Schedules a task with a return value on the After Effects main thread.
 *
 * @tparam Func Callable function/lambda type.
 * @param func Callable function/lambda.
 * @param call_idle Whether to trigger immediate idle processing.
 * @return std::future containing the task result.
 */
template <typename Func>
inline auto schedule_task(Func &&func, bool call_idle = true) {
  return scheduler::instance().schedule(std::forward<Func>(func), call_idle);
}

} // namespace aetk::aegp
