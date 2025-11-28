/**
 * @file src/video_colorspace.h
 * @brief Declarations for colorspace functions.
 */
#pragma once

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace video {

  /**
   * @brief Colorspace enumeration.
   */
  enum class colorspace_e {
    rec601,  ///< Rec. 601 (SDTV)
    rec709,  ///< Rec. 709 (HDTV)
    bt2020sdr,  ///< Rec. 2020 SDR
    bt2020,  ///< Rec. 2020 HDR
  };

  /**
   * @brief Sunshine colorspace configuration structure.
   */
  struct sunshine_colorspace_t {
    colorspace_e colorspace;  ///< Colorspace standard
    bool full_range;  ///< Whether to use full color range (true) or limited range (false)
    unsigned bit_depth;  ///< Bit depth (8, 10, etc.)
  };

  /**
   * @brief Check if colorspace is HDR.
   * @param colorspace The colorspace to check.
   * @return `true` if HDR, `false` if SDR.
   */
  bool colorspace_is_hdr(const sunshine_colorspace_t &colorspace);

  // Declared in video.h
  struct config_t;

  /**
   * @brief Get colorspace from client configuration.
   * @param config The client video configuration.
   * @param hdr_display Whether the display supports HDR.
   * @return Sunshine colorspace configuration.
   */
  sunshine_colorspace_t colorspace_from_client_config(const config_t &config, bool hdr_display);

  /**
   * @brief AVCodec colorspace structure.
   */
  struct avcodec_colorspace_t {
    AVColorPrimaries primaries;  ///< Color primaries
    AVColorTransferCharacteristic transfer_function;  ///< Transfer characteristic
    AVColorSpace matrix;  ///< Color matrix
    AVColorRange range;  ///< Color range
    int software_format;  ///< Software pixel format
  };

  /**
   * @brief Convert Sunshine colorspace to AVCodec colorspace.
   * @param sunshine_colorspace The Sunshine colorspace configuration.
   * @return AVCodec colorspace structure.
   */
  avcodec_colorspace_t avcodec_colorspace_from_sunshine_colorspace(const sunshine_colorspace_t &sunshine_colorspace);

  /**
   * @brief Color transformation vectors structure (16-byte aligned).
   */
  struct alignas(16) color_t {
    float color_vec_y[4];  ///< Y component transformation vector
    float color_vec_u[4];  ///< U component transformation vector
    float color_vec_v[4];  ///< V component transformation vector
    float range_y[2];  ///< Y component range [min, max]
    float range_uv[2];  ///< UV component range [min, max]
  };

  /**
   * @brief Get color transformation vectors from colorspace.
   * @param colorspace The colorspace configuration.
   * @return Pointer to color transformation vectors.
   */
  const color_t *color_vectors_from_colorspace(const sunshine_colorspace_t &colorspace);

  /**
   * @brief Get color transformation vectors from colorspace enum and range.
   * @param colorspace The colorspace enum value.
   * @param full_range Whether to use full color range.
   * @return Pointer to color transformation vectors.
   */
  const color_t *color_vectors_from_colorspace(colorspace_e colorspace, bool full_range);

  /**
   * @brief New version of `color_vectors_from_colorspace()` function that better adheres to the standards.
   *        Returned vectors are used to perform RGB->YUV conversion.
   *        Unlike its predecessor, color vectors will produce output in `UINT` range, not `UNORM` range.
   *        Input is still in `UNORM` range. Returned vectors won't modify color primaries and color
   *        transfer function.
   * @param colorspace Targeted YUV colorspace.
   * @return `const color_t*` that contains RGB->YUV transformation vectors.
   *         Components `range_y` and `range_uv` are there for backwards compatibility
   *         and can be ignored in the computation.
   */
  const color_t *new_color_vectors_from_colorspace(const sunshine_colorspace_t &colorspace);
}  // namespace video
