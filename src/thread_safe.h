/**
 * @file src/thread_safe.h
 * @brief Declarations for thread-safe data structures.
 */
#pragma once

// standard includes
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

// local includes
#include "utility.h"

namespace safe {
  /**
   * @brief Thread-safe event/signal class.
   * 
   * Provides a thread-safe mechanism for signaling events between threads.
   * Supports waiting for events, timeout-based waiting, and can be stopped/reset.
   * Thread-safe: all operations are protected by internal mutex.
   * 
   * @tparam T The type of value to signal with the event.
   */
  template<class T>
  class event_t {
  public:
    using status_t = util::optional_t<T>;

    template<class... Args>
    void raise(Args &&...args) {
      std::lock_guard lg {_lock};
      if (!_continue) {
        return;
      }

      if constexpr (std::is_same_v<std::optional<T>, status_t>) {
        _status = std::make_optional<T>(std::forward<Args>(args)...);
      } else {
        _status = status_t {std::forward<Args>(args)...};
      }

      _cv.notify_all();
    }

    /**
     * @brief Pop and consume the event value.
     * 
     * Waits for the event to be raised, then returns and clears the value.
     * Blocks until the event is raised or the event is stopped.
     * 
     * @note pop() and view() should not be used interchangeably.
     * 
     * @return Event value, or false value if stopped.
     */
    status_t pop() {
      std::unique_lock ul {_lock};

      if (!_continue) {
        return util::false_v<status_t>;
      }

      while (!_status) {
        _cv.wait(ul);

        if (!_continue) {
          return util::false_v<status_t>;
        }
      }

      auto val = std::move(_status);
      _status = util::false_v<status_t>;
      return val;
    }

    /**
     * @brief Pop and consume the event value with timeout.
     * 
     * Waits for the event to be raised with a timeout, then returns and clears the value.
     * 
     * @note pop() and view() should not be used interchangeably.
     * 
     * @param delay Maximum time to wait.
     * @return Event value, or false value if timeout or stopped.
     */
    template<class Rep, class Period>
    status_t pop(std::chrono::duration<Rep, Period> delay) {
      std::unique_lock ul {_lock};

      if (!_continue) {
        return util::false_v<status_t>;
      }

      while (!_status) {
        if (!_continue || _cv.wait_for(ul, delay) == std::cv_status::timeout) {
          return util::false_v<status_t>;
        }
      }

      auto val = std::move(_status);
      _status = util::false_v<status_t>;
      return val;
    }

    /**
     * @brief View the event value without consuming it.
     * 
     * Waits for the event to be raised, then returns the value without clearing it.
     * Blocks until the event is raised or the event is stopped.
     * 
     * @note pop() and view() should not be used interchangeably.
     * 
     * @return Event value, or false value if stopped.
     */
    status_t view() {
      std::unique_lock ul {_lock};

      if (!_continue) {
        return util::false_v<status_t>;
      }

      while (!_status) {
        _cv.wait(ul);

        if (!_continue) {
          return util::false_v<status_t>;
        }
      }

      return _status;
    }

    /**
     * @brief View the event value with timeout.
     * 
     * Waits for the event to be raised with a timeout, then returns the value without clearing it.
     * 
     * @note pop() and view() should not be used interchangeably.
     * 
     * @param delay Maximum time to wait.
     * @return Event value, or false value if timeout or stopped.
     */
    template<class Rep, class Period>
    status_t view(std::chrono::duration<Rep, Period> delay) {
      std::unique_lock ul {_lock};

      if (!_continue) {
        return util::false_v<status_t>;
      }

      while (!_status) {
        if (!_continue || _cv.wait_for(ul, delay) == std::cv_status::timeout) {
          return util::false_v<status_t>;
        }
      }

      return _status;
    }

    /**
     * @brief Check if event has a value without blocking.
     * 
     * @return True if event is running and has a value, false otherwise.
     */
    bool peek() {
      return _continue && (bool) _status;
    }

    /**
     * @brief Stop the event.
     * 
     * Stops the event and notifies all waiting threads.
     * After stopping, raise() will have no effect and pop()/view() will return false values.
     */
    void stop() {
      std::lock_guard lg {_lock};

      _continue = false;

      _cv.notify_all();
    }

    /**
     * @brief Reset the event to initial state.
     * 
     * Clears the event value and allows it to be used again.
     */
    void reset() {
      std::lock_guard lg {_lock};

      _continue = true;

      _status = util::false_v<status_t>;
    }

