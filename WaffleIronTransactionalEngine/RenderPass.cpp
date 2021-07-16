#include "Internal.h"

namespace WITE_internal {

  RenderPass::RenderPass(WITE::Queue* q, WITE::ImageSource **inputs, size_t inputCount, uint64_t outputImageUsage) :
    inputCount(inputCount), outputImageUsage(outputImageUsage | USAGE_ATTACH_AUTO), queue(reinterpret_cast<Queue*>(q))
  {//TODO double-buffer option
    this.attachments = std::make_unique<VkAttachmentDescription[]>(2 + inputCount);
    auto attachments = this.attachments.get();
    attachments[0] = { 0, FORMAT_STD_COLOR, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
    attachments[1] = { 0, DEPTH_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
		       VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    if(inputCount) {
      this.inputs = malloc(sizeof(*inputs) * inputCount);
      memcpy(this.inputs, inputs, inputCount * sizeof(*inputs));
      inputReferences = new std::make_unique<VkAttachmentReference[]>(inputCount);
      auto inputRefs = inputReferences.get();
      for(size_t i = 0;i < inputCount;i++) {
	inputRefs[i] = { i + 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };//i+2 bc 0 and 1 are color and depth
	attachments[i+2] = { 0, inputs[i].getFormat(), VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_NONE_QCOM, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			     VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
      }
    } else {
      this.inputs = NULL;
      inputReferences = NULL;
    }
    subpass = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, inputCount, NULL, 1, &colorReference, NULL, &depthReference, 0, NULL };
    rpInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 2 + inputCount, attachments, 1, &subpass, 0, NULL };//TODO subpass dependencies?
    CRASHIFFAIL(vkCreateRenderPass(gpu->device, &rpInfo, NULL, &rp));
    beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp, NULL, {{0, 0}, {0, 0}}, 2, CLEAR };
    attachmentViews = new VkImageView[inputCount + 2];
    fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, NULL, inputCount + 2, attachmentViews, 0, 0, 1 };
  }

    RenderPass::RenderPass(WITE::Queue* q) : this(q, NULL, 0, USAGE_TRANSFER_SRC) {}

  void render(WITE::Camera* wcam) {
    Camera* reinterpret_cast<Camera*>(wcam);
    auto q = static_cast<WITE_internal::Queue*>(wq);
    VkCommandBuffer cmd = q->getComplexPlan()->beginSerial();
    //transition all input images to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL with read bit
    bool needNewFB = false;
    for(size_t i = 0;i < inputCount;i++) {
      BackedImage* img = inputs[i].getImage();
      img->setLayout(LAYOUT_SHADER_RO, cmd, queue);
      uint64_t serial = img->serialNumber();
      if(serial > fbResourceSerial) {
	needNewFB = true;
	fbResourceSerial = serial;
      }
    }
    if(needNewFB) recreateFB();
    vkBeginRenderPass(cmd, beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    Shader::renderAll(queue->getComplexPlan(), layerMask, cam->getRenderMatrix(), queue->gpu, rp);//uh, looks like this doesn't exist anymore
    vkCmdEndRenderpass(cmd);;
  }

  void recreateFB() {
    fbInfo.renderPass = rp;
    fbInfo.pAttachments = attachmentViews;
    vkCreateFramebuffer(queue->gpu->device, &fbInfo, NULL, &fb);
  }

  void setOutputSize(WITE::IntBox3D screensize) {
    size_t width = screensize.width();
    size_t height = screensize.height();
    if(width == outputSize.width() && height == outputSize.height()) return;
    outputSize = screensize;
    if(fb) vkDestroyFramebuffer(queue->gpu->device, fb, NULL);
    colorOutImage = std::make_unique<BackedImage>(gpu, width, height, FORMAT_STD_COLOR, outputImageUsage);
    depthOutImage = std::make_unique<BackedImage>(gpu, width, height, FORMAT_STD_DEPTH, outputImageUsage);
    attachmentViews[0] = colorOutpImage->getImageView();
    attachmentViews[1] = colorOutpImage->getImageView();
    beginInfo.renderArea.extent = {width, height};
    fbInfo.width = width;
    fbInfo.height = height;
    recreateFB();
  }

}
