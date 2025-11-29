/**
 * @file src/video.h
 * @brief Declarations for video.
 */
#pragma once

// local includes
#include "input.h"
#include "platform/common.h"
#include "thread_safe.h"
#include "video_colorspace.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

struct AVPacket;

namespace video {

  /**
   * @brief Encoding configuration requested by remote client.
   * @warning DO NOT CHANGE ORDER OR ADD FIELDS IN THE MIDDLE!
   *          ONLY APPEND NEW FIELD AFTERWARDS!
   */
  struct config_t {
    int width;  ///< Video width in pixels
    int height;  ///< Video height in pixels
    int framerate;  ///< Requested framerate, used in individual frame bitrate budget calculation
    int bitrate;  ///< Video bitrate in kilobits (1000 bits) for requested framerate
    int slicesPerFrame;  ///< Number of slices per frame
    int numRefFrames;  ///< Max number of reference frames
    int encoderCscMode;  ///< Color range and SDR encoding colorspace. Color range (encoderCscMode & 0x1): 0=limited, 1=full. SDR colorspace (encoderCscMode >> 1): 0=BT.601, 1=BT.709, 2=BT.2020. HDR encoding colorspace is always BT.2020+ST2084.
    int videoFormat;  ///< Video format: 0=H.264, 1=HEVC, 2=AV1
    int dynamicRange;  ///< Encoding color depth (bit depth): 0=8-bit, 1=10-bit. HDR activates when >8-bit and display is in HDR mode.
    int chromaSamplingType;  ///< Chroma sampling: 0=4:2:0, 1=4:4:4
    int enableIntraRefresh;  ///< Intra refresh: 0=disabled, 1=enabled
    int encodingFramerate;  ///< Requested display framerate
    bool input_only;  ///< Whether this is an input-only session
  };

  /**
   * @brief Map AVCodec hardware device type to platform memory type.
   * @param type AVCodec hardware device type.
   * @return Platform memory type.
   */
  platf::mem_type_e map_base_dev_type(AVHWDeviceType type);

  /**
   * @brief Map AVCodec pixel format to platform pixel format.
   * @param fmt AVCodec pixel format.
   * @return Platform pixel format.
   */
  platf::pix_fmt_e map_pix_fmt(AVPixelFormat fmt);

  /**
   * @brief Free AVCodec context.
   * @param ctx The codec context to free.
   */
  void free_ctx(AVCodecContext *ctx);

  /**
   * @brief Free AVFrame.
   * @param frame The frame to free.
   */
  void free_frame(AVFrame *frame);

  /**
   * @brief Free AVBufferRef.
   * @param ref The buffer reference to free.
   */
  void free_buffer(AVBufferRef *ref);

  using avcodec_ctx_t = util::safe_ptr<AVCodecContext, free_ctx>;  ///< Safe pointer to AVCodec context
  using avcodec_frame_t = util::safe_ptr<AVFrame, free_frame>;  ///< Safe pointer to AVFrame
  using avcodec_buffer_t = util::safe_ptr<AVBufferRef, free_buffer>;  ///< Safe pointer to AVBufferRef
  using sws_t = util::safe_ptr<SwsContext, sws_freeContext>;  ///< Safe pointer to SwsContext
  using img_event_t = std::shared_ptr<safe::event_t<std::shared_ptr<platf::img_t>>>;  ///< Image event type

  /**
   * @brief Base structure for encoder platform formats.
   */
  struct encoder_platform_formats_t {
    virtual ~encoder_platform_formats_t() = default;
    platf::mem_type_e dev_type;  ///< Device memory type
    platf::pix_fmt_e pix_fmt_8bit;  ///< 8-bit pixel format
    platf::pix_fmt_e pix_fmt_10bit;  ///< 10-bit pixel format
    platf::pix_fmt_e pix_fmt_yuv444_8bit;  ///< YUV 4:4:4 8-bit pixel format
    platf::pix_fmt_e pix_fmt_yuv444_10bit;  ///< YUV 4:4:4 10-bit pixel format
  };

  /**
   * @brief AVCodec-based encoder platform formats structure.
   */
  struct encoder_platform_formats_avcodec: encoder_platform_formats_t {
    using init_buffer_function_t = std::function<util::Either<avcodec_buffer_t, int>(platf::avcodec_encode_device_t *)>;  ///< Buffer initialization function type

