//SPIRE_TEST_OPTS:-target dxbc-assembly -profile ps_5_0 -entry main

// Render target outputs (`SV_Target`) and UAVs are treated
// as sharing the same binding slots in HLSL, so we need to
// make sure that any `u` registers we allocate don't
// interfere with render targets.

#ifdef __SPIRE__
#define R(X) /**/
#else
#define R(X) X
#endif

float4 use(float  val) { return val; };
float4 use(float2 val) { return float4(val,0.0,0.0); };
float4 use(float3 val) { return float4(val,0.0); };
float4 use(float4 val) { return val; };
float4 use(Texture2D t, SamplerState s) { return t.Sample(s, 0.0); }

struct Foo { float2 v; };

// This should be allocated a register *after* the render target
RWStructuredBuffer<Foo> fooBuffer R(: register(u1));

float4 main() : SV_Target
{
	return use(fooBuffer[12].v);
}