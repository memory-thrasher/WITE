namespace WITE::Export {

  struct resource {
    enum class provider_t : uint8_t {
      source, target, constant;
    };
    enum class type_t : uint8_t {
      image, buffer
    };
    provider_t provider;
    type_t type;
    udm attributes; //one element for images, one or more for vertex buffers
  };

  struct outputDescription {
    uint16_t idx, views;//views is only for image resources
    constexpr uint32_t hashcode() { return ((uint32_t)idx << 16) | views; };
  };

}