    /**
     * @brief Construct AVCodec encoder platform formats.
     * @param avcodec_base_dev_type Base AVCodec hardware device type.
     * @param avcodec_derived_dev_type Derived AVCodec hardware device type.
     * @param avcodec_dev_pix_fmt Device pixel format.
     * @param avcodec_pix_fmt_8bit 8-bit pixel format.
     * @param avcodec_pix_fmt_10bit 10-bit pixel format.
     * @param avcodec_pix_fmt_yuv444_8bit YUV 4:4:4 8-bit pixel format.
     * @param avcodec_pix_fmt_yuv444_10bit YUV 4:4:4 10-bit pixel format.
     * @param init_avcodec_hardware_input_buffer_function Function to initialize hardware input buffer.
     */
    encoder_platform_formats_avcodec(
      const AVHWDeviceType &avcodec_base_dev_type,
      const AVHWDeviceType &avcodec_derived_dev_type,
      const AVPixelFormat &avcodec_dev_pix_fmt,
      const AVPixelFormat &avcodec_pix_fmt_8bit,
      const AVPixelFormat &avcodec_pix_fmt_10bit,
      const AVPixelFormat &avcodec_pix_fmt_yuv444_8bit,
      const AVPixelFormat &avcodec_pix_fmt_yuv444_10bit,
      const init_buffer_function_t &init_avcodec_hardware_input_buffer_function
    ):
        avcodec_base_dev_type {avcodec_base_dev_type},
        avcodec_derived_dev_type {avcodec_derived_dev_type},
        avcodec_dev_pix_fmt {avcodec_dev_pix_fmt},
        avcodec_pix_fmt_8bit {avcodec_pix_fmt_8bit},
        avcodec_pix_fmt_10bit {avcodec_pix_fmt_10bit},
        avcodec_pix_fmt_yuv444_8bit {avcodec_pix_fmt_yuv444_8bit},
        avcodec_pix_fmt_yuv444_10bit {avcodec_pix_fmt_yuv444_10bit},
        init_avcodec_hardware_input_buffer {init_avcodec_hardware_input_buffer_function} {
      dev_type = map_base_dev_type(avcodec_base_dev_type);
      pix_fmt_8bit = map_pix_fmt(avcodec_pix_fmt_8bit);
      pix_fmt_10bit = map_pix_fmt(avcodec_pix_fmt_10bit);
      pix_fmt_yuv444_8bit = map_pix_fmt(avcodec_pix_fmt_yuv444_8bit);
      pix_fmt_yuv444_10bit = map_pix_fmt(avcodec_pix_fmt_yuv444_10bit);
    }

    AVHWDeviceType avcodec_base_dev_type;  ///< Base AVCodec hardware device type
    AVHWDeviceType avcodec_derived_dev_type;  ///< Derived AVCodec hardware device type
    AVPixelFormat avcodec_dev_pix_fmt;  ///< Device pixel format
    AVPixelFormat avcodec_pix_fmt_8bit;  ///< 8-bit pixel format
    AVPixelFormat avcodec_pix_fmt_10bit;  ///< 10-bit pixel format
    AVPixelFormat avcodec_pix_fmt_yuv444_8bit;  ///< YUV 4:4:4 8-bit pixel format
    AVPixelFormat avcodec_pix_fmt_yuv444_10bit;  ///< YUV 4:4:4 10-bit pixel format
    init_buffer_function_t init_avcodec_hardware_input_buffer;  ///< Hardware input buffer initialization function
  };

  /**
   * @brief NVENC encoder platform formats structure.
   */
  struct encoder_platform_formats_nvenc: encoder_platform_formats_t {
    /**
     * @brief Construct NVENC encoder platform formats.
     * @param dev_type Device memory type.
     * @param pix_fmt_8bit 8-bit pixel format.
     * @param pix_fmt_10bit 10-bit pixel format.
     * @param pix_fmt_yuv444_8bit YUV 4:4:4 8-bit pixel format.
     * @param pix_fmt_yuv444_10bit YUV 4:4:4 10-bit pixel format.
     */
    encoder_platform_formats_nvenc(
      const platf::mem_type_e &dev_type,
      const platf::pix_fmt_e &pix_fmt_8bit,
      const platf::pix_fmt_e &pix_fmt_10bit,
      const platf::pix_fmt_e &pix_fmt_yuv444_8bit,
      const platf::pix_fmt_e &pix_fmt_yuv444_10bit
    ) {
      encoder_platform_formats_t::dev_type = dev_type;
      encoder_platform_formats_t::pix_fmt_8bit = pix_fmt_8bit;
      encoder_platform_formats_t::pix_fmt_10bit = pix_fmt_10bit;
      encoder_platform_formats_t::pix_fmt_yuv444_8bit = pix_fmt_yuv444_8bit;
      encoder_platform_formats_t::pix_fmt_yuv444_10bit = pix_fmt_yuv444_10bit;
    }
  };

