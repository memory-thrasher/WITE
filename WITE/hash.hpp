/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "literalList.hpp"
#include "onionTemplateStructs.hpp"
#include "wite_vulkan.hpp"

namespace WITE {

  constexpr uint64_t hashingPrime = 6700417;//non-mersenne to avoid collissions with ~0 in any size int
  typedef uint64_t hash_t;

  //for keying runtime maps using constexpr objects, such as vulkan *CreateInfo structs. Does not follow pointers.
  template<class T> consteval hash_t hash(const std::tuple<T> tuple);
  template<class T> requires std::is_same<T, void>::value consteval hash_t hash(const T* t);
  template<class T> requires (!std::is_same<T, void>::value) consteval hash_t hash(const T* t);
  template<class T> requires requires(const T& t){ t.id; } consteval hash_t hash(const T& t);
  template<class T, class U, class... V> consteval hash_t hash(const std::tuple<T, U, V...> tuple);
  template<class T> consteval inline size_t sizeofCollection(T t);
  template<class T> consteval hash_t hash(const T& t);
  consteval hash_t hash(const onionDescriptor& od);
  consteval hash_t hash(const layerRequirements& lr);
  template<class T> consteval hash_t hash(const vk::Flags<T> t);
  template<class T> consteval hash_t hash(const literalList<T>& ll);
  template<class T> consteval hash_t hash(const T& t);
  consteval hash_t hash(const vk::DescriptorSetLayoutBinding& dslb);
  consteval hash_t hash(const vk::DescriptorSetLayoutCreateInfo& ci);

  consteval hash_t hash(const vk::SamplerCreateInfo& s) {
    return hash(std::tie(s.sType, s.pNext, s.flags, s.magFilter, s.minFilter, s.mipmapMode, s.addressModeU, s.addressModeV, s.addressModeW, s.mipLodBias, s.anisotropyEnable, s.maxAnisotropy, s.compareEnable, s.compareOp, s.minLod, s.maxLod, s.borderColor, s.unnormalizedCoordinates));
  };

  template<class T> consteval hash_t hash(const literalList<T>& ll) {
    hash_t ret = hash(ll.len);
    for(const T& t : ll)
      ret = ret * hashingPrime + hash(t);
    return ret;
  };

  consteval hash_t hash(const onionDescriptor& od) {
    return hash(std::tie(od.IRS, od.BRS, od.RSS, od.CSRS, od.RPRS, od.CLS, od.CSS, od.LRS, od.OLS, od.TLS, od.SLS, od.GPUID));
  };

  consteval hash_t hash(const layerRequirements& lr) {
    return hash(std::tie(lr.clears, lr.copies, lr.renders, lr.computeShaders));
  };

  template<class T> consteval hash_t hash(const vk::Flags<T> t) {
    return static_cast<hash_t>(typename vk::Flags<T>::MaskType(t));//pointers
  };

  template<class T> consteval hash_t hash(const T& t) {
    return static_cast<hash_t>(t);//enums, smaller range ints etc
  };

  template<class T> requires (!std::is_same<T, void>::value) consteval hash_t hash(const T* t) {
    return hash(*t);//traversible pointer (last resort)
  };

  template<class T> requires requires(const T& t){ t.id; } consteval hash_t hash(const T& t) {
    return hash(t.id);//pointer to something in templateStructs (wherein ids are unique)
  };

  template<class T> requires std::is_same<T, void>::value consteval hash_t hash(const T* t) {
    return 5;//void pointers
  };

  template<class T> consteval hash_t hash(const std::tuple<T> tuple) {
    return hash(std::get<0>(tuple));
  };

  template<class T, class U, class... V> consteval hash_t hash(const std::tuple<T, U, V...> tuple) {
    return hash(std::get<0>(tuple)) * hashingPrime + hash(popTupleFront(tuple));
  };

  consteval hash_t hash(const vk::DescriptorSetLayoutBinding& dslb) {
    return hash(std::tie(dslb.binding, dslb.descriptorType, dslb.descriptorCount, dslb.stageFlags));
  };

  consteval hash_t hash(const vk::DescriptorSetLayoutCreateInfo& ci) {
    return hash(literalList<vk::DescriptorSetLayoutBinding>(ci.pBindings, ci.bindingCount));
  };

}
