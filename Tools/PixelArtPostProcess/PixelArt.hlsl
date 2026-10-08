// Pixel-art post process (Custom node body, Output Type = Float3)
// Blendable Location: Scene Color After Tonemapping (input/output are display/gamma encoded)
//
// Inputs:
//   UV                 ScreenPosition.ViewportUV
//   PixelSize          screen pixels per art pixel (integer)
//   ColorSteps         levels per channel (or per luminance)
//   DitherStrength     Bayer 4x4 dither amount (0 = banding, 1 = standard)
//   Saturation         saturation before quantize (1 = original)
//   Gamma              midtone curve before quantize (1 = original, >1 = darker, richer midtones)
//   Contrast           S-curve contrast around mid gray (1 = original)
//   Sharpen            cell-level unsharp mask against the 3x3 cell mean (0 = off)
//   PaletteMix         0 = quantized scene colors, 1 = 4-color gradient palette
//   Smoothing          0 = plain cell sample, 1 = Kuwahara over 7x7 cells (flat color areas)
//   ToneStrength       split toning amount (0 = off)
//   HazeStart/HazeDistance  depth haze: starts at HazeStart (cm), reaches ~63% at +HazeDistance
//   HazeMax            haze cap (also how much the sky is washed), 0 = off
//   HazeColor          haze color (linear)
//   ShadowTint/HighlightTint  split toning colors (linear, mid gray = neutral)
//   FixedPalette       1 = snap to the 64-color Resurrect palette (ignores ColorSteps/PaletteMix)
//   Pal0..Pal3         palette dark -> light (linear colors)
//   OutlineStrength    depth outline amount (0 = off)
//   OutlineThreshold   relative depth gap threshold (0.05 = 5%)
//   NormalEdgeStrength highlight on creases (normal change) of the nearer surface (0 = off)
//   OutlineColor       outline color (linear, unused with FixedPalette: outline darkens the cell's own hue)
//   DummyColor/DummyDepth/DummyNormal  only bind the scene textures (values unused)

float2 ViewSize = View.ViewSizeAndInvSize.xy;
float  Px       = max(1.0, floor(PixelSize));
float2 CellUV   = Px / ViewSize;

float2 Cell     = floor(UV * ViewSize / Px);
float2 CenterUV = (Cell + 0.5) * CellUV;
float2 MaxUV    = 1.0 - 0.5 / ViewSize;

// ---- 1. Cell averages (4 filtered taps each) over a 7x7 cell window, then Kuwahara:
//         the 4x4 quadrant with the lowest variance wins -> painterly flat areas, edges stay sharp
float3 S[49];
[unroll] for (int sj = 0; sj < 7; sj++)
{
	[unroll] for (int si = 0; si < 7; si++)
	{
		float2 SUV = CenterUV + CellUV * float2(si - 3, sj - 3);
		float3 Sum = 0;
		Sum += SceneTextureLookup(ViewportUVToBufferUV(clamp(SUV + CellUV * float2(-0.25, -0.25), 0, MaxUV)), 14, true).rgb;
		Sum += SceneTextureLookup(ViewportUVToBufferUV(clamp(SUV + CellUV * float2( 0.25, -0.25), 0, MaxUV)), 14, true).rgb;
		Sum += SceneTextureLookup(ViewportUVToBufferUV(clamp(SUV + CellUV * float2(-0.25,  0.25), 0, MaxUV)), 14, true).rgb;
		Sum += SceneTextureLookup(ViewportUVToBufferUV(clamp(SUV + CellUV * float2( 0.25,  0.25), 0, MaxUV)), 14, true).rgb;
		S[sj * 7 + si] = Sum * 0.25;
	}
}

float3 Flat    = S[24];
float  BestVar = 1e9;
[unroll] for (int q = 0; q < 4; q++)
{
	int ox = (q & 1) * 3;
	int oy = (q >> 1) * 3;
	float3 M = 0, M2 = 0;
	[unroll] for (int qy = 0; qy < 4; qy++)
	{
		[unroll] for (int qx = 0; qx < 4; qx++)
		{
			float3 s = S[(oy + qy) * 7 + ox + qx];
			M += s;
			M2 += s * s;
		}
	}
	M /= 16.0;
	float3 V = M2 / 16.0 - M * M;
	float Var = V.r + V.g + V.b;
	if (Var < BestVar) { BestVar = Var; Flat = M; }
}
float3 C = lerp(S[24], Flat, saturate(Smoothing));

