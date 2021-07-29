#include "Internal.h"

namespace WITE_internal {

  Camera::Camera(WITE::IntBox3D size, Queue* graphics, Render_cb_t render_cb) : WITE::Camera(), graphicsQ(graphics), render_cb(render_cb), fov(M_PI * 0.5f) {
    resize(size);
  }

  Camera::~Camera() {}

  float Camera::approxScreenArea(WITE::BBox3D* o) {
    WITE::BBox3D scrnAlloc, *scrn;
    scrn = renderTransform.transform(o, &scrnAlloc);
    return (float)((scrn->maxx-scrn->minx)*(scrn->maxy-scrn->miny))*180;
    //TODO this needs work
  }

  glm::dvec3 Camera::getLocation() {
    return worldTransform.getLocation();
  }

  void Camera::setLocation(glm::dvec3 l) {
    worldTransform.setLocation(l);
    updateMaths();
  }

  void Camera::setMatrix(glm::dmat4& n) {
    worldTransform.setMat(&n);
    updateMaths();
  }

  void Camera::setMatrix(glm::dmat4* n) {
    worldTransform.setMat(n);
    updateMaths();
  }

  void Camera::getRenderMatrix() {
    return renderTransform.getMat();
  }

  void Camera::render(Queue::ExecutionPlan* ep) {
    VkCommandBuffer cmd = ep->beginParallel();//cmd 0
    VkViewport viewport = { 0, 0, (float)screenbox.width(), (float)screenbox.height(), 0.01f, 1.0f };//TODO clipping plane as setting
    VkRect2D scissors = {{(int32_t)screenbox.minx, (int32_t)screenbox.miny}, {(uint32_t)screenbox.width(), (uint32_t)screenbox.height()}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissors);
    render_cb->call(ep);
    /*vkCmdBeginRenderPass(cmd, &passes[0].beginInfo, VK_SUBPASS_CONTENTS_INLINE);
      Shader::renderAll(ep, layerMask, renderTransform.getMat(), graphicsQ->gpu, passes[0].rp);
      vkCmdEndRenderPass(cmd);*/
  }

  void Camera::updateMaths() {
    //renderTransform = glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
    //  glm::perspective(fov, (double)(screenbox.width) / screenbox.height, 0.1, 100.0) * //projection
    //  worldTransform.getMat();//view
    auto clip = glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1),
      proj = glm::perspective(fov, (double)screenbox.width() / screenbox.height(), 0.1, 100.0),
      view = worldTransform.getMat(),
      rt = clip * proj * view;
    renderTransform = rt;
    //LOG("Size: %d,%d\n", screenbox.width(), screenbox.height());
    //LOGMAT(clip, "clip");
    //LOGMAT(proj, "proj");
    //LOGMAT(view, "view");
    //LOGMAT(rt, "renderTransform");
  }

  void Camera::resize(WITE::IntBox3D newsize) {
    if(screenbox.sameSize(newsize)) return;
    screenbox = newsize;
    recreateResources();
  }

  void Camera::blitTo(VkCommandBuffer cmd, BackedImage* dst) {
    const VkImageSubresourceLayers subres0 = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    WITE::IntBox3D size = screenbox;
    auto src = passes[passCount-1].getColorOutput();
    VkImageBlit region = { subres0, {{0, 0, 0}, {(int32_t)size.width(), (int32_t)size.height(), 1}}, subres0, {{(int32_t)size.minx, (int32_t)size.miny, 0}, {(int32_t)size.maxx, (int32_t)size.maxy, 1}} };
    vkCmdBlitImage(cmd, src->getImage(), src->getLayout(), dst, dst->getLayout(), 1, &region, VK_FILTER_NEAREST);//TODO filter should be setting, or cubic if we msaa
  }

}

WITE::Camera* WITE::Camera::make(WITE::Window* w, WITE::IntBox3D box, WITE::Camera::Render_cb_t rcb) {
  return w->addCamera(box, rcb);
}

