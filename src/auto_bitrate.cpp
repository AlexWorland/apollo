/**
 * @file src/auto_bitrate.cpp
 * @brief Implementation of auto bitrate adjustment controller.
 */
#include "auto_bitrate.h"

#include <iomanip>

#include "logging.h"

namespace auto_bitrate {

  AutoBitrateController::AutoBitrateController(
      int initialBitrate,
      int minBitrate,
      int maxBitrate,
      float poorNetworkThreshold,
      float goodNetworkThreshold,
      float increaseFactor,
      float decreaseFactor,
      int stabilityWindowMs,
      int minConsecutiveGoodIntervals)
      : currentBitrateKbps(initialBitrate),
        baseBitrateKbps(initialBitrate),
        minBitrateKbps(minBitrate),
        maxBitrateKbps(maxBitrate),
        increaseFactor(increaseFactor),
        decreaseFactor(decreaseFactor),
        stabilityWindowMs(stabilityWindowMs),
        poorNetworkThreshold(poorNetworkThreshold),
        goodNetworkThreshold(goodNetworkThreshold),
        minConsecutiveGoodIntervals(minConsecutiveGoodIntervals),
        lastCheckTime(std::chrono::steady_clock::now()),
        lastStatusLog(std::chrono::steady_clock::now()) {
    metrics.lastAdjustment = std::chrono::steady_clock::now();
    metrics.lastPoorCondition = std::chrono::steady_clock::now();
  }

  void AutoBitrateController::updateNetworkMetrics(float frameLossPercent, int timeSinceLastReportMs) {
    // Clamp frame loss percentage to non-negative to handle data corruption or counter issues
    // Negative values would incorrectly trigger good network conditions
    float oldFrameLoss = metrics.frameLossPercent;
    frameLossPercent = std::max(0.0f, frameLossPercent);
    metrics.frameLossPercent = frameLossPercent;

    auto now = std::chrono::steady_clock::now();
    int oldGoodIntervals = metrics.consecutiveGoodIntervals;
    int oldPoorIntervals = metrics.consecutivePoorIntervals;

    // Update consecutive interval counters
    if (frameLossPercent > poorNetworkThreshold) {
      metrics.consecutivePoorIntervals++;
      metrics.consecutiveGoodIntervals = 0;
      metrics.lastPoorCondition = now;
    } else if (frameLossPercent < goodNetworkThreshold) {
      metrics.consecutiveGoodIntervals++;
      metrics.consecutivePoorIntervals = 0;
    } else {
      // Stable zone: reset counters but don't change bitrate
      metrics.consecutiveGoodIntervals = 0;
      metrics.consecutivePoorIntervals = 0;
    }
    
    // Log when counters change significantly
    if (oldGoodIntervals != metrics.consecutiveGoodIntervals || oldPoorIntervals != metrics.consecutivePoorIntervals) {
      BOOST_LOG(debug) << "AutoBitrate: [Metrics] Interval counters updated - good: " << oldGoodIntervals 
                       << " -> " << metrics.consecutiveGoodIntervals 
                       << ", poor: " << oldPoorIntervals << " -> " << metrics.consecutivePoorIntervals
                       << ", frame_loss: " << std::fixed << std::setprecision(2) << frameLossPercent << "%";
    }
  }

