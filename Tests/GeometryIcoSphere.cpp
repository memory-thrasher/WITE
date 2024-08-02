/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

//begin shared
constexpr uint64_t gpuId = 0;
constexpr WITE::bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr WITE::bufferRequirements BR_S_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr WITE::copyStep CP_transform = defineCopy();
constexpr uint64_t RC_ID_present_color = __LINE__,
	    RC_ID_RP_1_color = __LINE__;

//begin sphere
constexpr WITE::objectLayout OL_sphere = { .id = __LINE__ };

struct sphereData_t {
  glm::vec4 locr;//xyz = location, w = radius
};

constexpr WITE::bufferRequirements BR_sphereData = defineSimpleUniformBuffer(gpuId, sizeof(sphereData_t)),
	    BR_S_sphereData = NEW_ID(stagingRequirementsFor(BR_sphereData, 2));

constexpr WITE::resourceSlot  RS_sphere_sphereData = {
  .id = __LINE__,
  .requirementId = BR_sphereData.id,
  .objectLayoutId = OL_sphere.id,
}, RS_sphere_S_sphereData = {
  .id = __LINE__,
  .requirementId = BR_S_sphereData.id,
  .objectLayoutId = OL_sphere.id,
}, RS_L_sphere[] = {
  RS_sphere_sphereData,
  RS_sphere_S_sphereData,
};

//begin sphere
#include "Tests/icosahedron.vert.spv.h"
#include "Tests/slerpPolyhedron.geom.spv.h"
#include "Tests/basicShader.frag.spv.h"

constexpr WITE::resourceConsumer RC_S_sphere_sphereData = defineUBConsumer(Vertex | vk::ShaderStageFlagBits::eGeometry),
	    RC_L_S_sphere_source[] = { RC_S_sphere_sphereData };

constexpr WITE::resourceConsumer RC_S_sphere_cameraTrans = defineUBConsumer(Geometry),
	    RC_S_sphere_cameraData = defineUBConsumer(Geometry),
	    RC_L_S_sphere_target[] = { RC_S_sphere_cameraData, RC_S_sphere_cameraTrans };

constexpr WITE::shaderModule SM_L_sphere[] = {
  { icosahedron_vert, sizeof(icosahedron_vert), vk::ShaderStageFlagBits::eVertex },
  { slerpPolyhedron_geom, sizeof(slerpPolyhedron_geom), vk::ShaderStageFlagBits::eGeometry },
  { basicShader_frag, sizeof(basicShader_frag), vk::ShaderStageFlagBits::eFragment },
};

constexpr WITE::graphicsShaderRequirements S_sphere {
  .id = __LINE__,
  .modules = SM_L_sphere,
  .targetProvidedResources = RC_L_S_sphere_target,
  .sourceProvidedResources = RC_L_S_sphere_source,
  .cullMode = vk::CullModeFlagBits::eBack,
  .vertexCountOverride = 60,//20 triangles
};

constexpr WITE::copyStep CP_sphereData = defineCopy();

constexpr WITE::resourceReference RR_L_sphere[] = {
  { .resourceConsumerId = RC_S_sphere_sphereData.id, .resourceSlotId = RS_sphere_sphereData.id },
  { .resourceConsumerId = CP_sphereData.src, .resourceSlotId = RS_sphere_S_sphereData.id },
  { .resourceConsumerId = CP_sphereData.dst, .resourceSlotId = RS_sphere_sphereData.id },
};

constexpr WITE::sourceLayout SL_sphere = {
  .id = __LINE__,
  .objectLayoutId = OL_sphere.id,
  .resources = RR_L_sphere,
};

//begin camera
struct cameraData_t {
  glm::vec4 clip;//(near plane, far plane, cot(fov/2), z/aspect
};

constexpr WITE::objectLayout OL_camera = {
  .id = __LINE__,
  .windowConsumerId = RC_ID_present_color,//this implicitly creates a window that presents the image here by RR
};

constexpr WITE::bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr WITE::bufferRequirements BR_S_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));
constexpr WITE::imageRequirements IR_standardColor = defineSimpleColor(gpuId);

constexpr WITE::copyStep CP_camera_cameraData = defineCopy();