    /**
     * @brief Check if event is running.
     * 
     * @return True if event is running (not stopped), false otherwise.
     */
    [[nodiscard]] bool running() const {
      return _continue;
    }

  private:
    bool _continue {true};
    status_t _status {util::false_v<status_t>};

    std::condition_variable _cv;
    std::mutex _lock;
  };

  /**
   * @brief Thread-safe alarm class.
   * 
   * Provides a thread-safe alarm mechanism that can be "rung" to signal
   * waiting threads. Supports timeout-based waiting and predicate-based waiting.
   * Thread-safe: all operations are protected by internal mutex.
   * 
   * @tparam T The type of status value associated with the alarm.
   */
  template<class T>
  class alarm_raw_t {
  public:
    using status_t = util::optional_t<T>;

    /**
     * @brief Ring the alarm with a status value (copy).
     * 
     * Sets the alarm status and notifies one waiting thread.
     * 
     * @param status Status value to set.
     */
    void ring(const status_t &status) {
      std::lock_guard lg(_lock);

      _status = status;
      _rang = true;
      _cv.notify_one();
    }

    /**
     * @brief Ring the alarm with a status value (move).
     * 
     * Sets the alarm status and notifies one waiting thread.
     * 
     * @param status Status value to set (moved).
     */
    void ring(status_t &&status) {
      std::lock_guard lg(_lock);

      _status = std::move(status);
      _rang = true;
      _cv.notify_one();
    }

    /**
     * @brief Wait for alarm to ring with timeout.
     * 
     * @param rel_time Maximum time to wait.
     * @return True if alarm rang, false if timeout.
     */
    template<class Rep, class Period>
    auto wait_for(const std::chrono::duration<Rep, Period> &rel_time) {
      std::unique_lock ul(_lock);

      return _cv.wait_for(ul, rel_time, [this]() {
        return _rang;
      });
    }

    /**
     * @brief Wait for alarm to ring with timeout and predicate.
     * 
     * @param rel_time Maximum time to wait.
     * @param pred Predicate to check (waits if alarm rang OR predicate is true).
     * @return True if alarm rang or predicate is true, false if timeout.
     */
    template<class Rep, class Period, class Pred>
    auto wait_for(const std::chrono::duration<Rep, Period> &rel_time, Pred &&pred) {
      std::unique_lock ul(_lock);

      return _cv.wait_for(ul, rel_time, [this, &pred]() {
        return _rang || pred();
      });
    }

    /**
     * @brief Wait until absolute time for alarm to ring.
     * 
     * @param rel_time Absolute time point to wait until.
     * @return True if alarm rang, false if timeout.
     */
    template<class Rep, class Period>
    auto wait_until(const std::chrono::duration<Rep, Period> &rel_time) {
      std::unique_lock ul(_lock);

      return _cv.wait_until(ul, rel_time, [this]() {
        return _rang;
      });
    }

    /**
     * @brief Wait until absolute time for alarm to ring with predicate.
     * 
     * @param rel_time Absolute time point to wait until.
     * @param pred Predicate to check (waits if alarm rang OR predicate is true).
     * @return True if alarm rang or predicate is true, false if timeout.
     */
    template<class Rep, class Period, class Pred>
    auto wait_until(const std::chrono::duration<Rep, Period> &rel_time, Pred &&pred) {
      std::unique_lock ul(_lock);

      return _cv.wait_until(ul, rel_time, [this, &pred]() {
        return _rang || pred();
      });
    }

    /**
     * @brief Wait indefinitely for alarm to ring.
     */
    auto wait() {
      std::unique_lock ul(_lock);
      _cv.wait(ul, [this]() {
        return _rang;
      });
    }

    /**
     * @brief Wait indefinitely for alarm to ring with predicate.
     * 
     * @param pred Predicate to check (waits if alarm rang OR predicate is true).
     */
    template<class Pred>
    auto wait(Pred &&pred) {
      std::unique_lock ul(_lock);
      _cv.wait(ul, [this, &pred]() {
        return _rang || pred();
      });
    }

    /**
     * @brief Get alarm status (const).
     * @return Const reference to status value.
     */
    const status_t &status() const {
      return _status;
    }

    /**
     * @brief Get alarm status.
     * @return Reference to status value.
     */
    status_t &status() {
      return _status;
    }

    /**
     * @brief Reset alarm to initial state.
     * 
     * Clears the status and resets the rang flag.
     */
    void reset() {
      _status = status_t {};
      _rang = false;
    }

