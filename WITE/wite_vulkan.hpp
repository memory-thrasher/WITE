#pragma once

#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC //makes vk::Floags<T> qualify as "structural" so it can be used in templates
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#define ALLOCCB (static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks*>(NULL)) //might actually use this later, so pass it everywhere

// template<class T> inline constexpr bool bitmaskContains(const vk::Flags<T> l, const vk::Flags<T> r) {
//   return (l & r) != vk::Flags<T>(0);
// }

// template<class T> inline constexpr bool nonzero(const T l) {
//   return l != T(0);
// };

// namespace WITE::GPU {
//   class cmd_t {//pool free requires the original reference, so encapsulate it to avoid getting a ptr to a copy of the handle
//   private:
//     vk::CommandBuffer* cmd;
//     cmd_t(vk::CommandBuffer* cmd) : cmd(cmd) {};
//     friend class Queue;
//   public:
//     cmd_t(const cmd_t& o) : cmd_t(o.cmd) {};
//     explicit cmd_t() : cmd(NULL) {};
//     inline operator bool() { return cmd; };
//     ~cmd_t() = default;
//     inline vk::CommandBuffer* operator->() { return cmd; };
//     inline vk::CommandBuffer operator*() { return *cmd; };
//   };
// };