// cell-level unsharp mask: push the cell away from its 3x3 neighbourhood mean (keeps small details readable)
float3 Mean3 = 0;
[unroll] for (int mj = 2; mj <= 4; mj++)
{
	[unroll] for (int mi = 2; mi <= 4; mi++)
		Mean3 += S[mj * 7 + mi];
}
Mean3 /= 9.0;
C = saturate(C + (S[24] - Mean3) * Sharpen);

float Luma = dot(C, float3(0.299, 0.587, 0.114));
C = saturate(lerp(Luma.xxx, C, Saturation));
C = pow(C, max(Gamma, 0.01));
C = saturate((C - 0.5) * Contrast + 0.5);

// split toning: tint shadows and highlights (cinematic grade)
float3 Tone = lerp(pow(saturate(ShadowTint), 1.0 / 2.2), pow(saturate(HighlightTint), 1.0 / 2.2), smoothstep(0.0, 1.0, Luma));
C = saturate(lerp(C, C * Tone * 2.0, saturate(ToneStrength)));

// depth haze: distant cells fade into the haze color (atmospheric perspective)
float HazeD = SceneTextureLookup(ViewportUVToBufferUV(CenterUV), 1, false).r;
float Haze  = (1.0 - exp(-max(HazeD - HazeStart, 0.0) / max(HazeDistance, 1.0))) * saturate(HazeMax);
C = lerp(C, pow(saturate(HazeColor), 1.0 / 2.2), Haze);

// ---- 2. Edges on the cell grid: depth silhouette (outline) and normal crease (highlight)
float DepthEdge = 0;
float NormalEdge = 0;
if (OutlineStrength > 0.0 || NormalEdgeStrength > 0.0)
{
	float2 Nb[4] = { float2(-1, 0), float2(1, 0), float2(0, -1), float2(0, 1) };
	float  D = SceneTextureLookup(ViewportUVToBufferUV(CenterUV), 1, false).r;
	float3 N = SceneTextureLookup(ViewportUVToBufferUV(CenterUV), 8, false).rgb;
	float  MaxGap = 0;
	float  MinDot = 1;
	[unroll] for (int k = 0; k < 4; k++)
	{
		float2 NUV = ViewportUVToBufferUV(clamp(CenterUV + Nb[k] * CellUV, 0, MaxUV));
		float  Dn  = SceneTextureLookup(NUV, 1, false).r;
		float3 Nn  = SceneTextureLookup(NUV, 8, false).rgb;
		MaxGap = max(MaxGap, Dn - D);
		// crease only where the neighbour is the same surface (no depth jump) and not in front of us
		if (abs(Dn - D) / max(D, 1.0) < OutlineThreshold && Dn >= D - 1.0)
			MinDot = min(MinDot, dot(N, Nn));
	}
	DepthEdge  = step(OutlineThreshold, MaxGap / max(D, 1.0)) * saturate(OutlineStrength);
	NormalEdge = (1.0 - DepthEdge) * step(MinDot, 0.75) * saturate(NormalEdgeStrength);
}

