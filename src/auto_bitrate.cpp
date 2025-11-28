/**
 * @file src/auto_bitrate.cpp
 * @brief Definitions for automatic bitrate adjustment controller.
 */

// local includes
#include "auto_bitrate.h"
#include "config.h"
#include "logging.h"
#include "stream.h"

namespace stream {

  /**
   * @brief Constructs an auto bitrate controller.
   *        Initializes an empty controller with no active sessions.
   */
  auto_bitrate_controller_t::auto_bitrate_controller_t() {
  }

  void auto_bitrate_controller_t::process_loss_stats(session_t *session, 
                                                      uint64_t lastGoodFrame,
                                                      std::chrono::milliseconds time_interval) {
    if (!session || !session->auto_bitrate_enabled) {
      return;
    }

    auto &state = get_or_create_state(session);
    auto now = std::chrono::steady_clock::now();

    // Compute loss percentage
    state.loss_percentage = compute_loss_percentage(session, lastGoodFrame, time_interval);
    state.last_reported_good_frame = lastGoodFrame;
    state.last_loss_stats_time = now;

    // Note: current_bitrate_kbps is initialized in get_or_create_state() and
    // updated in calculate_new_bitrate() when adjustments are made.
    // We don't overwrite it here to preserve adjusted values.
  }

  void auto_bitrate_controller_t::process_loss_stats_direct(session_t *session,
                                                            double loss_percentage_loss_pct,
                                                            uint64_t lastGoodFrame,
                                                            std::chrono::milliseconds /*time_interval*/) {
    if (!session || !session->auto_bitrate_enabled) {
      return;
    }

    auto &state = get_or_create_state(session);
    auto now = std::chrono::steady_clock::now();

    state.loss_percentage = loss_percentage_loss_pct;
    state.last_reported_good_frame = lastGoodFrame;
    state.last_loss_stats_time = now;
  }

  void auto_bitrate_controller_t::process_connection_status(session_t *session,
                                                              int status) {
    if (!session || !session->auto_bitrate_enabled) {
      return;
    }

    auto &state = get_or_create_state(session);
    state.connection_status = status;  // 0 = OKAY, 1 = POOR
  }

  bool auto_bitrate_controller_t::should_adjust_bitrate(session_t *session) const {
    if (!session || !session->auto_bitrate_enabled) {
      return false;
    }

    auto it = session_states.find(session);
    if (it == session_states.end()) {
      return false;
    }

    const auto &state = it->second;
    const auto settings = config::get_auto_bitrate_settings();
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_adjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - state.last_adjustment_time).count();

    // Minimum interval between adjustments (default 3 seconds)
    int min_interval_ms = settings.adjustment_interval_ms;
    if (min_interval_ms <= 0) {
      min_interval_ms = 3000;  // Default to 3 seconds
    }

    // Check minimum interval requirement
    if (time_since_last_adjustment < min_interval_ms) {
      return false;
    }

    // Calculate what the new bitrate would be
    double adjustment_factor = get_adjustment_factor(state, now, settings);
    
    // Only adjust if change is significant (configurable threshold)
    int min_adjustment_pct = settings.min_adjustment_pct;
    if (min_adjustment_pct < 0) {
      min_adjustment_pct = 5;  // Default to 5%
    }

    auto min_adjustment_factor = static_cast<double>(min_adjustment_pct) / 100.0;
    if ((min_adjustment_pct == 0 && adjustment_factor == 1.0) ||
        (min_adjustment_pct > 0 && std::abs(adjustment_factor - 1.0) < min_adjustment_factor)) {
      return false;
    }

