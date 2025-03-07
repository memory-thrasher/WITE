/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "Tests/basicShader.frag.spv.h"
#include "Tests/basicShader.vert.spv.h"

using namespace WITE;

constexpr uint64_t gpuId = 0;

/*
prefix acronyms:
BR buffer requirements
IR image requirements
RS resource slot
RC resource consumer
RR resource reference
TL target layout
SL source layout
OL object layout
S shader
SM shader module
RP render pass
CP copy step
CL clear step
L layer
*_L *list
*_ID *id
*_S *staging

BR/IR_dataDescription
RC_consumerName_usageDescription
RS_objectName_dataDescription
RR_L_object_layout
SL/TL_objectName_layerDescription
S_shaderName
SM_L_shaderName
L/RP_id
CL/CP_dataDescription

 */

//this sample orders like objects together to save lines. Larger works would probably order elements by shader.

//cube data just to show how a general mesh would be imported, in practice a cube should have a computed mesh

constexpr vk::Format cubeMeshFormat[] = { Format::RGB32float, Format::RGB32float};

wrap_mesh(gpuId, cubeMeshFormat, cubeMesh,
          {{-1, -1, -1}, {-1, 0, 0}}, {{-1, 1, 1}, {-1, 0, 0}}, {{-1, -1, 1}, {-1, 0, 0}},
	  {{-1, 1, 1}, {-1, 0, 0}}, {{-1, -1, -1}, {-1, 0, 0}}, {{-1, 1, -1}, {-1, 0, 0}},
	  // front face
	  {{-1, -1, -1}, {0, 0, -1}}, {{1, -1, -1}, {0, 0, -1}}, {{1, 1, -1}, {0, 0, -1}},
	  {{-1, -1, -1}, {0, 0, -1}}, {{1, 1, -1}, {0, 0, -1}}, {{-1, 1, -1}, {0, 0, -1}},
	  // top face
	  {{-1, -1, -1}, {0, -1, 0}}, {{1, -1, 1}, {0, -1, 0}}, {{1, -1, -1}, {0, -1, 0}},
	  {{-1, -1, -1}, {0, -1, 0}}, {{-1, -1, 1}, {0, -1, 0}}, {{1, -1, 1}, {0, -1, 0}},
	  // bottom face
	  {{-1, 1, -1}, {0, 1, 0}}, {{1, 1, 1}, {0, 1, 0}}, {{-1, 1, 1}, {0, 1, 0}},
	  {{-1, 1, -1}, {0, 1, 0}}, {{1, 1, -1}, {0, 1, 0}}, {{1, 1, 1}, {0, 1, 0}},
	  // right face
	  {{1, 1, -1}, {1, 0, 0}}, {{1, -1, 1}, {1, 0, 0}}, {{1, 1, 1}, {1, 0, 0}},
	  {{1, -1, 1}, {1, 0, 0}}, {{1, 1, -1}, {1, 0, 0}}, {{1, -1, -1}, {1, 0, 0}},
	  // back face
	  {{-1, 1, 1}, {0, 0, 1}}, {{1, 1, 1}, {0, 0, 1}}, {{-1, -1, 1}, {0, 0, 1}},
	  {{-1, -1, 1}, {0, 0, 1}}, {{1, 1, 1}, {0, 0, 1}}, {{1, -1, 1}, {0, 0, 1}});

struct cameraData_t {
  glm::vec4 clip;//(near plane, far plane, cot(fov/2), z/aspect
};

//first, the types of resources we have (vk{Buffer/Image}CreateInfo are created from these structs)
constexpr bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr bufferRequirements BR_S_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BR_S_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));
constexpr imageRequirements IR_standardDepth = defineSimpleDepth(gpuId);
constexpr imageRequirements IR_standardColor = defineSimpleColor(gpuId);

//we'll need lists of them all later
constexpr bufferRequirements BR_L_all[] = {
  cubeMesh.bufferRequirements_v,
  BR_cameraData,
  BR_S_cameraData,
  BR_singleTransform,
  BR_S_singleTransform
};

