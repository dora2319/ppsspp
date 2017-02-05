

#include "GPU/Directx9/helper/global.h"
#include "GPU/Directx9/helper/dx_fbo.h"
#include "thin3d/d3dx9_loader.h"
#include "Common/CommonFuncs.h"

namespace DX9 {

LPDIRECT3DDEVICE9 pD3Ddevice = NULL;
LPDIRECT3DDEVICE9EX pD3DdeviceEx = NULL;
LPDIRECT3D9 pD3D = NULL;

void DXSetViewport(float x, float y, float w, float h, float minZ, float maxZ) {
	D3DVIEWPORT9 vp;
	vp.X = (DWORD)x;
	vp.Y = (DWORD)y;
	vp.Width = (DWORD)w;
	vp.Height = (DWORD)h;
	vp.MinZ = minZ;
	vp.MaxZ = maxZ;
	pD3Ddevice->SetViewport(&vp);
}

static const char * vscode =
  "struct VS_IN {\n"
  "  float4 ObjPos   : POSITION;\n"
  "  float2 Uv    : TEXCOORD0;\n"
  "};"
  "struct VS_OUT {\n"
  "  float4 ProjPos  : POSITION;\n"
  "  float2 Uv    : TEXCOORD0;\n"
  "};\n"
  "VS_OUT main( VS_IN In ) {\n"
  "  VS_OUT Out;\n"
  "  Out.ProjPos = In.ObjPos;\n"
  "  Out.Uv = In.Uv;\n"
  "  return Out;\n"
  "}\n";

//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
static const char * pscode =
  "sampler s: register(s0);\n"
  "struct PS_IN {\n"
  "  float2 Uv : TEXCOORD0;\n"
  "};\n"
  "float4 main( PS_IN In ) : COLOR {\n"
  "  float4 c =  tex2D(s, In.Uv);\n"
  "  return c;\n"
  "}\n";

IDirect3DVertexDeclaration9* pFramebufferVertexDecl = NULL;

static const D3DVERTEXELEMENT9 VertexElements[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

LPDIRECT3DVERTEXSHADER9      pFramebufferVertexShader = NULL; // Vertex Shader
LPDIRECT3DPIXELSHADER9       pFramebufferPixelShader = NULL;  // Pixel Shader

bool CompilePixelShader(const char *code, LPDIRECT3DPIXELSHADER9 *pShader, LPD3DXCONSTANTTABLE *pShaderTable, std::string &errorMessage) {
	ID3DXBuffer *pShaderCode = nullptr;
	ID3DXBuffer *pErrorMsg = nullptr;

	// Compile pixel shader.
	HRESULT hr = dyn_D3DXCompileShader(code,
		(UINT)strlen(code),
		nullptr,
		nullptr,
		"main",
		"ps_2_0",
		0,
		&pShaderCode,
		&pErrorMsg,
		pShaderTable);

	if (pErrorMsg) {
		errorMessage = (CHAR *)pErrorMsg->GetBufferPointer();
		pErrorMsg->Release();
	} else if (FAILED(hr)) {
		errorMessage = GetStringErrorMsg(hr);
	} else {
		errorMessage = "";
	}

	if (FAILED(hr) || !pShaderCode) {
		if (pShaderCode)
			pShaderCode->Release();
		return false;
	}

	// Create pixel shader.
	pD3Ddevice->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), 
		pShader );

	pShaderCode->Release();

	return true;
}

bool CompileVertexShader(const char *code, LPDIRECT3DVERTEXSHADER9 *pShader, LPD3DXCONSTANTTABLE *pShaderTable, std::string &errorMessage) {
	ID3DXBuffer *pShaderCode = nullptr;
	ID3DXBuffer *pErrorMsg = nullptr;

	// Compile pixel shader.
	HRESULT hr = dyn_D3DXCompileShader(code,
		(UINT)strlen(code),
		nullptr,
		nullptr,
		"main",
		"vs_2_0",
		0,
		&pShaderCode,
		&pErrorMsg,
		pShaderTable);

	if (pErrorMsg) {
		errorMessage = (CHAR *)pErrorMsg->GetBufferPointer();
		pErrorMsg->Release();
	} else if (FAILED(hr)) {
		errorMessage = GetStringErrorMsg(hr);
	} else {
		errorMessage = "";
	}

	if (FAILED(hr) || !pShaderCode) {
		if (pShaderCode)
			pShaderCode->Release();
		return false;
	}

	// Create pixel shader.
	pD3Ddevice->CreateVertexShader( (DWORD*)pShaderCode->GetBufferPointer(), 
		pShader );

	pShaderCode->Release();

	return true;
}

bool CompileShaders(std::string &errorMsg) {
	if (!CompileVertexShader(vscode, &pFramebufferVertexShader, NULL, errorMsg)) {
		OutputDebugStringA(errorMsg.c_str());
		return false;
	}

	if (!CompilePixelShader(pscode, &pFramebufferPixelShader, NULL, errorMsg)) {
		OutputDebugStringA(errorMsg.c_str());
		if (pFramebufferVertexShader) {
			pFramebufferVertexShader->Release();
		}
		return false;
	}

	pD3Ddevice->CreateVertexDeclaration(VertexElements, &pFramebufferVertexDecl);
	pD3Ddevice->SetVertexDeclaration(pFramebufferVertexDecl);
	return true;
}

void DestroyShaders() {
	if (pFramebufferVertexShader) {
		pFramebufferVertexShader->Release();
		pFramebufferVertexShader = nullptr;
	}
	if (pFramebufferPixelShader) {
		pFramebufferPixelShader->Release();
		pFramebufferPixelShader = nullptr;
	}
	if (pFramebufferVertexDecl) {
		pFramebufferVertexDecl->Release();
		pFramebufferVertexDecl = nullptr;
	}
}

// Only used by Headless! TODO: Remove
void DirectxInit(HWND window) {
	pD3D = Direct3DCreate9( D3D_SDK_VERSION );

	// Set up the structure used to create the D3DDevice. Most parameters are
	// zeroed out. We set Windowed to TRUE, since we want to do D3D in a
	// window, and then set the SwapEffect to "discard", which is the most
	// efficient method of presenting the back buffer to the display.  And 
	// we request a back buffer format that matches the current desktop display 
	// format.
	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	// TODO?
	d3dpp.Windowed = TRUE;
	d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	d3dpp.MultiSampleQuality = 0;
	d3dpp.BackBufferCount = 1;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	//d3dpp.PresentationInterval = (useVsync == true)?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_IMMEDIATE;
	//d3dpp.RingBufferParameters = d3dr;

	HRESULT hr = pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp, &pD3Ddevice);
	if (hr != D3D_OK) {
		// TODO
	}

	std::string errorMessage;
	CompileShaders(errorMessage);

	fbo_init(pD3D);
}

};
