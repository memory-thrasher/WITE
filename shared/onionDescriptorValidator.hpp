/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <vector>
#include <set>
#include <string>
#include <format>

#include "../WITE/onionTemplateStructs.hpp"

#define pushCheck(set, id, name) if(!set.insert(id).second) errors.push_back(std::format("Duplicate id {} found in set {}", id, name));
#define containsCheck(set, id, name, expector, expectorId) if(!set.contains(id)) errors.push_back(std::format("Missing key {} in set {} expected by {} with id {}", id, name, expector, expectorId));

namespace WITE::validation {

  std::vector<std::string> errors;
  std::set<uint64_t> allIds, requirementIds, BRids, IRids, RCids, RSids, TLids, SLids, layoutIds, OLids, CSRids, GSRids, RPids, RPColorIds, RPDepthIds, CPids, CLids;

  void validateResourceConsumers(const literalList<resourceConsumer>& RCS) {
    for(const auto& X : RCS) {
      pushCheck(RCids, X.id, "resource consumers");
      pushCheck(allIds, X.id, "all ids");
      //TODO check X.usage ?
    }
  };

  void validateResourceReferences(const literalList<resourceReference>& RRS) {
    for(const auto& X : RRS) {
      containsCheck(RCids, X.resourceConsumerId, "resource consumers", "resource reference for slot", X.resourceSlotId);
      containsCheck(RSids, X.resourceSlotId, "resource slots", "resource reference for consumer", X.resourceConsumerId);
      //TODO validate subresource <= requirement size?
      //TODO validate frameLatency <= requirement swapcount?
    }
  };

  const std::vector<std::string>& getErrors(const onionDescriptor& OD) {
    for(const auto& X : OD.IRS) {
      pushCheck(IRids, X.id, "image requirements");
      pushCheck(requirementIds, X.id, "requirements");
      pushCheck(allIds, X.id, "all ids");
      if(X.deviceId != OD.GPUID) errors.push_back(std::format("onion data and requirement with id {} have gpu mismatch", X.id));
    }
    for(const auto& X : OD.BRS) {
      pushCheck(BRids, X.id, "buffer requirements");
      pushCheck(requirementIds, X.id, "requirements");
      pushCheck(allIds, X.id, "all ids");
      if(X.deviceId != OD.GPUID) errors.push_back(std::format("onion data and requirement with id {} have gpu mismatch", X.id));
    }
    for(const auto& X : OD.OLS) {
      pushCheck(OLids, X.id, "object layout");
      pushCheck(allIds, X.id, "all ids");
      if(X.windowConsumerId != NONE) {
	pushCheck(RCids, X.windowConsumerId, "resource consumers");
	pushCheck(allIds, X.windowConsumerId, "all ids");
      }
    }
    for(const auto& X : OD.RSS) {
      pushCheck(RSids, X.id, "resource slots");
      pushCheck(allIds, X.id, "all ids");
      containsCheck(requirementIds, X.requirementId, "requirements", "resource slot", X.id);
      containsCheck(OLids, X.objectLayoutId, "object layouts", "resource slot", X.id);
    }
    for(const auto& X : OD.CSRS) {
      pushCheck(CSRids, X.id, "compute shaders");
      pushCheck(allIds, X.id, "all ids");
      validateResourceConsumers(X.targetProvidedResources);
      validateResourceConsumers(X.sourceProvidedResources);
      if(X.primaryOutputSlotId != NONE)
	containsCheck(RSids, X.primaryOutputSlotId, "resource slots", "compute shader", X.id);
    }
    for(const auto& X : OD.RPRS) {
      pushCheck(RPids, X.id, "render passes");
      pushCheck(allIds, X.id, "all ids");
      if(X.depth != NONE) {
	pushCheck(RCids, X.depth, "resource consumers");
	pushCheck(allIds, X.depth, "all ids");
      }
      for(uint64_t id : X.color) {
	if(id != NONE) {
	  pushCheck(RCids, id, "resource consumers");
	  pushCheck(allIds, id, "all ids");
	}
      }
      for(uint64_t id : X.input) {
	pushCheck(RCids, id, "resource consumers");
	pushCheck(allIds, id, "all ids");
      }
      for(const auto& S : X.shaders) {
	validateResourceConsumers(S.targetProvidedResources);
	validateResourceConsumers(S.sourceProvidedResources);
	pushCheck(GSRids, S.id, "graphics shaders");
	pushCheck(allIds, S.id, "all ids");
      }
    }
    for(const auto& X : OD.CSS) {
      pushCheck(CPids, X.id, "copy steps");
      pushCheck(allIds, X.id, "all ids");
      pushCheck(RCids, X.src, "resource consumers");
      pushCheck(allIds, X.src, "all ids");
      pushCheck(RCids, X.dst, "resource consumers");
      pushCheck(allIds, X.dst, "all ids");
    }
    for(const auto& X : OD.CLS) {
      pushCheck(CLids, X.id, "clear steps");
      pushCheck(allIds, X.id, "all ids");
    }
    for(const auto& X : OD.TLS) {
      pushCheck(TLids, X.id, "target layout");
      pushCheck(layoutIds, X.id, "requirements");
      pushCheck(allIds, X.id, "all ids");
      containsCheck(OLids, X.objectLayoutId, "object layouts", "target layout", X.id);
      validateResourceReferences(X.resources);
    }
    for(const auto& X : OD.SLS) {
      pushCheck(SLids, X.id, "source layout");
      pushCheck(layoutIds, X.id, "requirements");
      pushCheck(allIds, X.id, "all ids");
      containsCheck(OLids, X.objectLayoutId, "object layouts", "source layout", X.id);
      validateResourceReferences(X.resources);
    }
    for(const auto& X : OD.LRS) {
      for(auto id : X.clears)
	containsCheck(CLids, id, "clear steps", "layer", "N/A");
      for(auto id : X.copies)
	containsCheck(CPids, id, "copy steps", "layer", "N/A");
      for(auto id : X.renders)
	containsCheck(RPids, id, "render passes", "layer", "N/A");
      for(auto id : X.computeShaders)
	containsCheck(CSRids, id, "compute shaders", "layer", "N/A");
    }
    //TODO validate requirement usages from access flags of consumers
    return errors;
  };

  //main should just call and return this for a dedicated test case (without actually materializing a onion type)
  int validate(const onionDescriptor& OD) {
    const auto& es = getErrors(OD);
    for(const std::string& e : es)
      std::cerr << e << "\n";
    return es.size();
  };

}

#undef pushCheck
#undef containsCheck
