#include "Gpu.hpp"

#include "Vulkan.hpp"

namespace WITE::GPU {

  class ImageBase {
  public:

    struct ImageFormatFamily {
      int8_t components, componentsSize;
      friend constexpr auto operator<(const ImageFormatFamily& l, const ImageFormatFamily& r) {
	return l.components < r.components || (l.components == r.components && l.componentsSize < r.componentsSize);
      };
    };

    static constexpr int32_t
    USAGE_INDIRECT = 1,
      USAGE_VERTEX = 2,
      USAGE_DS_READ = 4,
      USAGE_DS_WRITE = 8,
      USAGE_DS_SAMPLED = 0X10,
      USAGE_ATT_DEPTH = 0X20,
      USAGE_ATT_INPUT = 0X40,
      USAGE_ATT_OUTPUT = 0X80,
      USAGE_HOST_READ = 0X100,
      USAGE_HOST_WRITE = 0X200,
      USAGE_IS_CUBE = 0X1000;

    const vk::Format format;
    const int64_t usage, ldm;
    const int32_t al, mip;
    const int8_t dim, comp, comp_size, sam;

    virtual ~ImageBase() = default;
    ImageBase(ImageBase&) = delete;
    ImageBase(int64_t usage, int8_t dim, int8_t comp, int8_t comp_size, int32_t al, int32_t mip, int8_t sam, int64_t ldm);
    void getCreateInfo(Gpu&, void* out, size_t width, size_t height, size_t z = 1);
  };

  //MIP and SAM might be less than requested if the platform does not support it
  template<int64_t USAGE, int8_t DIM, int8_t COMP, int8_t COMP_SIZE, int32_t AL, int32_t MIP, int8_t SAM, int64_t LDM>
  class Image : ImageBase {
  public:
    Image() : ImageBase(USAGE, DIM, COMP, COMP_SIZE, AL, MIP, SAM, LDM) {};
  };

}
