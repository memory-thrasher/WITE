#include "Internal.h"

static constexpr float defColor[4] = {0, 0, 0, 0};

VkDebugUtilsMessengerEXT Debugger::messenger;
bool Debugger::inited = false;
PFN_vkCreateDebugUtilsMessengerEXT Debugger::createDebugUtilMessenger;
PFN_vkDestroyDebugUtilsMessengerEXT Debugger::destroyDebugUtilMessenger;
PFN_vkSubmitDebugUtilsMessageEXT Debugger::submitDebugUtilsMessage;
PFN_vkCmdBeginDebugUtilsLabelEXT Debugger::beginDebugUtilsLabel;
PFN_vkCmdEndDebugUtilsLabelEXT Debugger::endDebugUtilsLabel;
PFN_vkCmdInsertDebugUtilsLabelEXT Debugger::insertDebugUtilsLabel;
PFN_vkSetDebugUtilsObjectNameEXT Debugger::setDebugUtilsObjectName;

VkBool32 logValidationIssue(VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
  LOG("Khronos validation says: severity: %x, type: %x, message (%d): %s\n", sev, types, callbackData->messageIdNumber, callbackData->pMessage);
  if(callbackData->queueLabelCount + callbackData->cmdBufLabelCount + callbackData->objectCount)
    LOG("Involving the following named objects:\n");
  size_t i;
  for(i = 0;i < callbackData->queueLabelCount;i++)
    LOG("\tQueue: %s\n", callbackData->pQueueLabels[i].pLabelName);
  for(i = 0;i < callbackData->cmdBufLabelCount;i++)
    LOG("\tcmd: %s\n", callbackData->pCmdBufLabels[i].pLabelName);
  for(i = 0;i < callbackData->objectCount;i++)
    LOG("\tother (type %d): %s\n", callbackData->pObjects[i].objectType, callbackData->pObjects[i].pObjectName);
  return true;
}

VkDebugUtilsMessengerCreateInfoEXT Debugger::messengerInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, NULL, 0, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &logValidationIssue, NULL};

const void* Debugger::preInit() {
  auto vk = get_vkSingleton();
  for(size_t i = 0;i < VALIDATION_EXTENSION_COUNT;i++)
    vk->extensions[vk->extensionCount++] = VALIDATION_EXTENSIONS[i];
  vk->layers[vk->layerCount++] = "VK_LAYER_KHRONOS_validation";
  return &messengerInfo;
}

const void Debugger::doInit() {
  auto vk = get_vkSingleton();
  if(debugMode & DEBUG_MASK_VULKAN_VERBOSE)
    messengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  createDebugUtilMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk->instance, "vkCreateDebugUtilsMessengerEXT");
  destroyDebugUtilMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk->instance, "vkDestroyDebugUtilsMessengerEXT");
  submitDebugUtilsMessage = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(vk->instance, "vkSubmitDebugUtilsMessageEXT");
  beginDebugUtilsLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(vk->instance, "vkCmdBeginDebugUtilsLabelEXT");
  endDebugUtilsLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(vk->instance, "vkCmdEndDebugUtilsLabelEXT");
  insertDebugUtilsLabel = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(vk->instance, "vkCmdInsertDebugUtilsLabelEXT");
  setDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(vk->instance, "vkSetDebugUtilsObjectNameEXT");
  CRASHIFFAIL(createDebugUtilMessenger(vk->instance, &messengerInfo, NULL, &messenger));
  inited = true;
}

const void Debugger::beginLabel(VkCommandBuffer cmd, const char* label, const float color[4]) {
  if(!inited) return;
  if(!color) color = defColor;
  VkDebugUtilsLabelEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, label, { color[0], color[1], color[2], color[3] } };
  beginDebugUtilsLabel(cmd, &info);
}

const void Debugger::endLabel(VkCommandBuffer cmd) {
  if(!inited) return;
  endDebugUtilsLabel(cmd);
}

const void Debugger::insertLabel(VkCommandBuffer cmd, const char* label, const float color[4]) {
  if(!inited) return;
  if(!color) color = defColor;
  VkDebugUtilsLabelEXT info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, label, {color[0], color[1], color[2], color[3]}};
  insertDebugUtilsLabel(cmd, &info);
}

const void Debugger::setObjectName(GPU* device, VkObjectType type, void* objectHandle, const char* objectName, ...) {
  if(!inited) return;
  va_list args;
  va_start(args, objectName);
  int sizeofString = vsnprintf(NULL, 0, objectName, args);
  va_end(args);
  char* name = static_cast<char*>(malloc(sizeofString+1));//FIXME memory leak
  va_start(args, objectName);
  vsprintf(name, objectName, args);
  va_end(args);
  VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, NULL, type, reinterpret_cast<uint64_t>(objectHandle), name};
  CRASHIFFAIL(setDebugUtilsObjectName(device->device, &info));
}

