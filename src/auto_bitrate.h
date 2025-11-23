/**
 * @file src/auto_bitrate.h
 * @brief Declarations for automatic bitrate adjustment controller.
 */
#pragma once

// standard includes
#include <chrono>
#include <unordered_map>

namespace stream {
  struct session_t;

  /**
   * @brief Automatic bitrate adjustment controller.
   *        Adaptively adjusts encoder bitrate based on network quality metrics.
   */
  class auto_bitrate_controller_t {
  public:
    auto_bitrate_controller_t();
    
    /**
     * @brief Process loss statistics from client.
     * @param session The streaming session.
     * @param lastGoodFrame Last successfully received frame number.
     * @param time_interval Time interval in milliseconds.
     */
    void process_loss_stats(session_t *session, 
                            uint64_t lastGoodFrame,
                            std::chrono::milliseconds time_interval);
    
    /**
     * @brief Process connection status change.
     * @param session The streaming session.
     * @param status New connection status (OKAY or POOR).
     */
    void process_connection_status(session_t *session,
                                   int status);
    
    /**
     * @brief Check if bitrate adjustment is needed.
     * @param session The streaming session.
     * @return true if adjustment should be made, false otherwise.
     */
    bool should_adjust_bitrate(session_t *session) const;
    
    /**
     * @brief Calculate new bitrate value.
     * @param session The streaming session.
     * @return New bitrate in Kbps.
     */
    int calculate_new_bitrate(session_t *session) const;
    
    /**
     * @brief Confirm that a bitrate change was successfully applied by the encoder.
     *        Updates the controller state only after encoder confirmation.
     * @param session The streaming session.
     * @param new_bitrate_kbps The bitrate that was successfully applied.
     * @param success true if encoder successfully applied the change, false otherwise.
     */
    void confirm_bitrate_change(session_t *session, int new_bitrate_kbps, bool success);
    
    /**
     * @brief Reset controller state for a new session.
     * @param session The streaming session.
     */
    void reset(session_t *session);
    
    /**
     * @brief Get bitrate statistics for a session.
     * @param session The streaming session.
     * @param current_bitrate_kbps Output: Current encoder bitrate in Kbps.
     * @param last_adjustment_time_ms Output: Milliseconds since session start when last successful adjustment occurred (0 if never).
     * @param adjustment_count Output: Total number of successful adjustments made.
     * @param loss_percentage Output: Current frame loss percentage.
     * @return true if stats are available, false otherwise.
     */
    bool get_stats(session_t *session,
                   uint32_t &current_bitrate_kbps,
                   uint64_t &last_adjustment_time_ms,
                   uint32_t &adjustment_count,
                   float &loss_percentage) const;
    
  private:
    struct session_state_t {
      uint64_t last_reported_good_frame = 0;
      std::chrono::steady_clock::time_point last_loss_stats_time;
      std::chrono::steady_clock::time_point last_adjustment_time;
      std::chrono::steady_clock::time_point last_successful_adjustment_time;
      std::chrono::steady_clock::time_point session_start_time;
      double loss_percentage = 0.0;
      int connection_status = 0;  // 0 = OKAY, 1 = POOR
      int current_bitrate_kbps = 0;
      uint32_t adjustment_count = 0;
    };
    
    std::unordered_map<session_t*, session_state_t> session_states;
    
    // Helper methods
    double compute_loss_percentage(session_t *session, 
                                   uint64_t lastGoodFrame,
                                   std::chrono::milliseconds time_interval) const;
    double get_adjustment_factor(const session_state_t &state, 
                                  std::chrono::steady_clock::time_point now) const;
    int clamp_bitrate(int bitrate, int min_bitrate, int max_bitrate) const;
    session_state_t &get_or_create_state(session_t *session);
  };
}

