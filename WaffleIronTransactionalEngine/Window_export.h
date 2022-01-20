#pragma once

class export_def WITE::Window {
 public:
  Window(const Window&) = delete;//no copy
  virtual ~Window() = default;
  virtual size_t getCameraCount() = 0;
  virtual WITE::IntBox3D getBounds() = 0;
  virtual void setSize(uint32_t width, uint32_t height) = 0;
  virtual void setBounds(IntBox3D) = 0;
  virtual void setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
  virtual Camera* addCamera(IntBox3D, std::shared_ptr<WITE::ImageSource>) = 0;
  virtual Camera* getCamera(size_t idx) = 0;
  virtual Queue* getGraphicsQueue() = 0;
  static std::unique_ptr<Window> make(size_t display = 0);
  static Window** iterateWindows(size_t &num);
 protected:
  Window() = default;
};