  /**
   * @brief Encoder structure containing codec configurations and capabilities.
   */
  struct encoder_t {
    std::string_view name;  ///< Encoder name

    /**
     * @brief Encoder capability flags.
     */
    enum flag_e {
      PASSED,  ///< Indicates the encoder is supported.
      REF_FRAMES_RESTRICT,  ///< Set maximum reference frames.
      DYNAMIC_RANGE,  ///< HDR support.
      YUV444,  ///< YUV 4:4:4 support.
      VUI_PARAMETERS,  ///< AMD encoder with VAAPI doesn't add VUI parameters to SPS.
      MAX_FLAGS  ///< Maximum number of flags.
    };

    /**
     * @brief Convert flag enum to string view.
     * @param flag The flag to convert.
     * @return String representation of the flag.
     */
    static std::string_view from_flag(flag_e flag) {
#define _CONVERT(x) \
  case flag_e::x: \
    return std::string_view(#x)
      switch (flag) {
        _CONVERT(PASSED);
        _CONVERT(REF_FRAMES_RESTRICT);
        _CONVERT(DYNAMIC_RANGE);
        _CONVERT(YUV444);
        _CONVERT(VUI_PARAMETERS);
        _CONVERT(MAX_FLAGS);
      }
#undef _CONVERT

      return {"unknown"};
    }

    /**
     * @brief Encoder option structure.
     */
    struct option_t {
      KITTY_DEFAULT_CONSTR_MOVE(option_t)
      option_t(const option_t &) = default;

      std::string name;  ///< Option name
      std::variant<int, int *, std::optional<int> *, std::function<int()>, std::string, std::string *, std::function<const std::string(const config_t &)>> value;  ///< Option value (can be various types)

      /**
       * @brief Construct encoder option.
       * @param name Option name.
       * @param value Option value.
       */
      option_t(std::string &&name, decltype(value) &&value):
          name {std::move(name)},
          value {std::move(value)} {
      }
    };

    const std::unique_ptr<const encoder_platform_formats_t> platform_formats;  ///< Platform-specific formats

    /**
     * @brief Codec configuration structure.
     */
    struct codec_t {
      std::vector<option_t> common_options;  ///< Common encoder options
      std::vector<option_t> sdr_options;  ///< SDR-specific options
      std::vector<option_t> hdr_options;  ///< HDR-specific options
      std::vector<option_t> sdr444_options;  ///< SDR 4:4:4-specific options
      std::vector<option_t> hdr444_options;  ///< HDR 4:4:4-specific options
      std::vector<option_t> fallback_options;  ///< Fallback options
      std::string name;  ///< Codec name
      std::bitset<MAX_FLAGS> capabilities;  ///< Codec capability flags

      /**
       * @brief Check if a capability flag is set (const).
       * @param flag The flag to check.
       * @return `true` if flag is set, `false` otherwise.
       */
      bool operator[](flag_e flag) const {
        return capabilities[(std::size_t) flag];
      }

      /**
       * @brief Get reference to a capability flag.
       * @param flag The flag to access.
       * @return Reference to the flag bit.
       */
      std::bitset<MAX_FLAGS>::reference operator[](flag_e flag) {
        return capabilities[(std::size_t) flag];
      }
    } av1;  ///< AV1 codec configuration
    codec_t hevc;  ///< HEVC codec configuration
    codec_t h264;  ///< H.264 codec configuration

    const codec_t &codec_from_config(const config_t &config) const {
      switch (config.videoFormat) {
        default:
          BOOST_LOG(error) << "Unknown video format " << config.videoFormat << ", falling back to H.264";
          // fallthrough
        case 0:
          return h264;
        case 1:
          return hevc;
        case 2:
          return av1;
      }
    }

    uint32_t flags;
  };

  /**
   * @brief Base class for video encoding sessions.
   * 
   * Provides the interface for video encoding operations including frame conversion,
   * IDR frame requests, and reference frame invalidation.
   */
  struct encode_session_t {
    virtual ~encode_session_t() = default;

    virtual int convert(platf::img_t &img) = 0;

    virtual void request_idr_frame() = 0;

    virtual void request_normal_frame() = 0;

    virtual void invalidate_ref_frames(int64_t first_frame, int64_t last_frame) = 0;

    // Reconfigure bitrate during active session
    virtual bool reconfigure_bitrate(int new_bitrate_kbps) {
      // Default implementation: not supported
      return false;
    }
  };

