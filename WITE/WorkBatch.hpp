#pragma once

#include <vector>

#include "AdvancedSyncLock.hpp"
#include "Callback.hpp"
#include "Semaphore.hpp"
#include "Vulkan.hpp"
#include "BalancingThreadResourcePool.hpp"
#include "IterableBalancingThreadResourcePool.hpp"
#include "types.hpp"
#include "PerGpu.hpp"

namespace WITE::GPU {

  class ImageBase;
  class Queue;

  class WorkBatch {
  public:
    enum class result { done, yield };
    //acceptable forms of callback:
    typedefCB(thenCB, result, WorkBatch);
    struct renderInfo {
      static constexpr size_t MAX_RESOURCES = 4;//color + depth, x2 if VR, what more could you want?
      size_t resourceCount;
      ImageBase* resources[MAX_RESOURCES];
      vk::ImageLayout initialLayouts[MAX_RESOURCES], finalLayouts[MAX_RESOURCES];
      vk::RenderPassBeginInfo rpbi;
      vk::SubpassContents sc;
    };

  private:
    enum class state_t : uint8_t {
      recording, pending, running, done
    };

    struct Work {
      thenCB cpu;//if this contains both cpu and gpu work, do the cpu work first.
      size_t gpu = ~0;
      Queue* q;
      cmd_t cmd;
      vk::Fence* fence;//if populated, wait fence after cpu but before gpu
      Semaphore completeSem;
      Work() = default;
    };

    struct WorkBatchResources {
      state_t state = state_t::recording;
      Util::AdvancedSyncLock mutex;
      uint64_t cycle;
      std::vector<Work> steps;
      std::vector<WorkBatch> prerequisits;//, waiters;//waiters get tested and potentailly triggered when this work is done
      std::vector<vk::SemaphoreSubmitInfo> waitedSemsStaging, signalSemsStaging;
      std::vector<vk::CommandBufferSubmitInfo> cmdStaging;
      uint64_t nextStep;
      renderInfo currentRP;
      bool isRecordingRP;

      inline bool canRecord();
      Work& getCurrent();
      Work& append();
      void reset();
      bool canStart();
      void execute();//executes the current and maybe more
      void check();
      vk::CommandBuffer getCmd(QueueType qt);
      vk::CommandBuffer getCmd();
      vk::Fence useFence();
      WorkBatch&& lea();
    };

    static Collections::IterableBalancingThreadResourcePool<WorkBatchResources> allBatches;
    static void checkWorkerLoop();
    static PerGpuUP<Collections::BalancingThreadResourcePool<vk::Fence>> fences;

    WorkBatchResources* batch;
    uint64_t cycle;

    inline WorkBatchResources* get() const { return batch && cycle == batch->cycle ? batch : NULL; };
    inline WorkBatchResources& operator -> () { return *get(); };
    bool isDone(Work&);
    inline vk::CommandBuffer getCmd(QueueType qt) { return get()->getCmd(qt); };
    inline vk::CommandBuffer getCmd() { return get()->getCmd(); };
    uint32_t currentQFam();

    //begin cmd duplication helpers
    // WorkBatch& copyImage(ImageBase* src, ImageBase* dst);
    static void quickImageBarrier(vk::CommandBuffer, vk::Image, vk::ImageLayout, vk::ImageLayout);
    static WorkBatch& discardImage(vk::CommandBuffer, vk::Image, vk::ImageLayout post);
    static result presentImpl(vk::Fence fence, ImageBase*, vk::Image*, vk::SwapchainKHR, Queue*, WorkBatch);
    void presentImpl2(vk::SwapchainKHR, uint32_t);

    WorkBatch(WorkBatchResources* batch);
  public:
    WorkBatch(uint64_t gpuIdx = -1);
    WorkBatch(logicalDeviceMask_t ldm);
    WorkBatch(Queue* q);
    WorkBatch(const WorkBatch&) = default;

    static void init();

    WorkBatch then(thenCB);
    WorkBatch thenOnGpu(size_t gpuIdx, thenCB = NULL);
    WorkBatch thenOnAGpu(logicalDeviceMask_t ldm);
    WorkBatch submit();//begins execution. Some or all may be blocking/synchronous.
    void mustHappenAfter(WorkBatch& p);
    bool isDone() const;//done, null, and recording hasn't started are the same thing
    bool isSubmitted() const;//post-submission states also return true
    size_t getGpuIdx();
    Gpu& getGpu();
    // bool isWaiting() const;
    operator bool() const { return !isDone(); };
    auto operator <=>(const WorkBatch&) const = default;

    //SFINAE overload to support lambdas, including options to ignore the param or return void (mapped to done)
    template<class CB> requires requires(CB cb, WorkBatch wb) { {cb(wb)} -> std::convertible_to<result>; }
    WorkBatch then(CB cb) {
      return then(thenCB_F::make(cb));
    }
    template<class CB> requires requires(CB cb) { {cb()} -> std::convertible_to<result>; }
    WorkBatch then(CB cb) {
      return then(thenCB_F::make([cb](WorkBatch){ return cb(); }));
    }
    template<class CB> requires requires(CB cb) { {cb()} -> std::same_as<void>; }
    WorkBatch then(CB cb) {
      return then(thenCB_F::make([cb](WorkBatch){ cb(); return result::done; }));
    }
    template<class CB> requires requires(CB cb, WorkBatch wb) { {cb(wb)} -> std::same_as<void>; }
    WorkBatch then(CB cb) {
      return then(thenCB_F::make([cb](WorkBatch wb){ cb(wb); return result::done; }));
    }

    //begin vk::cmd stuff.
    WorkBatch& putImageInLayout(ImageBase*, vk::ImageLayout, QueueType,
				vk::AccessFlags2 = vk::AccessFlagBits2::eNone,
				vk::PipelineStageFlags2 = vk::PipelineStageFlagBits2::eAllCommands);
    WorkBatch& putImageInLayout(ImageBase*, vk::ImageLayout, uint32_t qFam,
				vk::AccessFlags2 = vk::AccessFlagBits2::eNone,
				vk::PipelineStageFlags2 = vk::PipelineStageFlagBits2::eAllCommands);

    //static multi-batch helpers
    static void batchDistributeAcrossLdm(size_t srcGpu, size_t cnt, ImageBase** srcs, ImageBase** stagings, void** ram);//stagings may be null or single elements may be null

    WorkBatch& pipelineBarrier2(vk::DependencyInfo* di);
    WorkBatch& beginRenderPass(const renderInfo*);
    WorkBatch& endRenderPass();
    WorkBatch& copyImage(ImageBase*, ImageBase*);//only on the current gpu like everything else, though each image can conatine multiple vk images each on a different gpu
    WorkBatch& copyImageToVk(ImageBase*, vk::Image, vk::ImageLayout);//mostly for swapchains
    // WorkBatch& copyImageToDev(Image<ISD>, size_t sgpu, size_t dgpu);
    WorkBatch& present(ImageBase*, vk::Image* swapImages, vk::SwapchainKHR swap);
    WorkBatch& bindDescriptorSets(vk::PipelineBindPoint pipelineBindPoint, vk::PipelineLayout layout,
				  uint32_t firstSet, uint32_t descriptorSetCount, const vk::DescriptorSet* pDescriptorSets,
				  uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
    WorkBatch& nextSubpass(vk::SubpassContents = vk::SubpassContents::eInline);
    WorkBatch& bindPipeline(vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline);
  };

}
