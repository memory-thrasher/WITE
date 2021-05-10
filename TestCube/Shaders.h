#pragma once

#include "../WaffleIronTransactionalEngine/Export.h"

using namespace WITE::DFOSLG;

//Flow<Load_Once<Image>, Load_Frame<Transform<TRANS_MVP>>, Shader<Pass<DepthStencil, Vertex<?identifier for transform?>, Fragment<?id_image?>, Image>>, Present<?id?>> flow_flat;

/*
find a way to represent:
static data load <initial>
first frame
data load <frame, or more often>
clear reflection images
frame barrier
load frame data
render every object onto every other object
pass barrier
render every object
frame barrier
present


!!>> rework in progress:
  >> the flow should define an entire render cycle for a (type of) camera, and be passed to the camera
  >> flow should have elements that iterate over objects by layer
  >> a resource shows the lifecycle of a group of data.
*/

declareDFOSLGImage(texture)
declareDFOSLGImage(present)

typedef Flow<
  Load<texture>,
  SerialExecution<
    RenderPass<
      Clear<DepthStencil<3>>,//depth first, void if none (uncommon)
      Clear<present>,//output, tuple if multiple
      Shader<
        std::tuple<Transform<TRANS_MVP | TRANS_OBJ, 1000>, texture>,
        present
      >
    >,
    Present<present>
  >
> flatCam;
