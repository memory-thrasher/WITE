#pragma once

#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC //makes vk::Floags<T> qualify as "structural" so it can be used in templates
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define ALLOCCB (static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks*>(NULL)) //might actually use this later, so pass it everywhere

namespace WITE {

  template<class TO, class FROM> consteval TO vkFlagCast(FROM f) {
    return TO(TO::MaskType(FROM::MaskType(f))) & vk::FlagTraits<TO>::allFlags;
  };

  //add overrides as needed. Note that vulka_handles.hpp:*.reflect is not constexpr but is otherwise identical to this
  constexpr auto vkToTuple(vk::SamplerCreateInfo ci) {
    return std::tie(ci.sType,
		    ci.pNext,
		    ci.flags,
		    ci.magFilter,
		    ci.minFilter,
		    ci.mipmapMode,
		    ci.addressModeU,
		    ci.addressModeV,
		    ci.addressModeW,
		    ci.mipLodBias,
		    ci.anisotropyEnable,
		    ci.maxAnisotropy,
		    ci.compareEnable,
		    ci.compareOp,
		    ci.minLod,
		    ci.maxLod,
		    ci.borderColor,
		    ci.unnormalizedCoordinates);
  };

}


