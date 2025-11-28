/**
 * @file src/cbs.h
 * @brief Declarations for FFmpeg Coded Bitstream API.
 */
#pragma once

// local includes
#include "utility.h"

struct AVPacket;
struct AVCodecContext;

namespace cbs {

  /**
   * @brief Network Abstraction Layer (NAL) unit structure.
   */
  struct nal_t {
    util::buffer_t<std::uint8_t> _new;  ///< New NAL unit buffer
    util::buffer_t<std::uint8_t> old;  ///< Old NAL unit buffer
  };

  /**
   * @brief HEVC (H.265) parameter set structure.
   */
  struct hevc_t {
    nal_t vps;  ///< Video Parameter Set
    nal_t sps;  ///< Sequence Parameter Set
  };

  /**
   * @brief H.264 parameter set structure.
   */
  struct h264_t {
    nal_t sps;  ///< Sequence Parameter Set
  };

  /**
   * @brief Create HEVC Sequence Parameter Set from codec context and packet.
   * @param ctx The AVCodecContext containing codec information.
   * @param packet The AVPacket containing encoded data.
   * @return HEVC parameter set structure.
   */
  hevc_t make_sps_hevc(const AVCodecContext *ctx, const AVPacket *packet);

  /**
   * @brief Create H.264 Sequence Parameter Set from codec context and packet.
   * @param ctx The AVCodecContext containing codec information.
   * @param packet The AVPacket containing encoded data.
   * @return H.264 parameter set structure.
   */
  h264_t make_sps_h264(const AVCodecContext *ctx, const AVPacket *packet);

  /**
   * @brief Validates the Sequence Parameter Set (SPS) of a given packet.
   * @param packet The packet to validate.
   * @param codec_id The ID of the codec used (either AV_CODEC_ID_H264 or AV_CODEC_ID_H265).
   * @return True if the SPS->VUI is present in the active SPS of the packet, false otherwise.
   */
  bool validate_sps(const AVPacket *packet, int codec_id);
}  // namespace cbs
