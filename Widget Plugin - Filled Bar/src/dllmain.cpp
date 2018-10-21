#include "LHMAPI.h"
// TODO: Remove this once args stabilize
#include "LHMPluginHeader.h"
#include "LHMSensorPlugin.h"

struct Widget2
{
	__declspec(align(16))
	struct VSConstants
	{

	};

	__declspec(align(16))
	struct PSConstants
	{
		// TODO: How do we get this? (Do the UV calculation in CPU, I think)
		v2i   res;

		v4    borderColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		float borderSize  = 20.0f;
		float borderBlur  = 20.0f;

		v4    fillColor  = { 1.0f, 1.0f, 1.0f, 0.5f };
		float fillAmount = 0.5f;
		float fillBlur   = 10.0f;

		v4    backgroundColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	//SensorRef sensorRef;
	//Mesh      mesh;
	v2i       position;
	v2i       size;

	VSConstants vsConstants;
	PSConstants psConstants;

	// TODO: Maybe string and range overrides?
	// TODO: drawFn? Widget type to function map?
};

#if false
void
DrawWidget_FilledBar(Widget& w /*, RendererState* renderer */)
{
	UNUSED(w);
	// Set Input Layout
	// Set Vertex Shader
	// Set VS constant buffer
	// Set Pixel Shader
	// Set PS constant buffer
}
#endif

//using ConstantBufferType = FilledBarConstants;

extern "C"
{
	__declspec(dllexport) void Initialize(SP_INITIALIZE_ARGS) { UNUSED(s); }
	__declspec(dllexport) void Update    (SP_UPDATE_ARGS    ) { UNUSED(s); }
	__declspec(dllexport) void Teardown  (SP_TEARDOWN_ARGS  ) { UNUSED(s); }
}