constexpr imageRequirements IR_L_all[] = {
  IR_standardDepth,
  IR_standardColor
};

//next, each time a resource is used (consumed), we need a consumer object
//some consumers are created automatically from ids we provide, and macros exist that use the line number as the id
constexpr copyStep CP_transform = defineCopy(),
	    CP_cameraData = defineCopy(),
	    CP_L_all[] = {
	      CP_transform,
	      CP_cameraData,
	    };

constexpr uint64_t CP_ID_L_L1[] = {
  CP_transform.id,
  CP_cameraData.id,
};

constexpr resourceConsumer RC_S_simple_cameraTrans = defineUBConsumer(Vertex),
	    RC_S_simple_cameraData = defineUBConsumer(Vertex),
	    RC_S_simple_modelTrans = defineUBConsumer(Vertex);

constexpr resourceConsumer RC_L_S_simple_target[] = {
  //order here defines 'binding' layout qualifier in shader
  RC_S_simple_cameraData,
  RC_S_simple_cameraTrans,
};

constexpr resourceConsumer RC_L_S_simple_source[] = {
  //order here defines 'binding' layout qualifier in shader
  RC_S_simple_modelTrans,
  cubeMesh.resourceConsumer_v,//defined for convenience by the mesh wrapper
};

constexpr uint64_t RC_ID_RP_1_depth = __LINE__,
	    RC_ID_RP_1_color = __LINE__,
	    RC_ID_present_color = __LINE__;

//now set up the object structures (two objects: camera and cube)
constexpr objectLayout OL_camera = {
  .id = __LINE__,
  .windowConsumerId = RC_ID_present_color,//this implicitly creates a window that presents the image here by RR
}, OL_cube = {
  .id = __LINE__,
}, OL_L_all[] = {
  OL_camera,
  OL_cube,
};

//now slots, which define what resources are created for each type of object, note the reference to the buffer/image requirements
constexpr resourceSlot RS_camera_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BR_S_cameraData.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_cameraData = {
  .id = __LINE__,
  .requirementId = BR_cameraData.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = resize_trackWindow_discard,
}, RS_camera_depth = {
  .id = __LINE__,
  .requirementId = IR_standardDepth.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = resize_trackWindow_discard,
}, RS_camera_trans_staging = {
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_cube_trans_staging = {//begin cube
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_cube.id,
}, RS_cube_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_cube.id,
}, RS_cube_mesh = {
  .id = __LINE__,
  .requirementId = cubeMesh.bufferRequirements_v.id,
  .objectLayoutId = OL_cube.id,
  .external = true,
}, RS_L_all[] = {
  RS_camera_cameraData_staging,
  RS_camera_cameraData,
  RS_camera_color,
  RS_camera_depth,
  RS_camera_trans_staging,
  RS_camera_trans,
  //
  RS_cube_trans_staging,
  RS_cube_trans,
  RS_cube_mesh,
};

//now tie each slot to one or more consumers
constexpr resourceReference RR_L_camera[] = {
  { .resourceConsumerId = CP_cameraData.src, .resourceSlotId = RS_camera_cameraData_staging.id },
  { .resourceConsumerId = CP_cameraData.dst, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = RC_S_simple_cameraData.id, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_camera_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_S_simple_cameraTrans.id, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_ID_RP_1_color, .resourceSlotId = RS_camera_color.id },
  { .resourceConsumerId = RC_ID_present_color, .resourceSlotId = RS_camera_color.id },
  { .resourceConsumerId = RC_ID_RP_1_depth, .resourceSlotId = RS_camera_depth.id },
}, RR_L_cube[] = {
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_cube_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_cube_trans.id },
  { .resourceConsumerId = RC_S_simple_modelTrans.id, .resourceSlotId = RS_cube_trans.id },
  { .resourceConsumerId = cubeMesh.resourceConsumer_v.id, .resourceSlotId = RS_cube_mesh.id },
};

