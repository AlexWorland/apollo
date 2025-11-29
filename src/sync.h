/**
 * @file src/sync.h
 * @brief Declarations for synchronization utilities.
 */
#pragma once

// standard includes
#include <array>
#include <mutex>
#include <utility>

namespace sync_util {

  /**
   * @brief Synchronized value wrapper.
   * 
   * Wraps a value with a mutex to provide thread-safe access.
   * 
   * @tparam T Value type to synchronize.
   * @tparam M Mutex type (defaults to std::mutex).
   */
  template<class T, class M = std::mutex>
  class sync_t {
  public:
    using value_t = T;  ///< Value type.
    using mutex_t = M;  ///< Mutex type.

    /**
     * @brief Acquire lock on the synchronized value.
     * 
     * Returns a lock guard that will automatically unlock when destroyed.
     * 
     * @return Lock guard for the mutex.
     */
    std::lock_guard<mutex_t> lock() {
      return std::lock_guard {_lock};
    }

    /**
     * @brief Construct synchronized value.
     * 
     * @param args Arguments to forward to value constructor.
     */
    template<class... Args>
    sync_t(Args &&...args):
        raw {std::forward<Args>(args)...} {
    }

    /**
     * @brief Move assignment operator.
     * 
     * Thread-safely moves value from another sync_t.
     * 
     * @param other Other sync_t to move from.
     * @return Reference to this.
     */
    sync_t &operator=(sync_t &&other) noexcept {
      std::lock(_lock, other._lock);

      raw = std::move(other.raw);

      _lock.unlock();
      other._lock.unlock();

      return *this;
    }

    /**
     * @brief Copy assignment operator.
     * 
     * Thread-safely copies value from another sync_t.
     * 
     * @param other Other sync_t to copy from.
     * @return Reference to this.
     */
    sync_t &operator=(sync_t &other) noexcept {
      std::lock(_lock, other._lock);

      raw = other.raw;

      _lock.unlock();
      other._lock.unlock();

      return *this;
    }

    /**
     * @brief Assignment operator for compatible types.
     * 
     * Thread-safely assigns a value of compatible type.
     * 
     * @tparam V Value type (must be assignable to T).
     * @param val Value to assign.
     * @return Reference to this.
     */
    template<class V>
    sync_t &operator=(V &&val) {
      auto lg = lock();

      raw = val;

      return *this;
    }

    /**
     * @brief Assignment operator for const value.
     * 
     * Thread-safely assigns a const value.
     * 
     * @param val Value to assign.
     * @return Reference to this.
     */
    sync_t &operator=(const value_t &val) noexcept {
      auto lg = lock();

      raw = val;

      return *this;
    }

    /**
     * @brief Assignment operator for rvalue value.
     * 
     * Thread-safely moves a value.
     * 
     * @param val Value to move.
     * @return Reference to this.
     */
    sync_t &operator=(value_t &&val) noexcept {
      auto lg = lock();

      raw = std::move(val);

      return *this;
    }

    /**
     * @brief Member access operator.
     * 
     * @warning Not thread-safe. Use lock() for thread-safe access.
     * 
     * @return Pointer to raw value.
     */
    value_t *operator->() {
      return &raw;
    }

    /**
     * @brief Dereference operator.
     * 
     * @warning Not thread-safe. Use lock() for thread-safe access.
     * 
     * @return Reference to raw value.
     */
    value_t &operator*() {
      return raw;
    }

    /**
     * @brief Dereference operator (const).
     * 
     * @warning Not thread-safe. Use lock() for thread-safe access.
     * 
     * @return Const reference to raw value.
     */
    const value_t &operator*() const {
      return raw;
    }

    /**
     * @brief Raw value access.
     * 
     * @warning Direct access is not thread-safe. Use lock() for thread-safe access.
     */
    value_t raw;

  private:
    mutex_t _lock;
  };

}  // namespace sync_util
