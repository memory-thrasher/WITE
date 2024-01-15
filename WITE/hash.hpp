#pragma once

#include "literalList.hpp"
#include "templateStructs.hpp"
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

  template<class T> requires requires(const T& t) { t.reflect(); } consteval hash_t hash(const T& t) {
    return hash(t.reflect());
  };

  template<class T> consteval hash_t hash(const literalList<T>& ll) {
    hash_t ret = hash(ll.len);
    for(const T& t : ll)
      ret = ret * hashingPrime + hash(t);
    return ret;
  };

  consteval hash_t hash(const onionDescriptor& od) {
    return hash(std::tie(od.IRS, od.BRS, od.CSRS, od.RPRS, od.CSS, od.LRS, od.TLS, od.SLS, od.GPUID));
  };

  consteval hash_t hash(const layerRequirements& lr) {
    return hash(std::tie(lr.sourceLayouts, lr.targetLayouts, lr.copies, lr.renders, lr.computeShaders));
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