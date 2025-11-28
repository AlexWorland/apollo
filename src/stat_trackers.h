/**
 * @file src/stat_trackers.h
 * @brief Declarations for streaming statistic tracking.
 */
#pragma once

// standard includes
#include <chrono>
#include <functional>
#include <limits>

// lib includes
#include <boost/format.hpp>

namespace stat_trackers {

  /**
   * @brief Create a boost::format for one digit after decimal point.
   * @return Format object for "%.1f" style formatting.
   */
  boost::format one_digit_after_decimal();

  /**
   * @brief Create a boost::format for two digits after decimal point.
   * @return Format object for "%.2f" style formatting.
   */
  boost::format two_digits_after_decimal();

  /**
   * @brief Template class for tracking minimum, maximum, and average values.
   * @tparam T The numeric type to track.
   */
  template<typename T>
  class min_max_avg_tracker {
  public:
    using callback_function = std::function<void(T stat_min, T stat_max, double stat_avg)>;  ///< Callback function type

    /**
     * @brief Collect a statistic value and call callback when interval elapses.
     * @param stat The statistic value to collect.
     * @param callback The callback function to invoke when interval elapses.
     * @param interval_in_seconds The time interval between callbacks.
     */
    void collect_and_callback_on_interval(T stat, const callback_function &callback, std::chrono::seconds interval_in_seconds) {
      if (data.calls == 0) {
        data.last_callback_time = std::chrono::steady_clock::now();
      } else if (std::chrono::steady_clock::now() > data.last_callback_time + interval_in_seconds) {
        callback(data.stat_min, data.stat_max, data.stat_total / data.calls);
        data = {};
      }
      data.stat_min = std::min(data.stat_min, stat);
      data.stat_max = std::max(data.stat_max, stat);
      data.stat_total += stat;
      data.calls += 1;
    }

    /**
     * @brief Reset all collected statistics.
     */
    void reset() {
      data = {};
    }

  private:
    /**
     * @brief Internal data structure for tracking statistics.
     */
    struct {
      std::chrono::steady_clock::time_point last_callback_time = std::chrono::steady_clock::now();  ///< Time of last callback
      T stat_min = std::numeric_limits<T>::max();  ///< Minimum value seen
      T stat_max = std::numeric_limits<T>::min();  ///< Maximum value seen
      double stat_total = 0;  ///< Sum of all values for average calculation
      uint32_t calls = 0;  ///< Number of values collected
    } data;
  };

}  // namespace stat_trackers
