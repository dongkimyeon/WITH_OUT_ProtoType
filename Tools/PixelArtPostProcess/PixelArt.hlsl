// Pixel-art post process (Custom node body, Output Type = Float3)
// Blendable Location: Scene Color After Tonemapping (input/output are display/gamma encoded)
//
// Inputs:
//   UV               ScreenPosition.ViewportUV
//   PixelSize        screen pixels per art pixel (integer)
//   ColorSteps       levels per channel (or per luminance)
//   DitherStrength   Bayer 4x4 dither amount (0 = banding, 1 = standard)
//   Saturation       saturation before quantize (1 = original)
//   PaletteMix       0 = quantized scene colors, 1 = 4-color gradient palette
//   Pal0..Pal3       palette dark -> light (linear colors)
//   OutlineStrength  depth outline amount (0 = off)
//   OutlineThreshold relative depth gap threshold (0.05 = 5%)
//   OutlineColor     outline color (linear)
//   DummyColor/DummyDepth  only bind the scene textures (values unused)

float2 ViewSize = View.ViewSizeAndInvSize.xy;
float  Px       = max(1.0, floor(PixelSize));
float2 CellUV   = Px / ViewSize;

// ---- 1. Pixel grid: average 2x2 filtered taps per cell (less shimmer in motion)
float2 Cell     = floor(UV * ViewSize / Px);
float2 CenterUV = (Cell + 0.5) * CellUV;
float2 MaxUV    = 1.0 - 0.5 / ViewSize;

float3 C = 0;
C += SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV + CellUV * float2(-0.25, -0.25), 0, MaxUV)), 14, true).rgb;
C += SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV + CellUV * float2( 0.25, -0.25), 0, MaxUV)), 14, true).rgb;
C += SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV + CellUV * float2(-0.25,  0.25), 0, MaxUV)), 14, true).rgb;
C += SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV + CellUV * float2( 0.25,  0.25), 0, MaxUV)), 14, true).rgb;
C = saturate(C * 0.25);

float Luma = dot(C, float3(0.299, 0.587, 0.114));
C = saturate(lerp(Luma.xxx, C, Saturation));

// ---- 2. Bayer 4x4 ordered dither in cell space, so the pattern is art-pixel sized
const float Bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
uint2 B = uint2(Cell) & 3;
float T = ((Bayer[B.y * 4 + B.x] + 0.5) / 16.0 - 0.5) * DitherStrength;

float Steps = max(2.0, floor(ColorSteps)) - 1.0;

// ---- 3a. Per-channel quantize (keeps scene hues)
float3 Quant = saturate(floor(C * Steps + 0.5 + T) / Steps);

// ---- 3b. Luminance -> 4-color gradient palette
Luma = dot(C, float3(0.299, 0.587, 0.114));
float LumaQ = saturate(floor(Luma * Steps + 0.5 + T) / Steps);
float3 P0 = pow(saturate(Pal0), 1.0 / 2.2);
float3 P1 = pow(saturate(Pal1), 1.0 / 2.2);
float3 P2 = pow(saturate(Pal2), 1.0 / 2.2);
float3 P3 = pow(saturate(Pal3), 1.0 / 2.2);
float X = LumaQ * 3.0;
float3 Grad = X < 1.0 ? lerp(P0, P1, X) : (X < 2.0 ? lerp(P1, P2, X - 1.0) : lerp(P2, P3, X - 2.0));

float3 Result = lerp(Quant, Grad, saturate(PaletteMix));

// ---- 4. Depth outline: paint cells clearly closer than a neighbour (inner silhouette)
if (OutlineStrength > 0.0)
{
	float D  = SceneTextureLookup(ViewportUVToBufferUV(CenterUV), 1, false).r;
	float DL = SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV - float2(CellUV.x, 0), 0, MaxUV)), 1, false).r;
	float DR = SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV + float2(CellUV.x, 0), 0, MaxUV)), 1, false).r;
	float DU = SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV - float2(0, CellUV.y), 0, MaxUV)), 1, false).r;
	float DD = SceneTextureLookup(ViewportUVToBufferUV(clamp(CenterUV + float2(0, CellUV.y), 0, MaxUV)), 1, false).r;
	float Diff = max(max(DL, DR), max(DU, DD)) - D;
	float Edge = step(OutlineThreshold, Diff / max(D, 1.0));
	Result = lerp(Result, pow(saturate(OutlineColor), 1.0 / 2.2), Edge * saturate(OutlineStrength));
}

return Result;
