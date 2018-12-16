#ifndef LHM_RENDERER
#define LHM_RENDERER

using Mesh         = List<struct MeshData>::RefT;
using VertexShader = List<struct VertexShaderData>::RefT;
using PixelShader  = List<struct PixelShaderData>::RefT;

// TODO: Re-order structs in this file
enum struct ShaderStage
{
	Null,
	Vertex,
	Pixel,
};

struct Material
{
	Mesh         mesh;
	VertexShader vs;
	PixelShader  ps;
};

struct Vertex
{
	v3 position;
	v4 color;
	v2 uv;
};

using Index = u32;

namespace StandardMesh
{
	static const Mesh Null     = { 0 };
	static const Mesh Triangle = { 1 };
	static const Mesh Quad     = { 2 };
};

struct ConstantBufferDesc
{
	u32 size;
};

namespace StandardVertexShader
{
	static const VertexShader Null  = { 0 };
	// TODO: Rename?
	static const VertexShader Debug = { 1 };
};

namespace StandardPixelShader
{
	static const PixelShader Null  = { 0 };
	static const PixelShader Debug = { 1 };
};

#endif
