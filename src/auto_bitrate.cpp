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
    
    // Only adjust if change is significant (at least 5%)
    if (std::abs(adjustment_factor - 1.0) < 0.05) {
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
    int min_bitrate = settings.min_kbps;
    if (min_bitrate <= 0) {
      min_bitrate = 500;  // Default minimum
    }

    int max_bitrate = settings.max_kbps;
    if (max_bitrate <= 0) {
      // Use client's requested max or config max_bitrate
      max_bitrate = session->config.monitor.bitrate;
      if (settings.max_bitrate_cap > 0 && settings.max_bitrate_cap < max_bitrate) {
        max_bitrate = settings.max_bitrate_cap;
      }
    } else {
      // Use minimum of auto_bitrate_max_kbps and config max_bitrate
      if (settings.max_bitrate_cap > 0 && settings.max_bitrate_cap < max_bitrate) {
        max_bitrate = settings.max_bitrate_cap;
      }
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

  double auto_bitrate_controller_t::compute_loss_percentage(session_t *session, 
                                                             uint64_t lastGoodFrame,
                                                             std::chrono::milliseconds time_interval) const {
    auto it = session_states.find(session);
    if (it == session_states.end()) {
      return 0.0;
    }

    const auto &state = it->second;
    
    if (state.last_reported_good_frame == 0) {
      // First report, no loss yet
      return 0.0;
    }

    // Calculate expected frame progression
    float framerate = static_cast<float>(session->config.monitor.framerate);
    if (framerate > 1000) {
      framerate = framerate / 1000.0f;  // Convert from millifps to fps
    }

    float expected_frames = framerate * (time_interval.count() / 1000.0f);
    uint64_t expected_current_frame = state.last_reported_good_frame + static_cast<uint64_t>(expected_frames);

    // Calculate loss
    int64_t loss_count = 0;
    if (lastGoodFrame < expected_current_frame) {
      loss_count = static_cast<int64_t>(expected_current_frame - lastGoodFrame);
    }

    if (expected_frames <= 0) {
      return 0.0;
    }

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