  std::optional<int> AutoBitrateController::getAdjustedBitrate() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCheckTime).count();

    // Only check for adjustments at regular intervals
    if (timeSinceLastCheck < ADJUSTMENT_INTERVAL_MS) {
      BOOST_LOG(debug) << "AutoBitrate: [Decision] Skipping check - only " << timeSinceLastCheck 
                       << " ms since last check (need " << ADJUSTMENT_INTERVAL_MS << " ms)";
      return std::nullopt;
    }

    BOOST_LOG(debug) << "AutoBitrate: [Decision] Evaluating bitrate adjustment - current=" << currentBitrateKbps 
                     << " kbps, frame_loss=" << std::fixed << std::setprecision(2) << metrics.frameLossPercent 
                     << "%, good_intervals=" << metrics.consecutiveGoodIntervals 
                     << ", poor_intervals=" << metrics.consecutivePoorIntervals;
    lastCheckTime = now;

    // Check for poor network conditions - immediate decrease
    if (metrics.frameLossPercent > poorNetworkThreshold) {
      BOOST_LOG(debug) << "AutoBitrate: [Decision] Poor network detected - loss=" << std::fixed << std::setprecision(2) 
                       << metrics.frameLossPercent << "% (threshold=" << poorNetworkThreshold << "%)";
      // Check if we've already adjusted recently (avoid rapid oscillations)
      auto timeSinceLastAdjustment = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastAdjustment).count();
      if (timeSinceLastAdjustment < ADJUSTMENT_INTERVAL_MS) {
        BOOST_LOG(debug) << "AutoBitrate: [Decision] Skipping poor network adjustment - only " << timeSinceLastAdjustment 
                         << " ms since last adjustment (need " << ADJUSTMENT_INTERVAL_MS << " ms)";
        return std::nullopt;
      }

      int calculatedBitrate = static_cast<int>(currentBitrateKbps * decreaseFactor);
      int newBitrate = std::max(calculatedBitrate, minBitrateKbps);
      BOOST_LOG(debug) << "AutoBitrate: [Decision] Poor network calculation - current=" << currentBitrateKbps 
                       << " kbps, factor=" << decreaseFactor << ", calculated=" << calculatedBitrate 
                       << " kbps, clamped=" << newBitrate << " kbps (min=" << minBitrateKbps << " kbps)";

      if (newBitrate != currentBitrateKbps) {
        if (newBitrate == minBitrateKbps && calculatedBitrate < minBitrateKbps) {
          BOOST_LOG(info) << "AutoBitrate: [Decision] Poor network detected (" << std::fixed << std::setprecision(2) << metrics.frameLossPercent
                          << "% loss), decreasing bitrate from " << currentBitrateKbps << " to " << newBitrate
                          << " kbps (clamped to minimum " << minBitrateKbps << " kbps)";
        } else {
          BOOST_LOG(info) << "AutoBitrate: [Decision] Poor network detected (" << std::fixed << std::setprecision(2) << metrics.frameLossPercent
                          << "% loss), decreasing bitrate from " << currentBitrateKbps << " to " << newBitrate << " kbps";
        }
        currentBitrateKbps = newBitrate;
        metrics.lastAdjustment = now;
        metrics.consecutiveGoodIntervals = 0;
        metrics.consecutivePoorIntervals = 0;
        return newBitrate;
      } else {
        BOOST_LOG(debug) << "AutoBitrate: [Decision] Poor network detected but bitrate unchanged (already at " << currentBitrateKbps << " kbps)";
      }
    }
    // Check for good network conditions - increase after stability period
    else if (metrics.frameLossPercent < goodNetworkThreshold) {
      BOOST_LOG(debug) << "AutoBitrate: [Decision] Good network detected - loss=" << std::fixed << std::setprecision(2) 
                       << metrics.frameLossPercent << "% (threshold=" << goodNetworkThreshold << "%)";
      // Require consecutive good intervals and stability window
      if (metrics.consecutiveGoodIntervals >= minConsecutiveGoodIntervals) {
        auto timeSinceLastPoor = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastPoorCondition).count();
        BOOST_LOG(debug) << "AutoBitrate: [Decision] Good intervals check - consecutive=" << metrics.consecutiveGoodIntervals 
                         << " (need " << minConsecutiveGoodIntervals << "), time_since_poor=" << timeSinceLastPoor 
                         << " ms (need " << stabilityWindowMs << " ms)";
        if (timeSinceLastPoor >= stabilityWindowMs) {
          // Check if we've already adjusted recently
          auto timeSinceLastAdjustment = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastAdjustment).count();
          if (timeSinceLastAdjustment < ADJUSTMENT_INTERVAL_MS) {
            BOOST_LOG(debug) << "AutoBitrate: [Decision] Skipping good network adjustment - only " << timeSinceLastAdjustment 
                             << " ms since last adjustment (need " << ADJUSTMENT_INTERVAL_MS << " ms)";
            return std::nullopt;
          }

          int calculatedBitrate = static_cast<int>(currentBitrateKbps * increaseFactor);
          int newBitrate = std::min(calculatedBitrate, maxBitrateKbps);
          BOOST_LOG(debug) << "AutoBitrate: [Decision] Good network calculation - current=" << currentBitrateKbps 
                           << " kbps, factor=" << increaseFactor << ", calculated=" << calculatedBitrate 
                           << " kbps, clamped=" << newBitrate << " kbps (max=" << maxBitrateKbps << " kbps)";

          if (newBitrate != currentBitrateKbps) {
            if (newBitrate == maxBitrateKbps && calculatedBitrate > maxBitrateKbps) {
              BOOST_LOG(info) << "AutoBitrate: [Decision] Good network detected (" << std::fixed << std::setprecision(2) << metrics.frameLossPercent
                              << "% loss), increasing bitrate from " << currentBitrateKbps << " to " << newBitrate
                              << " kbps (clamped to maximum " << maxBitrateKbps << " kbps)";
            } else {
              BOOST_LOG(info) << "AutoBitrate: [Decision] Good network detected (" << std::fixed << std::setprecision(2) << metrics.frameLossPercent
                              << "% loss), increasing bitrate from " << currentBitrateKbps << " to " << newBitrate << " kbps";
            }
            currentBitrateKbps = newBitrate;
            metrics.lastAdjustment = now;
            metrics.consecutiveGoodIntervals = 0;
            metrics.consecutivePoorIntervals = 0;  // Reset poor intervals counter for consistency
            return newBitrate;
          } else {
            BOOST_LOG(debug) << "AutoBitrate: [Decision] Good network detected but bitrate unchanged (already at " << currentBitrateKbps << " kbps)";
          }
        } else {
          BOOST_LOG(debug) << "AutoBitrate: [Decision] Stability window not met - " << timeSinceLastPoor << " ms < " << stabilityWindowMs << " ms";
        }
      } else {
        BOOST_LOG(debug) << "AutoBitrate: [Decision] Consecutive good intervals not met - " << metrics.consecutiveGoodIntervals << " < " << minConsecutiveGoodIntervals;
      }
    }
    // Stable zone (1-5% loss): maintain current bitrate
    // No adjustment needed
    BOOST_LOG(debug) << "AutoBitrate: [Decision] Stable zone - frame_loss=" << std::fixed << std::setprecision(2) 
                     << metrics.frameLossPercent << "% (between " << goodNetworkThreshold << "% and " 
                     << poorNetworkThreshold << "%), maintaining current bitrate: " << currentBitrateKbps << " kbps";

    return std::nullopt;
  }

  void AutoBitrateController::reset(int newBaseBitrate) {
    int oldBitrate = currentBitrateKbps;
    baseBitrateKbps = newBaseBitrate;
    currentBitrateKbps = newBaseBitrate;
    metrics.consecutiveGoodIntervals = 0;
    metrics.consecutivePoorIntervals = 0;
    metrics.frameLossPercent = 0.0f;
    metrics.lastAdjustment = std::chrono::steady_clock::now();
    metrics.lastPoorCondition = std::chrono::steady_clock::now();
    lastCheckTime = std::chrono::steady_clock::now();
    BOOST_LOG(info) << "AutoBitrate: Controller reset from " << oldBitrate << " to " << newBaseBitrate << " kbps";
  }

  void AutoBitrateController::logStatusIfNeeded(int statusLogIntervalMs) {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatusLog).count();

    if (timeSinceLastLog >= statusLogIntervalMs) {
      std::string networkState;
      if (metrics.frameLossPercent > poorNetworkThreshold) {
        networkState = "poor";
      } else if (metrics.frameLossPercent < goodNetworkThreshold) {
        networkState = "good";
      } else {
        networkState = "stable";
      }

      BOOST_LOG(info) << "AutoBitrate: Status - bitrate=" << currentBitrateKbps
                      << " kbps, frame_loss=" << std::fixed << std::setprecision(2) << metrics.frameLossPercent
                      << "%, state=" << networkState
                      << ", good_intervals=" << metrics.consecutiveGoodIntervals
                      << ", poor_intervals=" << metrics.consecutivePoorIntervals;
      
      lastStatusLog = now;
    }
  }

}  // namespace auto_bitrate


