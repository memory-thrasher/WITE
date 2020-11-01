#pragma once

#define NS_PER_MS 1000000
#define BASIC_LOCK_TIMEOUT 100000
#define STATELOCK_TIMEOUT 10000
#define FILEIO_TIMEOUT 10000000 //10 seconds
#define FOREVER_TIMEOUT UINT32_MAX
#define NS_PER_SECOND (NS_PER_MS * 1000)
#define RB_SUCCESS 0
#define RB_BUFFER_UNDERFLOW 1
#define RB_BUFFER_OVERFLOW 2
#define RB_OUT_OF_BOUNDS 3
#define RB_NOMEM 4
#define RB_READ_INCOMPLETE 5
#define RB_LOCK_FAILED 6
#define RB_NOMAP 7
#define WARM_THREADS 8
#define MAX_THREADS 256
#define SIZEOF_VERTEX sizeof(WITE::Vertex)
#define MAX_RENDER_LAYERS 8
#define MAX_SUBSHADERS 4
//these must match the corrosponding vk_descriptor_type_*
#define SHADER_RESOURCE_UNIFORM 6 //uniform_buffer
#define SHADER_RESOURCE_SAMPLED_IMAGE 1 //combined_image_sampler, **each instance gets its own copy/allocation.
//and these corrospond to the vk_shader_stage_*_bit
#define SHADER_STAGE_VERT 1
#define SHADER_STAGE_FRAG 16
//VkFormat
#define DEBUG_MASK_VULKAN 1
#define DEBUG_MASK_VULKAN_VERBOSE 2
//more debug options, like timing, logging, per-frame db rebase or log dumping, allocation (vkAlloc)
