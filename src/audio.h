/**
 * @file src/audio.h
 * @brief Declarations for audio capture and encoding.
 */
#pragma once

// local includes
#include "platform/common.h"
#include "thread_safe.h"
#include "utility.h"

#include <bitset>

namespace audio {
  enum stream_config_e : int {
    STEREO,  ///< Stereo
    HIGH_STEREO,  ///< High stereo
    SURROUND51,  ///< Surround 5.1
    HIGH_SURROUND51,  ///< High surround 5.1
    SURROUND71,  ///< Surround 7.1
    HIGH_SURROUND71,  ///< High surround 7.1
    MAX_STREAM_CONFIG  ///< Maximum audio stream configuration
  };

  /**
   * @brief Opus stream configuration structure.
   */
  struct opus_stream_config_t {
    std::int32_t sampleRate;  ///< Sample rate in Hz
    int channelCount;  ///< Number of audio channels
    int streams;  ///< Number of Opus streams
    int coupledStreams;  ///< Number of coupled streams
    const std::uint8_t *mapping;  ///< Channel mapping array
    int bitrate;  ///< Bitrate in bits per second
  };

  /**
   * @brief Stream parameters structure.
   */
  struct stream_params_t {
    int channelCount;  ///< Number of audio channels
    int streams;  ///< Number of Opus streams
    int coupledStreams;  ///< Number of coupled streams
    std::uint8_t mapping[8];  ///< Channel mapping array
  };

  extern opus_stream_config_t stream_configs[MAX_STREAM_CONFIG];  ///< Predefined stream configurations

  /**
   * @brief Audio configuration structure.
   */
  struct config_t {
    /**
     * @brief Audio configuration flags.
     */
    enum flags_e : int {
      HIGH_QUALITY,  ///< High quality audio
      HOST_AUDIO,  ///< Host audio
      CUSTOM_SURROUND_PARAMS,  ///< Custom surround parameters
      MAX_FLAGS  ///< Maximum number of flags
    };

    int packetDuration;  ///< Packet duration in milliseconds
    int channels;  ///< Number of audio channels
    int mask;  ///< Channel mask
    stream_params_t customStreamParams;  ///< Custom stream parameters
    std::bitset<MAX_FLAGS> flags;  ///< Configuration flags
    uint64_t __padding;  ///< Padding to prevent input_only from being overwritten
    bool input_only;  ///< Whether this is input-only mode
  };

  /**
   * @brief Audio context structure.
   */
  struct audio_ctx_t {
    std::unique_ptr<std::atomic_bool> sink_flag;  ///< Flag to change sink for first stream only
    std::unique_ptr<platf::audio_control_t> control;  ///< Platform audio control
    bool restore_sink;  ///< Whether to restore original sink
    platf::sink_t sink;  ///< Audio sink
  };

  using buffer_t = util::buffer_t<std::uint8_t>;
  using packet_t = std::pair<void *, buffer_t>;
  using audio_ctx_ref_t = safe::shared_t<audio_ctx_t>::ptr_t;

  void capture(safe::mail_t mail, config_t config, void *channel_data);

  /**
   * @brief Get the reference to the audio context.
   * @returns A shared pointer reference to audio context.
   * @note Aside from the configuration purposes, it can be used to extend the
   *       audio sink lifetime to capture sink earlier and restore it later.
   *
   * @examples
   * audio_ctx_ref_t audio = get_audio_ctx_ref()
   * @examples_end
   */
  audio_ctx_ref_t get_audio_ctx_ref();

  /**
   * @brief Check if the audio sink held by audio context is available.
   * @returns True if available (and can probably be restored), false otherwise.
   * @note Useful for delaying the release of audio context shared pointer (which
   *       tries to restore original sink).
   *
   * @examples
   * audio_ctx_ref_t audio = get_audio_ctx_ref()
   * if (audio.get()) {
   *     return is_audio_ctx_sink_available(*audio.get());
   * }
   * return false;
   * @examples_end
   */
  bool is_audio_ctx_sink_available(const audio_ctx_t &ctx);
}  // namespace audio