  // encoders
  extern encoder_t software;

#if !defined(__APPLE__)
  extern encoder_t nvenc;  // available for windows and linux
#endif

#ifdef _WIN32
  extern encoder_t amdvce;
  extern encoder_t quicksync;
#endif

#ifdef __linux__
  extern encoder_t vaapi;
#endif

#ifdef __APPLE__
  extern encoder_t videotoolbox;
#endif

  /**
   * @brief Base class for raw video packet data.
   * 
   * Provides the interface for accessing encoded video packet data,
   * including frame type detection, frame indexing, and data access.
   */
  struct packet_raw_t {
    virtual ~packet_raw_t() = default;

    virtual bool is_idr() = 0;

    virtual int64_t frame_index() = 0;

    virtual uint8_t *data() = 0;

    virtual size_t data_size() = 0;

    /**
     * @brief Replacement operation for packet data.
     * 
     * Defines a single byte sequence replacement operation.
     * Used to replace specific byte sequences in the packet data
     * (e.g., for SPS/PPS injection or header modifications).
     */
    struct replace_t {
      std::string_view old;
      std::string_view _new;

      KITTY_DEFAULT_CONSTR_MOVE(replace_t)

      replace_t(std::string_view old, std::string_view _new) noexcept:
          old {std::move(old)},
          _new {std::move(_new)} {
      }
    };

    void *channel_data = nullptr;
    bool after_ref_frame_invalidation = false;
    std::optional<std::chrono::steady_clock::time_point> frame_timestamp;
  };

  /**
   * @brief Video packet implementation using libavcodec.
   * 
   * Wraps AVPacket from FFmpeg/libavcodec for video packet handling.
   */
  struct packet_raw_avcodec: packet_raw_t {
    packet_raw_avcodec() {
      av_packet = av_packet_alloc();
    }

    ~packet_raw_avcodec() {
      av_packet_free(&this->av_packet);
    }

    bool is_idr() override {
      return av_packet->flags & AV_PKT_FLAG_KEY;
    }

    int64_t frame_index() override {
      return av_packet->pts;
    }

    uint8_t *data() override {
      return av_packet->data;
    }

    size_t data_size() override {
      return av_packet->size;
    }

    AVPacket *av_packet;
  };

  /**
   * @brief Generic video packet implementation.
   * 
   * Stores video packet data in a vector with frame index and IDR flag.
   * Used for encoders that don't use libavcodec.
   */
  struct packet_raw_generic: packet_raw_t {
    packet_raw_generic(std::vector<uint8_t> &&frame_data, int64_t frame_index, bool idr):
        frame_data {std::move(frame_data)},
        index {frame_index},
        idr {idr} {
    }

    bool is_idr() override {
      return idr;
    }

    int64_t frame_index() override {
      return index;
    }

    uint8_t *data() override {
      return frame_data.data();
    }

    size_t data_size() override {
      return frame_data.size();
    }

    std::vector<uint8_t> frame_data;
    int64_t index;
    bool idr;
  };

  using packet_t = std::unique_ptr<packet_raw_t>;

  /**
   * @brief HDR information structure.
   * 
   * Contains HDR metadata and enabled state for High Dynamic Range video.
   */
  struct hdr_info_raw_t {
    explicit hdr_info_raw_t(bool enabled):
        enabled {enabled},
        metadata {} {};
    explicit hdr_info_raw_t(bool enabled, const SS_HDR_METADATA &metadata):
        enabled {enabled},
        metadata {metadata} {};

    bool enabled;
    SS_HDR_METADATA metadata;
  };

  using hdr_info_t = std::unique_ptr<hdr_info_raw_t>;

  extern int active_hevc_mode;
  extern int active_av1_mode;
  extern bool last_encoder_probe_supported_ref_frames_invalidation;
  extern std::array<bool, 3> last_encoder_probe_supported_yuv444_for_codec;  // 0 - H.264, 1 - HEVC, 2 - AV1

  void capture(
    safe::mail_t mail,
    config_t config,
    void *channel_data
  );

  bool validate_encoder(encoder_t &encoder, bool expect_failure);

  /**
   * @brief Check if we can allow probing for the encoders.
   * @return True if there should be no issues with the probing, false if we should prevent it.
   */
  bool allow_encoder_probing();

  /**
   * @brief Probe encoders and select the preferred encoder.
   * This is called once at startup and each time a stream is launched to
   * ensure the best encoder is selected. Encoder availability can change
   * at runtime due to all sorts of things from driver updates to eGPUs.
   *
   * @warning This is only safe to call when there is no client actively streaming.
   */
  int probe_encoders();
}  // namespace video
