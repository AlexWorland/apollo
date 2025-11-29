/**
 * @file src/task_pool.h
 * @brief Declarations for the task pool system.
 */
#pragma once

// standard includes
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

// local includes
#include "move_by_copy.h"
#include "utility.h"

namespace task_pool_util {

  /**
   * @brief Base class for task pool implementation.
   * 
   * Internal base class for task execution in the task pool.
   */
  class _ImplBase {
  public:
    // _unique_base_type _this_ptr;

    inline virtual ~_ImplBase() = default;

    /**
     * @brief Execute the task.
     * 
     * Pure virtual method to be implemented by derived classes.
     */
    virtual void run() = 0;
  };

  /**
   * @brief Task implementation for function execution.
   * 
   * Internal implementation class that wraps a function for execution in the task pool.
   * 
   * @tparam Function Function or callable type to execute.
   */
  template<class Function>
  class _Impl: public _ImplBase {
    Function _func;

  public:
    /**
     * @brief Construct task implementation.
     * 
     * @param f Function to execute.
     */
    _Impl(Function &&f):
        _func(std::forward<Function>(f)) {
    }

    /**
     * @brief Execute the wrapped function.
     */
    void run() override {
      _func();
    }
  };

  /**
   * @brief Thread pool for asynchronous task execution.
   * 
   * Manages a pool of worker threads for executing tasks asynchronously.
   * Supports scheduled tasks and timer-based execution.
   */
  class TaskPool {
  public:
    typedef std::unique_ptr<_ImplBase> __task;  ///< Task pointer type.
    typedef _ImplBase *task_id_t;  ///< Task identifier type.

    typedef std::chrono::steady_clock::time_point __time_point;  ///< Time point type.

    /**
     * @brief Timer task result structure.
     * 
     * Contains the task ID and future result for scheduled timer tasks.
     * 
     * @tparam R Return type of the timer task.
     */
    template<class R>
    class timer_task_t {
    public:
      task_id_t task_id;  ///< Task identifier for cancellation/delaying.
      std::future<R> future;  ///< Future for getting task result.

      /**
       * @brief Construct timer task result.
       * 
       * @param task_id Task identifier.
       * @param future Future for task result.
       */
      timer_task_t(task_id_t task_id, std::future<R> &future):
          task_id {task_id},
          future {std::move(future)} {
      }
    };

  protected:
    std::deque<__task> _tasks;  ///< Queue of immediate tasks.
    std::vector<std::pair<__time_point, __task>> _timer_tasks;  ///< Sorted list of scheduled timer tasks.
    std::mutex _task_mutex;  ///< Mutex for thread-safe task access.

  public:
    TaskPool() = default;

    TaskPool(TaskPool &&other) noexcept:
        _tasks {std::move(other._tasks)},
        _timer_tasks {std::move(other._timer_tasks)} {
    }

    TaskPool &operator=(TaskPool &&other) noexcept {
      std::swap(_tasks, other._tasks);
      std::swap(_timer_tasks, other._timer_tasks);

      return *this;
    }

    /**
     * @brief Push a task to the pool for immediate execution.
     * 
     * Creates a task from a function and arguments, and adds it to the task queue.
     * 
     * @tparam Function Function or callable type.
     * @tparam Args Argument types.
     * @param newTask Function to execute.
     * @param args Arguments to pass to function.
     * @return Future for getting the task result.
     */
    template<class Function, class... Args>
    auto push(Function &&newTask, Args &&...args) {
      static_assert(std::is_invocable_v<Function, Args &&...>, "arguments don't match the function");

      using __return = std::invoke_result_t<Function, Args &&...>;
      using task_t = std::packaged_task<__return()>;

      auto bind = [task = std::forward<Function>(newTask), tuple_args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply(task, std::move(tuple_args));
      };

      task_t task(std::move(bind));

      auto future = task.get_future();

      std::lock_guard<std::mutex> lg(_task_mutex);
      _tasks.emplace_back(toRunnable(std::move(task)));

      return future;
    }

    /**
     * @brief Push a delayed task to the timer queue.
     * 
     * Adds a task with its scheduled time point to the sorted timer queue.
     * 
     * @param task Pair of time point and task to schedule.
     */
    void pushDelayed(std::pair<__time_point, __task> &&task) {
      std::lock_guard lg(_task_mutex);

      auto it = _timer_tasks.cbegin();
      for (; it < _timer_tasks.cend(); ++it) {
        if (std::get<0>(*it) < task.first) {
          break;
        }
      }

      _timer_tasks.emplace(it, task.first, std::move(task.second));
    }