constexpr targetLayout TL_camera = {
  .id = __LINE__,
  .objectLayoutId = OL_camera.id,
  .resources = RR_L_camera, //pass by reference (with extra steps), so prior declaration is necessary
};

constexpr sourceLayout SL_cube = {
  .id = __LINE__,
  .objectLayoutId = OL_cube.id,
  .resources = RR_L_cube,
};

//now prep shader code
defineShaderModules(SM_L_simple,
		    //yes, size is in bytes even though code is an array of 32-bit ints. If shader code is stored in a file (as opposed to a header in this example), it will have to be trailing-whitespace-padded to 4-byte units. (it's code as text)
		    { basicShader_vert, sizeof(basicShader_vert), vk::ShaderStageFlagBits::eVertex },
		    { basicShader_frag, sizeof(basicShader_frag), vk::ShaderStageFlagBits::eFragment });

//link shader to consumers
constexpr graphicsShaderRequirements S_simple {
  .id = __LINE__,
  .modules = SM_L_simple,
  .targetProvidedResources = RC_L_S_simple_target,//target is always layout set 1
  .sourceProvidedResources = RC_L_S_simple_source,//source is always layout set 0
};

//create render pass
constexpr renderPassRequirements RP_1 {
  .id = __LINE__,
  .depth = RC_ID_RP_1_depth,
  .color = RC_ID_RP_1_color,
  .clearDepth = true,
  .clearColor = true,
  //default depth clear is to 1.0f, could be set here
  .clearColorValue = { 0.1f, 0.1f, 0.1f, 1.0f },
  .shaders = S_simple//takes a literalList of shaders, normally there'd be much more than one
};

//next a layer, which would usually have many more things. Use as few layers as possible
constexpr layerRequirements L_1 {
  .copies = CP_ID_L_L1,
  .renders = RP_1.id,
};

//finally shove it all into the onion soup
constexpr onionDescriptor od = {
  .IRS = IR_L_all,
  .BRS = BR_L_all,
  .RSS = RS_L_all,
  .RPRS = RP_1,
  .CSS = CP_L_all,
  .LRS = L_1,
  .OLS = OL_L_all,
  .TLS = TL_camera,
  .SLS = SL_cube,
  .GPUID = gpuId,
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

const float fov = 45;

//2342fps on test system
int main(int argc, const char** argv) {
  configuration::setOptions(argc, argv);
  gpu::init("Simple render test");
  winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto cubeMeshBuf = cubeMesh.spawnMeshBuffer();//note: pointer, managed by onion
  auto camera = primaryOnion->create<OL_camera.id>();
  auto cube = primaryOnion->create<OL_cube.id>();
  cube->set<RS_cube_mesh.id>(cubeMeshBuf.get());
  glm::vec3 rotAxis = glm::normalize(glm::dvec3(0, 1, 0));
  glm::mat4 cameraTrans = glm::lookAt(glm::dvec3(-15, 13, -10), glm::dvec3(0, 0, 0), glm::dvec3(0, 1, 0)),
    model = glm::mat4(1);//model: diagonal identity
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cameraData;
  cameraData.clip.x = 0.1f;
  cameraData.clip.y = 100;
  cameraData.clip.z = glm::cot(glm::radians(fov/2));
  cameraData.clip.w = cameraData.clip.z * size.y / size.x;
  for(size_t i = 0;i < 10000 && !shutdownRequested();i++) {
    winput::pollInput();
    model = glm::rotate(model, (float)glm::radians(0.01), rotAxis);
    cube->write<RS_cube_trans_staging.id>(model);
    camera->write<RS_camera_cameraData_staging.id>(cameraData);
    camera->write<RS_camera_trans_staging.id>(cameraTrans);
    primaryOnion->render();
  }
  primaryOnion->join();
  LOG("NOTE: done rendering (any validation whining after here is in cleanup)");
  //could let it die with the program but this way we can profile the cleanup
  primaryOnion->destroy(camera);
  primaryOnion->destroy(cube);
  primaryOnion.reset();
  LOG("NOTE: onion cleanup done, only globals remain");
  WITE::profiler::printProfileData();
}

