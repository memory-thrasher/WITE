#pragma once

#include "stdafx.h"
#include "Globals.h"
#include "Export.h"

class Debugger {
public:
  Debugger() = delete;
  ~Debugger() = delete;
  static const void* preInit();//call before creating instance
  static const void doInit();
  static const void beginLabel(VkCommandBuffer cmd, const char* label, const float color[4] = NULL);
  static const void endLabel(VkCommandBuffer cmd);
  static const void insertLabel(VkCommandBuffer cmd, const char* label, const float color[4] = NULL);
  static const void setObjectName(GPU* device, VkObjectType, void* objectHandle, const char* objectName, ...);
  //TODO hook more of this up
private:
  static VkDebugUtilsMessengerEXT messenger;
  static VkDebugUtilsMessengerCreateInfoEXT messengerInfo;
  static bool inited;
  //function points from vk
  static PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilMessenger;
  static PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilMessenger;
  static PFN_vkSubmitDebugUtilsMessageEXT submitDebugUtilsMessage;
  static PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabel;
  static PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabel;
  static PFN_vkCmdInsertDebugUtilsLabelEXT insertDebugUtilsLabel;
  static PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectName;
};

