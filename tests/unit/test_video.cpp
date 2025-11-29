/**
 * @file tests/unit/test_video.cpp
 * @brief Test src/video.*.
 */
#include "../tests_common.h"

#include <src/video.h>

struct EncoderTest: PlatformTestSuite, testing::WithParamInterface<video::encoder_t *> {
  void SetUp() override {
    auto &encoder = *GetParam();
    if (!video::validate_encoder(encoder, false)) {
      // Encoder failed validation,
      // if it's software - fail, otherwise skip
      if (encoder.name == "software") {
        FAIL() << "Software encoder not available";
      } else {
        GTEST_SKIP() << "Encoder not available";
      }
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
  EncoderVariants,
  EncoderTest,
  testing::Values(
#if !defined(__APPLE__)
    &video::nvenc,
#endif
#ifdef _WIN32
    &video::amdvce,
    &video::quicksync,
#endif
#ifdef __linux__
    &video::vaapi,
#endif
#ifdef __APPLE__
    &video::videotoolbox,
#endif
    &video::software
  ),
  [](const auto &info) {
    return std::string(info.param->name);
  }
);

TEST_P(EncoderTest, ValidateEncoder) {
  // todo:: test something besides fixture setup
}

TEST(PacketRawTests, ReplacementsDefaultNullptr) {
  // Define a simple concrete packet type for testing the base class behaviour
  struct test_packet_t: video::packet_raw_t {
    bool is_idr() override {
      return false;
    }

    int64_t frame_index() override {
      return 0;
    }

    uint8_t *data() override {
      return nullptr;
    }

    size_t data_size() override {
      return 0;
    }
  };

  test_packet_t packet;
  ASSERT_EQ(packet.replacements, nullptr);
}
