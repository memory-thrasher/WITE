/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <queue>

#include "wite_vulkan.hpp"
#include "syncLock.hpp"

namespace WITE {

  //primary rendering and other complex work belongs in an onion, which manages it's own cmds. This is for one-offs and small async jobs. Not thread safe (except for in handling the shared queue); one pool per thread exists in the gpu class. Ensure no other thread attempts access. Do not wait the fence outside waitFor. Call waitFor at most once per submit. Do not resubmit.

  struct cmdPool;

  struct tempCmd {
    vk::CommandBuffer cmd;
    vk::Fence fence;
    cmdPool*pool;
    void submit();
    bool isPending();
    void waitFor();
    inline vk::CommandBuffer* operator->() { return &cmd; };
  };

  struct cmdPool {
    std::vector<tempCmd> all;
    std::queue<tempCmd> submitted;
    vk::CommandPool pool;
    vk::Device dev;
    vk::Queue q;
    syncLock* qMutex;

    cmdPool(vk::CommandPool pool, vk::Device dev, vk::Queue q, syncLock* qMutex);
    ~cmdPool();

    //for threadResource
    static cmdPool* forDevice(size_t idx);
    static cmdPool* lowPrioForDevice(size_t idx);

    tempCmd allocate();
    void waitFor();

  };

};