  private:
    std::mutex _lock;
    std::condition_variable _cv;

    status_t _status {util::false_v<status_t>};
    bool _rang {false};
  };

  /**
   * @brief Shared pointer type alias for alarm.
   * 
   * @tparam T Status type.
   */
  template<class T>
  using alarm_t = std::shared_ptr<alarm_raw_t<T>>;

  /**
   * @brief Create a new alarm.
   * 
   * @tparam T Status type.
   * @return Shared pointer to new alarm.
   */
  template<class T>
  alarm_t<T> make_alarm() {
    return std::make_shared<alarm_raw_t<T>>();
  }

  /**
   * @brief Thread-safe queue class.
   * 
   * Provides a thread-safe FIFO queue with a maximum size limit.
   * When the queue reaches maximum capacity, it automatically clears.
   * Thread-safe: all operations are protected by internal mutex.
   * 
   * @tparam T The type of elements stored in the queue.
   */
  template<class T>
  class queue_t {
  public:
    using status_t = util::optional_t<T>;

    queue_t(std::uint32_t max_elements = 32):
        _max_elements {max_elements} {
    }

    /**
     * @brief Add element to queue.
     * 
     * Adds an element to the queue. If the queue is full, it clears before adding.
     * Notifies all waiting threads.
     * 
     * @param args Arguments to construct the element.
     */
    template<class... Args>
    void raise(Args &&...args) {
      std::lock_guard ul {_lock};

      if (!_continue) {
        return;
      }

      if (_queue.size() == _max_elements) {
        _queue.clear();
      }

      _queue.emplace_back(std::forward<Args>(args)...);

      _cv.notify_all();
    }

    /**
     * @brief Check if queue has elements without blocking.
     * 
     * @return True if queue is running and has elements, false otherwise.
     */
    bool peek() {
      return _continue && !_queue.empty();
    }

    /**
     * @brief Pop element from queue with timeout.
     * 
     * Waits for an element to be available, then removes and returns it.
     * 
     * @param delay Maximum time to wait.
     * @return Element value, or false value if timeout or stopped.
     */
    template<class Rep, class Period>
    status_t pop(std::chrono::duration<Rep, Period> delay) {
      std::unique_lock ul {_lock};

      if (!_continue) {
        return util::false_v<status_t>;
      }

      while (_queue.empty()) {
        if (!_continue || _cv.wait_for(ul, delay) == std::cv_status::timeout) {
          return util::false_v<status_t>;
        }
      }

      auto val = std::move(_queue.front());
      _queue.erase(std::begin(_queue));

      return val;
    }

    /**
     * @brief Pop element from queue.
     * 
     * Waits indefinitely for an element to be available, then removes and returns it.
     * 
     * @return Element value, or false value if stopped.
     */
    status_t pop() {
      std::unique_lock ul {_lock};

      if (!_continue) {
        return util::false_v<status_t>;
      }

      while (_queue.empty()) {
        _cv.wait(ul);

        if (!_continue) {
          return util::false_v<status_t>;
        }
      }

      auto val = std::move(_queue.front());
      _queue.erase(std::begin(_queue));

      return val;
    }

    /**
     * @brief Get unsafe access to underlying vector.
     * 
     * @warning Not thread-safe. Only use when you have external synchronization.
     * 
     * @return Reference to underlying vector.
     */
    std::vector<T> &unsafe() {
      return _queue;
    }

    /**
     * @brief Stop the queue.
     * 
     * Stops the queue and notifies all waiting threads.
     * After stopping, raise() will have no effect and pop() will return false values.
     */
    void stop() {
      std::lock_guard lg {_lock};

      _continue = false;

      _cv.notify_all();
    }

    /**
     * @brief Check if queue is running.
     * 
     * @return True if queue is running (not stopped), false otherwise.
     */
    [[nodiscard]] bool running() const {
      return _continue;
    }

  private:
    bool _continue {true};
    std::uint32_t _max_elements;

    std::mutex _lock;
    std::condition_variable _cv;

    std::vector<T> _queue;
  };

  /**
   * @brief Thread-safe shared object manager.
   * 
   * Manages a single instance of an object with reference counting.
   * The object is constructed on first access and destructed when the last
   * reference is released. Thread-safe: all operations are protected by internal mutex.
   * 
   * @tparam T The type of object to manage.
   */
  template<class T>
  class shared_t {
  public:
    using element_type = T;

    using construct_f = std::function<int(element_type &)>;
    using destruct_f = std::function<void(element_type &)>;

