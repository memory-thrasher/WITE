namespace WITE::Export {

  struct resource {
    enum class provider_t : uint8_t {
      source, target, constant, none; //none = create internal disposable ...?
    };
    enum class resourceType_t : uint8_t {
      image, buffer
    };
    provider_t provider;
    resourceType_t type;
  };

}
