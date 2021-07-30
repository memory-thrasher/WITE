#pragma once

namespace WITE {

  class ImageSource {
  public:
    ImageSource(const ImageSource&) = delete;
    virtual BackedImage* getImage(WITE::IntBox3D) = 0;
  };

  class StaticImageSource : public ImageSource {
  public:
    StaticImageSource(BackedImage* i) : image(i) {};
    StaticImageSource(std::shared_ptr<BackedImage> i) : image(i) {};
    BackedImage* getImage(WITE::IntBox3D) { return image.get(); };
  private:
    std::shared_ptr<BackedImage> image;
  };

}