    /**
     * @brief Smart pointer to a shared object.
     * 
     * Manages a reference to a shared object. Automatically releases
     * the reference when destroyed. Copying increments the reference count.
     */
    struct ptr_t {
      shared_t *owner;

      /**
       * @brief Default constructor (null pointer).
       */
      ptr_t():
          owner {nullptr} {
      }

      /**
       * @brief Construct from owner.
       * 
       * @param owner Shared object manager.
       */
      explicit ptr_t(shared_t *owner):
          owner {owner} {
      }

      /**
       * @brief Move constructor.
       * 
       * @param ptr Pointer to move from.
       */
      ptr_t(ptr_t &&ptr) noexcept:
          owner {ptr.owner} {
        ptr.owner = nullptr;
      }

      /**
       * @brief Copy constructor.
       * 
       * Increments reference count.
       * 
       * @param ptr Pointer to copy from.
       */
      ptr_t(const ptr_t &ptr) noexcept:
          owner {ptr.owner} {
        if (!owner) {
          return;
        }

        auto tmp = ptr.owner->ref();
        tmp.owner = nullptr;
      }

      /**
       * @brief Copy assignment operator.
       * 
       * Releases current reference and increments new reference count.
       * 
       * @param ptr Pointer to copy from.
       * @return Reference to this.
       */
      ptr_t &operator=(const ptr_t &ptr) noexcept {
        if (!ptr.owner) {
          release();

          return *this;
        }

        return *this = std::move(*ptr.owner->ref());
      }

      /**
       * @brief Move assignment operator.
       * 
       * @param ptr Pointer to move from.
       * @return Reference to this.
       */
      ptr_t &operator=(ptr_t &&ptr) noexcept {
        if (owner) {
          release();
        }

        std::swap(owner, ptr.owner);

        return *this;
      }

      /**
       * @brief Destructor.
       * 
       * Automatically releases reference.
       */
      ~ptr_t() {
        if (owner) {
          release();
        }
      }

      /**
       * @brief Boolean conversion operator.
       * 
       * @return True if pointer is valid, false otherwise.
       */
      operator bool() const {
        return owner != nullptr;
      }

      /**
       * @brief Release reference to shared object.
       * 
       * Decrements reference count. If count reaches zero, calls destructor
       * and cleanup function.
       */
      void release() {
        std::lock_guard lg {owner->_lock};

        if (!--owner->_count) {
          owner->_destruct(*get());
          (*this)->~element_type();
        }

        owner = nullptr;
      }

      /**
       * @brief Get raw pointer to shared object.
       * 
       * @return Pointer to shared object.
       */
      element_type *get() const {
        return reinterpret_cast<element_type *>(owner->_object_buf.data());
      }

      /**
       * @brief Member access operator.
       * 
       * @return Pointer to shared object.
       */
      element_type *operator->() {
        return reinterpret_cast<element_type *>(owner->_object_buf.data());
      }
    };

    template<class FC, class FD>
    shared_t(FC &&fc, FD &&fd):
        _construct {std::forward<FC>(fc)},
        _destruct {std::forward<FD>(fd)} {
    }

    /**
     * @brief Get a reference to the shared object.
     * 
     * If this is the first reference, constructs the object.
     * Increments reference count.
     * 
     * @return Pointer to shared object reference.
     */
    [[nodiscard]] ptr_t ref() {
      std::lock_guard lg {_lock};

      if (!_count) {
        new (_object_buf.data()) element_type;
        if (_construct(*reinterpret_cast<element_type *>(_object_buf.data()))) {
          return ptr_t {nullptr};
        }
      }

      ++_count;

      return ptr_t {this};
    }

  private:
    construct_f _construct;
    destruct_f _destruct;

    std::array<std::uint8_t, sizeof(element_type)> _object_buf;

    std::uint32_t _count;
    std::mutex _lock;
  };

  /**
   * @brief Create a shared object manager.
   * 
   * Factory function to create a shared_t with construct and destruct functions.
   * 
   * @tparam T Object type.
   * @tparam F_Construct Construct function type.
   * @tparam F_Destruct Destruct function type.
   * @param fc Construct function (returns 0 on success, non-zero on failure).
   * @param fd Destruct function.
   * @return Shared object manager.
   */
  template<class T, class F_Construct, class F_Destruct>
  auto make_shared(F_Construct &&fc, F_Destruct &&fd) {
    return shared_t<T> {
      std::forward<F_Construct>(fc),
      std::forward<F_Destruct>(fd)
    };
  }