    /**
     * @brief Push a task to be executed after a delay.
     * 
     * Creates a task from a function and arguments, and schedules it for execution
     * after the specified duration.
     * 
     * @tparam Function Function or callable type.
     * @tparam X Duration value type.
     * @tparam Y Duration period type.
     * @tparam Args Argument types.
     * @param newTask Function to execute.
     * @param duration Delay before execution.
     * @param args Arguments to pass to function.
     * @return Timer task result with task ID and future.
     */
    template<class Function, class X, class Y, class... Args>
    auto pushDelayed(Function &&newTask, std::chrono::duration<X, Y> duration, Args &&...args) {
      static_assert(std::is_invocable_v<Function, Args &&...>, "arguments don't match the function");

      using __return = std::invoke_result_t<Function, Args &&...>;
      using task_t = std::packaged_task<__return()>;

      __time_point time_point;
      if constexpr (std::is_floating_point_v<X>) {
        time_point = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
      } else {
        time_point = std::chrono::steady_clock::now() + duration;
      }

      auto bind = [task = std::forward<Function>(newTask), tuple_args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply(task, std::move(tuple_args));
      };

      task_t task(std::move(bind));

      auto future = task.get_future();
      auto runnable = toRunnable(std::move(task));

      task_id_t task_id = &*runnable;

      pushDelayed(std::pair {time_point, std::move(runnable)});

      return timer_task_t<__return> {task_id, future};
    }

    /**
     * @brief Delay an existing scheduled task.
     * 
     * Updates the execution time of a task that's already in the timer queue.
     * 
     * @tparam X Duration value type.
     * @tparam Y Duration period type.
     * @param task_id The ID of the task to delay.
     * @param duration The additional delay before executing the task.
     */
    template<class X, class Y>
    void delay(task_id_t task_id, std::chrono::duration<X, Y> duration) {
      std::lock_guard<std::mutex> lg(_task_mutex);

      auto it = _timer_tasks.begin();
      for (; it < _timer_tasks.cend(); ++it) {
        const __task &task = std::get<1>(*it);

        if (&*task == task_id) {
          std::get<0>(*it) = std::chrono::steady_clock::now() + duration;

          break;
        }
      }

      if (it == _timer_tasks.cend()) {
        return;
      }

      // smaller time goes to the back
      auto prev = it - 1;
      while (it > _timer_tasks.cbegin()) {
        if (std::get<0>(*it) > std::get<0>(*prev)) {
          std::swap(*it, *prev);
        }

        --prev;
        --it;
      }
    }

    /**
     * @brief Cancel a scheduled task.
     * 
     * Removes a task from the timer queue if it hasn't been executed yet.
     * 
     * @param task_id The ID of the task to cancel.
     * @return True if task was found and cancelled, false otherwise.
     */
    bool cancel(task_id_t task_id) {
      std::lock_guard lg(_task_mutex);

      auto it = _timer_tasks.begin();
      for (; it < _timer_tasks.cend(); ++it) {
        const __task &task = std::get<1>(*it);

        if (&*task == task_id) {
          _timer_tasks.erase(it);

          return true;
        }
      }

      return false;
    }

    /**
     * @brief Pop a specific task from the timer queue.
     * 
     * Removes and returns a task with the given ID from the timer queue.
     * 
     * @param task_id The ID of the task to pop.
     * @return Optional pair of time point and task, or nullopt if not found.
     */
    std::optional<std::pair<__time_point, __task>> pop(task_id_t task_id) {
      std::lock_guard lg(_task_mutex);

      auto pos = std::find_if(std::begin(_timer_tasks), std::end(_timer_tasks), [&task_id](const auto &t) {
        return t.second.get() == task_id;
      });

      if (pos == std::end(_timer_tasks)) {
        return std::nullopt;
      }

      return std::move(*pos);
    }

    /**
     * @brief Pop the next ready task.
     * 
     * Returns the next task that's ready to execute (either from immediate queue
     * or from timer queue if its time has come).
     * 
     * @return Optional task, or nullopt if no task is ready.
     */
    std::optional<__task> pop() {
      std::lock_guard lg(_task_mutex);

      if (!_tasks.empty()) {
        __task task = std::move(_tasks.front());
        _tasks.pop_front();
        return task;
      }

      if (!_timer_tasks.empty() && std::get<0>(_timer_tasks.back()) <= std::chrono::steady_clock::now()) {
        __task task = std::move(std::get<1>(_timer_tasks.back()));
        _timer_tasks.pop_back();
        return task;
      }

      return std::nullopt;
    }

    /**
     * @brief Check if any task is ready to execute.
     * 
     * @return True if there's an immediate task or a timer task whose time has come.
     */
    bool ready() {
      std::lock_guard<std::mutex> lg(_task_mutex);

      return !_tasks.empty() || (!_timer_tasks.empty() && std::get<0>(_timer_tasks.back()) <= std::chrono::steady_clock::now());
    }

    /**
     * @brief Get the time point of the next scheduled task.
     * 
     * @return Optional time point of next timer task, or nullopt if no timer tasks.
     */
    std::optional<__time_point> next() {
      std::lock_guard<std::mutex> lg(_task_mutex);

      if (_timer_tasks.empty()) {
        return std::nullopt;
      }

      return std::get<0>(_timer_tasks.back());
    }

  private:
    /**
     * @brief Convert function to runnable task.
     * 
     * Wraps a function in a task implementation.
     * 
     * @tparam Function Function or callable type.
     * @param f Function to wrap.
     * @return Unique pointer to task implementation.
     */
    template<class Function>
    std::unique_ptr<_ImplBase> toRunnable(Function &&f) {
      return std::make_unique<_Impl<Function>>(std::forward<Function &&>(f));
    }
  };
}  // namespace task_pool_util