    return true;
  }

  int auto_bitrate_controller_t::calculate_new_bitrate(session_t *session) const {
    if (!session || !session->auto_bitrate_enabled) {
      return session->config.monitor.bitrate;
    }

    auto it = session_states.find(session);
    if (it == session_states.end()) {
      return session->config.monitor.bitrate;
    }

    const auto &state = it->second;
    const auto settings = config::get_auto_bitrate_settings();
    auto now = std::chrono::steady_clock::now();
    
    double adjustment_factor = get_adjustment_factor(state, now, settings);
    int new_bitrate = static_cast<int>(state.current_bitrate_kbps * adjustment_factor);

    // Get min/max bounds
    // Start with client-provided values (if set)
    int client_min = session->auto_bitrate_min_kbps > 0 ? session->auto_bitrate_min_kbps : 0;
    int client_max = session->auto_bitrate_max_kbps > 0 ? session->auto_bitrate_max_kbps : 0;
    
    // Get server config bounds
    int server_min = settings.min_kbps;
    if (server_min <= 0) {
      server_min = 1;  // Default minimum (changed from 500 to 1 Kbps)
    }
    
    int server_max = settings.max_kbps;
    if (server_max <= 0) {
      // Use config max_bitrate cap if set, otherwise no server limit
      server_max = settings.max_bitrate_cap > 0 ? settings.max_bitrate_cap : 0;
    } else {
      // Server max is set, apply cap if configured
      if (settings.max_bitrate_cap > 0 && settings.max_bitrate_cap < server_max) {
        server_max = settings.max_bitrate_cap;
      }
    }
    
    // Calculate final bounds: use client values as base, clamp by server config
    int min_bitrate = client_min > 0 ? client_min : server_min;
    // Apply server minimum clamp (server min is absolute minimum)
    if (server_min > 0 && min_bitrate < server_min) {
      min_bitrate = server_min;
    }
    
    int max_bitrate;
    if (client_max > 0) {
      max_bitrate = client_max;
      // Apply server maximum clamp (server max is absolute maximum if set)
      if (server_max > 0 && max_bitrate > server_max) {
        max_bitrate = server_max;
      }
    } else {
      // No client max provided, use server max or configured bitrate
      if (server_max > 0) {
        max_bitrate = server_max;
      } else {
        max_bitrate = session->config.monitor.bitrate;
        // Ensure max is at least 1 Kbps (safety check)
        if (max_bitrate < 1) {
          max_bitrate = 1000;  // Fallback to 1 Mbps if configured bitrate is invalid
        }
      }
    }
    
    // Ensure min <= max
    if (min_bitrate > max_bitrate) {
      min_bitrate = max_bitrate;
    }
    
    // Final safety check: ensure both are at least 1 Kbps
    if (min_bitrate < 1) {
      min_bitrate = 1;
    }
    if (max_bitrate < 1) {
      max_bitrate = 1;
    }

    new_bitrate = clamp_bitrate(new_bitrate, min_bitrate, max_bitrate);

    // Note: State is NOT updated here. It will be updated in confirm_bitrate_change()
    // after the encoder successfully applies the change. This prevents the state from
    // reflecting a bitrate that the encoder never actually used.

    return new_bitrate;
  }

  void auto_bitrate_controller_t::confirm_bitrate_change(session_t *session, int new_bitrate_kbps, bool success) {
    if (!session || !session->auto_bitrate_enabled) {
      return;
    }

    auto &state = get_or_create_state(session);
    auto now = std::chrono::steady_clock::now();

    // Always update last_adjustment_time to prevent immediate retries, even on failure.
    // This ensures should_adjust_bitrate() respects the configured backoff interval
    // (auto_bitrate_adjustment_interval_ms) and prevents endless retry loops when the
    // encoder doesn't support runtime reconfiguration.
    state.last_adjustment_time = now;

    if (success) {
      // Only update bitrate and count if the encoder successfully applied the change
      if (new_bitrate_kbps != state.current_bitrate_kbps) {
        state.adjustment_count++;
        state.current_bitrate_kbps = new_bitrate_kbps;
        state.last_successful_adjustment_time = now;
      }
    }
    // If success is false, we don't update current_bitrate_kbps or adjustment_count
    // since the encoder didn't apply the change, so the state should continue to
    // reflect the previous (still active) bitrate.
  }

  void auto_bitrate_controller_t::reset(session_t *session) {
    if (session) {
      session_states.erase(session);
    }
  }

  /**
   * @brief Computes frame loss percentage based on expected vs actual frame progression.
   * 
   * This method calculates packet loss by comparing the expected frame number (based on
   * framerate and time elapsed) with the last good frame reported by the client.
   * 
   * Algorithm:
   * 1. Get the last reported good frame from session state
   * 2. Calculate expected frames based on framerate and time interval
   * 3. Compute expected current frame = last_reported + expected_frames
   * 4. If client's lastGoodFrame < expected_current_frame, frames were lost
   * 5. Loss percentage = (lost_frames / expected_frames) * 100
   * 
   * @param session The streaming session.
   * @param lastGoodFrame Last successfully received frame number from client.
   * @param time_interval Time interval in milliseconds since last report.
   * @return Loss percentage (0-100), or 0.0 if calculation cannot be performed.
   */
  double auto_bitrate_controller_t::compute_loss_percentage(session_t *session, 
                                                             uint64_t lastGoodFrame,
                                                             std::chrono::milliseconds time_interval) const {
    auto it = session_states.find(session);
    if (it == session_states.end()) {
      return 0.0;
    }

    const auto &state = it->second;
    
    // First report from client - no baseline to compare against yet
    if (state.last_reported_good_frame == 0) {
      return 0.0;
    }

    // Calculate expected frame progression based on configured framerate
    // Framerate may be in fps (if <= 1000) or millifps (if > 1000)
    float framerate = static_cast<float>(session->config.monitor.framerate);
    if (framerate > 1000) {
      framerate = framerate / 1000.0f;  // Convert from millifps to fps
    }

    // Expected frames = framerate (fps) * time_interval (seconds)
    float expected_frames = framerate * (time_interval.count() / 1000.0f);
    uint64_t expected_current_frame = state.last_reported_good_frame + static_cast<uint64_t>(expected_frames);

    // Calculate actual loss: if client reports a frame number less than expected,
    // the difference represents lost frames
    int64_t loss_count = 0;
    if (lastGoodFrame < expected_current_frame) {
      loss_count = static_cast<int64_t>(expected_current_frame - lastGoodFrame);
    }

    // Safety check: avoid division by zero
    if (expected_frames <= 0) {
      return 0.0;
    }

    // Convert loss count to percentage
    double loss_percentage = (static_cast<double>(loss_count) / expected_frames) * 100.0;
    return loss_percentage;
  }

  double auto_bitrate_controller_t::get_adjustment_factor(const session_state_t &state, 
                                                          std::chrono::steady_clock::time_point now,
                                                          const config::auto_bitrate_settings_t &settings) const {
    double adjustment_factor = 1.0;

    auto severe_threshold = std::max(0, settings.loss_severe_pct);
    auto moderate_threshold = std::max(0, settings.loss_moderate_pct);
    auto mild_threshold = std::max(0, settings.loss_mild_pct);

    auto severe_reduction_pct = std::max(0, settings.decrease_severe_pct);
    auto moderate_reduction_pct = std::max(0, settings.decrease_moderate_pct);
    auto mild_reduction_pct = std::max(0, settings.decrease_mild_pct);
    auto increase_pct = std::max(0, settings.increase_good_pct);
    auto poor_status_cap_pct = std::max(0, settings.poor_status_cap_pct);

    // Determine base adjustment factor from loss
    if (state.loss_percentage > severe_threshold) {
      adjustment_factor = 1.0 - (severe_reduction_pct / 100.0);
    } else if (state.loss_percentage > moderate_threshold) {
      adjustment_factor = 1.0 - (moderate_reduction_pct / 100.0);
    } else if (state.loss_percentage > mild_threshold) {
      adjustment_factor = 1.0 - (mild_reduction_pct / 100.0);
    } else {
      // Consider increase only if stable
      auto time_since_last_adjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - state.last_adjustment_time).count();
      
      // Require configured duration of good conditions for increase
      if (time_since_last_adjustment >= settings.good_stability_ms && state.connection_status == 0) {
        adjustment_factor = 1.0 + (increase_pct / 100.0);
      } else {
        return 1.0;  // No change - return multiplier factor, not absolute bitrate
      }
    }

    // Apply connection status override
    if (state.connection_status == 1) {  // POOR
      adjustment_factor = std::min(adjustment_factor, 1.0 - (poor_status_cap_pct / 100.0));
    }

    // Check minimum interval requirement for increases
    auto time_since_last_adjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - state.last_adjustment_time).count();
    
    if (adjustment_factor > 1.0 && time_since_last_adjustment < settings.increase_min_interval_ms) {
      return 1.0;  // Too soon for increase
    }

    return adjustment_factor;
  }

  int auto_bitrate_controller_t::clamp_bitrate(int bitrate, int min_bitrate, int max_bitrate) const {
    if (bitrate < min_bitrate) {
      return min_bitrate;
    }
    if (bitrate > max_bitrate) {
      return max_bitrate;
    }
    return bitrate;
  }

  auto_bitrate_controller_t::session_state_t &auto_bitrate_controller_t::get_or_create_state(session_t *session) {
    auto it = session_states.find(session);
    if (it == session_states.end()) {
      session_state_t new_state;
      new_state.current_bitrate_kbps = session->config.monitor.bitrate;
      auto now = std::chrono::steady_clock::now();
      new_state.session_start_time = now;
      new_state.last_adjustment_time = now;
      new_state.last_successful_adjustment_time = now;
      new_state.last_loss_stats_time = now;
      new_state.adjustment_count = 0;
      it = session_states.emplace(session, new_state).first;
    }
    return it->second;
  }

  bool auto_bitrate_controller_t::get_stats(session_t *session,
                                             uint32_t &current_bitrate_kbps,
                                             uint64_t &last_adjustment_time_ms,
                                             uint32_t &adjustment_count,
                                             float &loss_percentage) const {
    if (!session || !session->auto_bitrate_enabled) {
      return false;
    }

    auto it = session_states.find(session);
    if (it == session_states.end()) {
      return false;
    }

    const auto &state = it->second;
    current_bitrate_kbps = state.current_bitrate_kbps;
    adjustment_count = state.adjustment_count;
    loss_percentage = static_cast<float>(state.loss_percentage);

    // Calculate milliseconds since session start
    auto now = std::chrono::steady_clock::now();
    auto adjustment_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        state.last_successful_adjustment_time - state.session_start_time).count();
    
    if (state.adjustment_count > 0 && adjustment_duration >= 0) {
      last_adjustment_time_ms = static_cast<uint64_t>(adjustment_duration);
    } else {
      last_adjustment_time_ms = 0;  // Never adjusted
    }

    return true;
  }

}  // namespace stream