constexpr WITE::resourceSlot RS_camera_cameraData_staging = {
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
  .resizeBehavior = WITE::resize_trackWindow_discard,
}, RS_camera_trans_staging = {
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_L_camera[] = {
  RS_camera_trans,
  RS_camera_trans_staging,
  RS_camera_color,
  RS_camera_cameraData,
  RS_camera_cameraData_staging,
};

constexpr WITE::resourceReference RR_L_camera[] = {
  { .resourceConsumerId = CP_camera_cameraData.src, .resourceSlotId = RS_camera_cameraData_staging.id },
  { .resourceConsumerId = CP_camera_cameraData.dst, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_camera_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_S_sphere_cameraData.id, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = RC_S_sphere_cameraTrans.id, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_ID_RP_1_color, .resourceSlotId = RS_camera_color.id },
  { .resourceConsumerId = RC_ID_present_color, .resourceSlotId = RS_camera_color.id },
};

constexpr WITE::targetLayout TL_camera = {
  .id = __LINE__,
  .objectLayoutId = OL_camera.id,
  .resources = RR_L_camera, //pass by reference (with extra steps), so prior declaration is necessary
};

constexpr WITE::renderPassRequirements RP_1 = {
  .id = __LINE__,
  .color = RC_ID_RP_1_color,
  .clearColor = true,
  .clearColorValue = { 0, 0, 0, 0 },
  .shaders = S_sphere,
};

constexpr uint64_t CP_IDL_all[] = { CP_camera_cameraData.id, CP_sphereData.id, CP_transform.id };

constexpr WITE::layerRequirements LRS[] = {
  {
    .copies = CP_IDL_all,
    .renders = RP_1.id,
  }
};

//begin onion
constexpr WITE::bufferRequirements allBRS[] = {
  BR_S_cameraData,
  BR_cameraData,
  BR_S_sphereData,
  BR_sphereData,
  BR_S_singleTransform,
  BR_singleTransform,
};

constexpr auto RS_all = WITE::concat<WITE::resourceSlot, RS_L_sphere, RS_L_camera>();

constexpr WITE::copyStep CP_all[] = { CP_camera_cameraData, CP_sphereData, CP_transform };

constexpr WITE::objectLayout allOLS[] = { OL_sphere, OL_camera };

constexpr WITE::onionDescriptor od = {
  .IRS = IR_standardColor,
  .BRS = allBRS,
  .RSS = RS_all,
  .RPRS = RP_1,
  .CSS = CP_all,
  .LRS = LRS,
  .OLS = allOLS,
  .TLS = TL_camera,
  .SLS = SL_sphere,
  .GPUID = gpuId,
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

const float fov = 45;

int main(int argc, const char** argv) {
  WITE::configuration::setOptions(argc, argv);
  WITE::gpu::init("Input and combined procedural and traditional render test");
  WITE::winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->create<OL_camera.id>();
  cameraData_t cameraData;
  cameraData.clip.x = 0.1f;
  cameraData.clip.y = 100;
  cameraData.clip.z = glm::cot(glm::radians(fov/2));
  auto sphere1 = primaryOnion->create<OL_sphere.id>();
  sphereData_t sphereData { {1, 0, 0, 1} };
  float az = 1.5f, inc = 0.5f, dist = 12;
  WITE::winput::compositeInputData cid;
  for(size_t i = 0;i < 100000 && !WITE::shutdownRequested();i++) {
  // while(!WITE::shutdownRequested()) {
    WITE::winput::pollInput();
    if(WITE::winput::getButton(WITE::winput::rmb)) {
      WITE::winput::getInput(WITE::winput::mouse, cid);
      az += cid.axes[0].delta * 0.01f;
      inc += cid.axes[1].delta * 0.01f;
    }
    WITE::winput::getInput(WITE::winput::mouseWheel, cid);
    dist -= cid.axes[1].numPositive - cid.axes[1].numNegative;//Normal mouse wheel is y-axis. Some mice have an x-axis.
    glm::vec3 loc = (glm::vec3(glm::cos(az), 0, glm::sin(az)) * glm::cos(inc) + glm::vec3(0, glm::sin(inc), 0)) * (float)pow(1.3f, dist);
    glm::vec2 size = camera->getWindow().getVecSize();
    cameraData.clip.w = cameraData.clip.z * size.y / size.x;
    camera->write<RS_camera_trans_staging.id>(glm::lookAt(loc, glm::vec3(0), glm::vec3(0, 1, 0)));
    camera->write<RS_camera_cameraData_staging.id>(cameraData);
    sphere1->write<RS_sphere_S_sphereData.id>(sphereData);
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
}