float3 Result;
if (FixedPalette > 0.5)
{
	// ---- 3. Fixed palette: shade edges with the cell's own hue, then nearest color in OKLab
	C = lerp(C, C * 0.35, DepthEdge);
	C = saturate(lerp(C, C * 1.35 + 0.05, NormalEdge));

	float3 Lin = pow(C, 2.2);
	float3 lms = float3(
		dot(Lin, float3(0.4122214708, 0.5363325363, 0.0514459929)),
		dot(Lin, float3(0.2119034982, 0.6806995451, 0.1073969566)),
		dot(Lin, float3(0.0883024619, 0.2817188376, 0.6299787005)));
	lms = pow(max(lms, 1e-6), 1.0 / 3.0);
	float3 Lab = float3(
		dot(lms, float3(0.2104542553,  0.7936177850, -0.0040720468)),
		dot(lms, float3(1.9779984951, -2.4285922050,  0.4505937099)),
		dot(lms, float3(0.0259040371,  0.7827717662, -0.8086757660)));

	// Resurrect 64 (sRGB) and its OKLab coordinates
	const float3 PalRGB[64] = {
		float3(0.180, 0.133, 0.184), float3(0.243, 0.208, 0.275), float3(0.384, 0.333, 0.396), float3(0.588, 0.424, 0.424),
		float3(0.671, 0.580, 0.478), float3(0.412, 0.310, 0.384), float3(0.498, 0.439, 0.541), float3(0.608, 0.671, 0.698),
		float3(0.780, 0.863, 0.816), float3(1.000, 1.000, 1.000), float3(0.431, 0.153, 0.153), float3(0.702, 0.220, 0.192),
		float3(0.918, 0.310, 0.212), float3(0.961, 0.490, 0.290), float3(0.682, 0.137, 0.204), float3(0.910, 0.231, 0.231),
		float3(0.984, 0.420, 0.114), float3(0.969, 0.588, 0.090), float3(0.976, 0.761, 0.169), float3(0.478, 0.188, 0.271),
		float3(0.620, 0.271, 0.224), float3(0.804, 0.408, 0.239), float3(0.902, 0.565, 0.306), float3(0.984, 0.725, 0.329),
		float3(0.298, 0.243, 0.141), float3(0.404, 0.400, 0.200), float3(0.635, 0.663, 0.278), float3(0.835, 0.878, 0.294),
		float3(0.984, 1.000, 0.525), float3(0.086, 0.353, 0.298), float3(0.137, 0.565, 0.388), float3(0.118, 0.737, 0.451),
		float3(0.569, 0.859, 0.412), float3(0.804, 0.875, 0.424), float3(0.192, 0.212, 0.220), float3(0.216, 0.306, 0.290),
		float3(0.329, 0.494, 0.392), float3(0.573, 0.663, 0.518), float3(0.698, 0.729, 0.565), float3(0.043, 0.369, 0.396),
		float3(0.043, 0.541, 0.561), float3(0.055, 0.686, 0.608), float3(0.188, 0.882, 0.725), float3(0.561, 0.973, 0.886),
		float3(0.196, 0.200, 0.325), float3(0.282, 0.290, 0.467), float3(0.302, 0.396, 0.706), float3(0.302, 0.608, 0.902),
		float3(0.561, 0.827, 1.000), float3(0.271, 0.161, 0.247), float3(0.420, 0.243, 0.459), float3(0.565, 0.369, 0.663),
		float3(0.659, 0.518, 0.953), float3(0.918, 0.678, 0.929), float3(0.459, 0.235, 0.329), float3(0.635, 0.294, 0.435),
		float3(0.812, 0.396, 0.498), float3(0.929, 0.502, 0.600), float3(0.514, 0.110, 0.365), float3(0.765, 0.141, 0.329),
		float3(0.941, 0.310, 0.471), float3(0.965, 0.506, 0.506), float3(0.988, 0.655, 0.565), float3(0.992, 0.796, 0.690) };
	const float3 PalLab[64] = {
		float3(0.2716, 0.0235, -0.0172), float3(0.3454, 0.0197, -0.0245), float3(0.4680, 0.0231, -0.0194), float3(0.5740, 0.0510, 0.0174),
		float3(0.6803, 0.0154, 0.0432), float3(0.4615, 0.0412, -0.0181), float3(0.5682, 0.0282, -0.0326), float3(0.7309, -0.0144, -0.0148),
		float3(0.8754, -0.0262, 0.0091), float3(1.0000, 0.0000, 0.0000), float3(0.3784, 0.0932, 0.0400), float3(0.5207, 0.1424, 0.0738),
		float3(0.6382, 0.1661, 0.1029), float3(0.7177, 0.1187, 0.1091), float3(0.4925, 0.1625, 0.0608), float3(0.6158, 0.1885, 0.0915),
		float3(0.6971, 0.1402, 0.1335), float3(0.7574, 0.0716, 0.1496), float3(0.8408, 0.0102, 0.1620), float3(0.4193, 0.1047, 0.0093),
		float3(0.5046, 0.1054, 0.0602), float3(0.6284, 0.1032, 0.0951), float3(0.7299, 0.0736, 0.1100), float3(0.8287, 0.0355, 0.1341),
		float3(0.3719, 0.0061, 0.0441), float3(0.4990, -0.0219, 0.0684), float3(0.7092, -0.0475, 0.1125), float3(0.8716, -0.0668, 0.1538),
		float3(0.9726, -0.0505, 0.1351), float3(0.4221, -0.0700, 0.0051), float3(0.5821, -0.1119, 0.0393), float3(0.7021, -0.1490, 0.0657),
		float3(0.8178, -0.1185, 0.1154), float3(0.8666, -0.0640, 0.1265), float3(0.3289, -0.0056, -0.0053), float3(0.4036, -0.0291, -0.0015),
		float3(0.5544, -0.0568, 0.0246), float3(0.7070, -0.0404, 0.0422), float3(0.7717, -0.0259, 0.0519), float3(0.4408, -0.0657, -0.0294),
		float3(0.5763, -0.0907, -0.0319), float3(0.6768, -0.1204, -0.0014), float3(0.8153, -0.1467, 0.0186), float3(0.9093, -0.1033, 0.0022),
		float3(0.3356, 0.0112, -0.0548), float3(0.4280, 0.0139, -0.0728), float3(0.5269, -0.0022, -0.1274), float3(0.6736, -0.0470, -0.1273),
		float3(0.8374, -0.0493, -0.0781), float3(0.3246, 0.0490, -0.0229), float3(0.4404, 0.0780, -0.0652), float3(0.5657, 0.0856, -0.0897),
		float3(0.6940, 0.0729, -0.1433), float3(0.8247, 0.0897, -0.0629), float3(0.4356, 0.0846, -0.0080), float3(0.5334, 0.1215, -0.0085),
		float3(0.6398, 0.1352, 0.0147), float3(0.7270, 0.1348, 0.0151), float3(0.4211, 0.1479, -0.0341), float3(0.5379, 0.1902, 0.0335),
		float3(0.6592, 0.1951, 0.0314), float3(0.7342, 0.1337, 0.0518), float3(0.8064, 0.0862, 0.0623), float3(0.8791, 0.0436, 0.0517) };

	float BestDist = 1e9;
	Result = PalRGB[0];
	[unroll] for (int p = 0; p < 64; p++)
	{
		float3 d = Lab - PalLab[p];
		float Dist = d.x * d.x + 2.0 * (d.y * d.y + d.z * d.z);   // favour matching hue over matching lightness
		if (Dist < BestDist) { BestDist = Dist; Result = PalRGB[p]; }
	}
}
else
{
	// ---- 3. Bayer 4x4 ordered dither in cell space, so the pattern is art-pixel sized
	const float Bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
	uint2 B = uint2(Cell) & 3;
	float T = ((Bayer[B.y * 4 + B.x] + 0.5) / 16.0 - 0.5) * DitherStrength;

	float Steps = max(2.0, floor(ColorSteps)) - 1.0;

	// Per-channel quantize (keeps scene hues)
	float3 Quant = saturate(floor(C * Steps + 0.5 + T) / Steps);

	// Luminance -> 4-color gradient palette
	Luma = dot(C, float3(0.299, 0.587, 0.114));
	float LumaQ = saturate(floor(Luma * Steps + 0.5 + T) / Steps);
	float3 P0 = pow(saturate(Pal0), 1.0 / 2.2);
	float3 P1 = pow(saturate(Pal1), 1.0 / 2.2);
	float3 P2 = pow(saturate(Pal2), 1.0 / 2.2);
	float3 P3 = pow(saturate(Pal3), 1.0 / 2.2);
	float X = LumaQ * 3.0;
	float3 Grad = X < 1.0 ? lerp(P0, P1, X) : (X < 2.0 ? lerp(P1, P2, X - 1.0) : lerp(P2, P3, X - 2.0));

	Result = lerp(Quant, Grad, saturate(PaletteMix));
	Result = lerp(Result, pow(saturate(OutlineColor), 1.0 / 2.2), DepthEdge);
	Result = saturate(Result + NormalEdge * 0.15);
}

return Result;
