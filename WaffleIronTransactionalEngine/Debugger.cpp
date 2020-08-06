#include "Debugger.h"

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
  LOG("Khronos validation says: severity: %x, type: %x, message: %s\n", sev, types, callbackData->pMessage);
  return true;
}

const VkDebugUtilsMessengerCreateInfoEXT Debugger::messengerInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, NULL, 0, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &logValidationIssue, NULL};

const void Debugger::doInit() {
  auto vk = get_vkSingleton();
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

const void Debugger::beginLabel(VkCommandBuffer cmd, const char* label, float color[4]) {
  if(!inited) return;
  VkDebugUtilsLabelEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, label, { color[0], color[1], color[2], color[3] } };
  beginDebugUtilsLabel(cmd, &info);
}

const void Debugger::endLabel(VkCommandBuffer cmd) {
  if(!inited) return;
  endDebugUtilsLabel(cmd);
}

const void Debugger::insertLabel(VkCommandBuffer cmd, const char* label, float color[4]) {
  if(!inited) return;
  VkDebugUtilsLabelEXT info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, label, {color[0], color[1], color[2], color[3]}};
  insertDebugUtilsLabel(cmd, &info);
}

const void Debugger::setObjectName(GPU* device, VkObjectType type, void* objectHandle, const char* objectName) {
  if(!inited) return;
  VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, NULL, type, reinterpret_cast<uint64_t>(objectHandle), objectName };
}

