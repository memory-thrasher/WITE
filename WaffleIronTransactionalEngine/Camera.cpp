#include "stdafx.h"
#include "Camera.h"


Camera::Camera(IntBox3D size, Queue* present) : presentQ(present) {
  resize(size);
}

Camera::~Camera() {}

float Camera::approxScreenArea(BBox3D* o) {
  BBox3D scrnAlloc, *scrn;
  scrn = renderTransform.getInv()->transform(o, &scrnAlloc);
  return (scrn->maxx-scrn->minx)*(scrn->maxy-scrn->miny);
}

glm::vec3 Camera::getLocation() {
  return worldTransform[3];
}

void Camera::setLocation(glm::vec3d l) {
  worldTransform[3] = glm::vec(l, 1.0f);
  updateMaths();
}

void Camera::render(Queue::ExecutionPlan* ep) {
  VkCommandBuffer cmd = ep->beginParallel();
  VkViewport viewport = { 0, 0, screenbox.width, screenbox.height, 0.01f, 100f };//TODO clipping plane as setting
  VkScissors scissors = {{screenbox.width>>1, screenbox.height>>1}};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissors(cmd, 0, 1, &scissors);
  vkCmdBeginRenderPass(cmd, &passes[0].beginInfo, VK_SUBPASS_CONTENTS_INLINE);
  Shader::renderAll(ep, layerMask, renderTransform.getMat(), gpu);
  vkEndRenderPass(cmd);
}

void Camera::updateMaths() {
  renderTransform = glm::mat4d(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
    glm::perspective(fov, screenbox.width2D() / screenbox.height2D(), 0.1f, 100f) * //projection
    worldTransform.getMat();//view
}

void Camera::resize(IntBox3D newsize) {
  if(screenbox.sameSize(newsize)) {
    screenbox = newsize;
    return;
  }
  recreateResources();
}

void Camera::blitTo(VkCommandBuffer cmd, VkImage dst) {
  const VkImageSubresourceLayers subres0 = { VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1 };
  IntBox3D size = screenbox;
  VkImageBlit region = { subres0, {{0, 0, 0}, {size.width, size.height, 0}}, subres0, {{size.minx, size.miny, 0}, {size.maxx, size.maxy, 0}} };
  vkCmdBlitImage(cmd, passes[passCount-1].color->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		 dst, VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR, 1, &region, VK_FILTER_NONE);
}

void Camera::recreateResources() {
  IntBox3D size = screenbox;
  VkFormatProperties depthProps;
  vkGetPhysicalDeviceFormatProperties(presentQ->gpu->device, RenderPass.DEPTH_FORMAT, &depthProps);
  for(size_t i = 0;i < passCount;i++) {
    passes[i].color = std::make_unique(presentQ->gpu, {size.width, size.height},
				       { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, NULL, VK_IMAGE_VIEW_TYPE_2D, RenderPass.COLOR_FORMAT,
					 { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
					 { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				       }, {
					   VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D, RenderPass.COLOR_FORMAT,
					   { size.width, size.height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR,
					   VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_PREINITIALIZED });
    passes[i].depth = std::make_unique(presentQ->gpu, {size.width, size.height},
				       { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, NULL, VK_IMAGE_VIEW_TYPE_2D, RenderPass.DEPTH_FORMAT,
					 { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
					 { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
				       }, {
					   VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
					   RenderPass.DEPTH_FORMAT, { size.width, size.height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
					   (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_TILING_LINEAR :
					   (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ?
					   VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_BEGIN_RANGE,
					   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED
				       });
    CRASHIFFAIL(vkCreateRenderPass(gpu->device, &RenderPass.rpInfo, NULL, &passes[i].renderPass));
    VkImageView attachments[2] = { passes[i].color->imageView, passes[i].depth->imageView };
    VkFramebufferCreateInfo fbInfo =
      { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, passes[i].renderPass, 2, attachments, size.width, size.height, 1 };
    CRASHIFFAIL(vkCreateFramebuffer(presentQ->gpu->device, &fbInfo, NULL, &passes[i].fb));
    passes[i].beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, passes[i].renderPass, passes[i].fb,
			    {{size.minx, size.miny}, {size.width, size.height}}, 2, &RenderPass::CLEAR};
  }
}

Camera* WITE::Camera::make(WITE::Window* w, IntBox3D box) {
  w->addCamera(box);
}
