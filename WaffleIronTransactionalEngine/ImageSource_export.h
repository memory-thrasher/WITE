#pragma once

namespace WITE {

  class ImageSource {
  public:
    ImageSource(const ImageSource&) = delete;
    virtual const BackedImage* getImages() = 0;
    virtual size_t getImageCount() = 0;
    virtual int64_t getFormat() = 0;
    //the rest are optional
    virtual void requestResize() {};
    virtual size_t getReadIdx() { return database->getCurrentFrame() % getImageCount(); };
    virtual size_t getWriteIdx() { return (database->getCurrentFrame() + 1) % getImageCount(); };
    virtual const BackedImage* getReadImage() { return getImages()[getReadIdx()]; };
    virtual BackedImage* getWriteImage() { return getImages()[getWriteIdx()]; };
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
