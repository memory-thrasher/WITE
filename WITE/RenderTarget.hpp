/*

Shader object (template class), tracks all Renderers using that shader so they can be batched to a single pipeline binding cycle per target.
Shader template includes any descriptor set definitions and push constants.
One Material to one or more Shaders

RenderTarget tracks the target state and type (dimensionality, resolution etc) and owns certain common resources.
Color attachment, depth/stencil, framebuffer?.

Material defines one or more shaders, render passes, subpasses etc that define how objects using it will look and what factors may affect them (and how those factors are passed in, but how exactly they affect the appearance is only in the shader code)
template class

Buffer
bool shaderRead, shaderWrite, texel //make these template somehow... derive from Material template args live?

Image
includes a Buffer
tracks the "current" image loadout and queues trnascoding barriers automatically

enum ShaderType { Draw, Mesh, Compute } //, eventually raytracing etc

bitmask for Buffer and Image, reused for Shader/Material templates:
Indirect
Vertex
DS_Read
DS_Sampled (sampled image or uniform texel buffer
DS_Write (Storage *)
Att_Depth
Att_Input
Att_Output
Host_Read
Host_Write
Is_Cube
 */

namespace WITE::GPU {

}

