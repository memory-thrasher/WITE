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
#define FORMAT_B8G8R8A8_UNORM 44
#define FORMAT_D16_UNORM 124
#define FORMAT_STD_COLOR FORMAT_B8G8R8A8_UNORM
#define FORMAT_STD_DEPTH FORMAT_D16_UNORM
//VkLayout
#define LAYOUT_TRANSFER_SRC 6
#define LAYOUT_TRANSFER_DST 7
#define LAYOUT_PREINITIALIZED 8
#define LAYOUT_UNDEFINED 0
#define LAYOUT_SHADER_RO 5
#define LAYOUT_COLOR 2
#define LAYOUT_DEPTH 3 //stencil (or resolve attachment) only
#define LAYOUT_DEPTH_PLUS_SHADER_RO 4 //ro stencil and ro image input to a shader only
//#define LAYOUT_DEPTH_DEPTH_RO 1000117000 //ro stencil where depth aspect is ro or ro image input into a shader only
//#define LAYOUT_DEPTH_STENCIL_RO 1000117001 //ro stencil where stencil aspect is ro or ro image input into a shader only
//VkAttachmentLoadOp
#define INIT_MODE_KEEP 0
#define INIT_MODE_CLEAR 1
#define INIT_MODE_IGNORE 2

//These do not corrospond directly to anything from vulkan, but are used (in conjunction with other info) to determine multiple vulkan fields
#define USAGE_ATTACH_AUTO 1 //what type is determined by format.
#define USAGE_TRANSFER 2 //both dirs
#define USAGE_SAMPLED 4
#define USAGE_LOAD 8

#define DEBUG_MASK_VULKAN 1
#define DEBUG_MASK_VULKAN_VERBOSE 2
//more debug options, like timing, logging, per-frame db rebase or log dumping, allocation (vkAlloc)
