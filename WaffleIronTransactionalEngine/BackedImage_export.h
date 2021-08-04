#pragma once

namespace WITE {
  class BackedImage : public WITE::ShaderResource {
  public:
    BackedImage(const BackedImage&) = delete;
    virtual size_t getSize() = 0;
    virtual size_t getWidth() = 0;
    virtual size_t getHeight() = 0;
    virtual uint64_t serialNumber() = 0;
    static std::unique_ptr<BackedImage> make(GPU* dev, size_t width, size_t height, uint32_t format, uint64_t imageUsages = USAGE_SAMPLED, uint32_t mipmap = 1);
    //TODO bigger constructor with more options
  protected:
    BackedImage(BackedImage& in) = delete;
    BackedImage() = default;
  };
}