  /**
   * @brief Signal type alias.
   * 
   * Event type for boolean signals (simple on/off events).
   */
  using signal_t = event_t<bool>;

  class mail_raw_t;
  
  /**
   * @brief Mail container shared pointer type.
   */
  using mail_t = std::shared_ptr<mail_raw_t>;

  void cleanup(mail_raw_t *);

  /**
   * @brief Post wrapper for mail-based communication.
   * 
   * Wraps an event or queue with automatic cleanup when destroyed.
   * Ensures proper cleanup of mail resources when the post goes out of scope.
   * 
   * @tparam T The type being wrapped (typically event_t or queue_t).
   */
  template<class T>
  class post_t: public T {
  public:
    /**
     * @brief Construct post wrapper.
     * 
     * @param mail Mail container for cleanup.
     * @param args Arguments to forward to base class constructor.
     */
    template<class... Args>
    post_t(mail_t mail, Args &&...args):
        T(std::forward<Args>(args)...),
        mail {std::move(mail)} {
    }

    /**
     * @brief Mail container reference.
     * 
     * Used for cleanup when post is destroyed.
     */
    mail_t mail;

    /**
     * @brief Destructor.
     * 
     * Automatically cleans up mail container.
     */
    ~post_t() {
      cleanup(mail.get());
    }
  };

  /**
   * @brief Lock weak pointer and cast to specific type.
   * 
   * @tparam T Target type (must have element_type member).
   * @param wp Weak pointer to lock.
   * @return Shared pointer to locked object, or nullptr if expired.
   */
  template<class T>
  inline auto lock(const std::weak_ptr<void> &wp) {
    return std::reinterpret_pointer_cast<typename T::element_type>(wp.lock());
  }

  /**
   * @brief Mail container for named events and queues.
   * 
   * Provides a registry for named thread-safe events and queues.
   * Allows multiple components to access the same event/queue by name.
   * Automatically cleans up expired references.
   */
  class mail_raw_t: public std::enable_shared_from_this<mail_raw_t> {
  public:
    template<class T>
    using event_t = std::shared_ptr<post_t<event_t<T>>>;

    template<class T>
    using queue_t = std::shared_ptr<post_t<queue_t<T>>>;

    /**
     * @brief Get or create an event by ID.
     * 
     * Returns existing event if found, otherwise creates a new one.
     * 
     * @tparam T Event value type.
     * @param id Event identifier.
     * @return Shared pointer to event.
     */
    template<class T>
    event_t<T> event(const std::string_view &id) {
      std::lock_guard lg {mutex};

      auto it = id_to_post.find(id);
      if (it != std::end(id_to_post)) {
        return lock<event_t<T>>(it->second);
      }

      auto post = std::make_shared<typename event_t<T>::element_type>(shared_from_this());
      id_to_post.emplace(std::pair<std::string, std::weak_ptr<void>> {std::string {id}, post});

      return post;
    }

    /**
     * @brief Get or create a queue by ID.
     * 
     * Returns existing queue if found, otherwise creates a new one.
     * 
     * @tparam T Queue element type.
     * @param id Queue identifier.
     * @return Shared pointer to queue.
     */
    template<class T>
    queue_t<T> queue(const std::string_view &id) {
      std::lock_guard lg {mutex};

      auto it = id_to_post.find(id);
      if (it != std::end(id_to_post)) {
        return lock<queue_t<T>>(it->second);
      }

      auto post = std::make_shared<typename queue_t<T>::element_type>(shared_from_this(), 32);
      id_to_post.emplace(std::pair<std::string, std::weak_ptr<void>> {std::string {id}, post});

      return post;
    }

    /**
     * @brief Clean up expired references.
     * 
     * Removes entries for expired weak pointers from the registry.
     */
    void cleanup() {
      std::lock_guard lg {mutex};

      for (auto it = std::begin(id_to_post); it != std::end(id_to_post); ++it) {
        auto &weak = it->second;

        if (weak.expired()) {
          id_to_post.erase(it);

          return;
        }
      }
    }

    std::mutex mutex;

    std::map<std::string, std::weak_ptr<void>, std::less<>> id_to_post;
  };

  /**
   * @brief Clean up mail container.
   * 
   * Helper function to clean up expired references in mail container.
   * 
   * @param mail Mail container to clean up.
   */
  inline void cleanup(mail_raw_t *mail) {
    mail->cleanup();
  }
}  // namespace safe
