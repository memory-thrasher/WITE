#include "stdafx.h"
#include "Camera.h"
#include "Shader.h"

Camera::Camera(WITE::IntBox3D size, Queue* present) : WITE::Camera(), presentQ(present) {
  resize(size);
}

Camera::~Camera() {}

float Camera::approxScreenArea(WITE::BBox3D* o) {
  WITE::BBox3D scrnAlloc, *scrn;
  scrn = renderTransform.getInv()->transform(o, &scrnAlloc);
  return (float)((scrn->maxx-scrn->minx)*(scrn->maxy-scrn->miny));
}

glm::dvec3 Camera::getLocation() {
  return worldTransform.getLocation();
}

void Camera::setLocation(glm::dvec3 l) {
  worldTransform.setLocation(l);
  updateMaths();
}

void Camera::render(std::shared_ptr<Queue::ExecutionPlan> ep) {
  VkCommandBuffer cmd = ep->beginParallel();
  VkViewport viewport = { 0, 0, (float)screenbox.width, (float)screenbox.height, 0.01f, 100 };//TODO clipping plane as setting
  VkRect2D scissors = {{(int32_t)screenbox.minx, (int32_t)screenbox.miny}, {(uint32_t)screenbox.width, (uint32_t)screenbox.height}};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissors);
  vkCmdBeginRenderPass(cmd, &passes[0].beginInfo, VK_SUBPASS_CONTENTS_INLINE);
  Shader::renderAll(ep, layerMask, renderTransform.getMat(), presentQ->gpu, passes[0].rp);
  vkCmdEndRenderPass(cmd);
}

void Camera::updateMaths() {
  renderTransform = glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
    glm::perspective(fov, (double)(screenbox.width) / screenbox.height, 0.1, 100.0) * //projection
    worldTransform.getMat();//view
}

void Camera::resize(WITE::IntBox3D newsize) {
  if(screenbox.sameSize(newsize)) {
    screenbox = newsize;
    return;
  }
  recreateResources();
}

void Camera::blitTo(VkCommandBuffer cmd, VkImage dst) {
  const VkImageSubresourceLayers subres0 = { VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1 };
  WITE::IntBox3D size = screenbox;
  VkImageBlit region = { subres0, {{0, 0, 0}, {(int32_t)size.width, (int32_t)size.height, 0}},
			 subres0, {{(int32_t)size.minx, (int32_t)size.miny, 0}, {(int32_t)size.maxx, (int32_t)size.maxy, 0}} };
  vkCmdBlitImage(cmd, passes[passCount-1].color->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		 dst, VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR, 1, &region, VK_FILTER_LINEAR);//TODO filter should be setting, or cubic if we msaa
}

void Camera::recreateResources() {
  WITE::IntBox3D size = screenbox;
  VkFormatProperties depthProps;
  vkGetPhysicalDeviceFormatProperties(presentQ->gpu->phys, RenderPass::DEPTH_FORMAT, &depthProps);
  for(size_t i = 0;i < passCount;i++) {
    BackedImage* img = new BackedImage
      (presentQ->gpu, {(uint32_t)size.width, (uint32_t)size.height},
       { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, NULL, VK_IMAGE_VIEW_TYPE_2D, RenderPass::COLOR_FORMAT,
	 { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
       }, {
	   VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D, RenderPass::COLOR_FORMAT,
	   { (uint32_t)size.width, (uint32_t)size.height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR,
	   VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_PREINITIALIZED
       });
    passes[i].color = std::unique_ptr<BackedImage>(img);
    img = new BackedImage
      (presentQ->gpu, {(uint32_t)size.width, (uint32_t)size.height},
       { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, NULL, VK_IMAGE_VIEW_TYPE_2D, RenderPass::DEPTH_FORMAT,
	 { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
       }, {
	   VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
	   RenderPass::DEPTH_FORMAT, { (uint32_t)size.width, (uint32_t)size.height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
	   (depthProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_TILING_LINEAR :
	   (depthProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_BEGIN_RANGE,
	   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED
       });
    passes[i].depth = std::unique_ptr<BackedImage>(img);
    CRASHIFFAIL(vkCreateRenderPass(presentQ->gpu->device, &RenderPass::rpInfo, NULL, &passes[i].rp));
    VkImageView attachments[2] = { passes[i].color->getImageView(), passes[i].depth->getImageView() };
    VkFramebufferCreateInfo fbInfo =
      { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, passes[i].rp, 2, attachments, (uint32_t)size.width, (uint32_t)size.height, 1 };
    CRASHIFFAIL(vkCreateFramebuffer(presentQ->gpu->device, &fbInfo, NULL, &passes[i].fb));
    passes[i].beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, passes[i].rp, passes[i].fb,
			    {{(int32_t)size.minx, (int32_t)size.miny}, {(uint32_t)size.width, (uint32_t)size.height}}, 2, RenderPass::CLEAR};
  }
}

WITE::Camera* WITE::Camera::make(WITE::Window* w, WITE::IntBox3D box) {
  return w->addCamera(box);
}

