#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx9.lib")
#include <dinput.h>
#include <tchar.h>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <complex>
#include <iostream>
#include <condition_variable>
#include <mutex>

#include "Color.hpp"
#include "Utils.hpp"

#define RELEASE_INTERFACE(pInterface) if (pInterface) { pInterface->Release(); pInterface = nullptr; }


typedef int (*WndProcFunc)(HWND, UINT, WPARAM, LPARAM);
typedef void (*RenderLoopFunc)(void*, double);
typedef void (*SecondaryLoopFunc)(double);
enum RenderDrawType : uint32_t
{
	RenderDrawType_None = 0,
	RenderDrawType_Outlined = 1 << 0,
	RenderDrawType_Filled = 1 << 1,
	RenderDrawType_Gradient = 1 << 2,
	RenderDrawType_OutlinedGradient = RenderDrawType_Outlined | RenderDrawType_Gradient,
	RenderDrawType_FilledGradient = RenderDrawType_Filled | RenderDrawType_Gradient
};

class Image {
public:
	Image(LPDIRECT3DDEVICE9 Dx3DDevice, const char* FilePath) {
		D3DXIMAGE_INFO di{ 0 };
		if (D3DXGetImageInfoFromFile(FilePath, &di) != D3D_OK) {
			ERROR_LOG((std::string("Failed loading image info with path: ") + std::string(FilePath)).c_str());
			return;
		}

		this->Size = { (float)di.Width,(float)di.Height };
		this->OriginalSize = { (float)di.Width,(float)di.Height };

		HRESULT result = D3DXCheckTextureRequirements(Dx3DDevice, &di.Width, &di.Height, 0, 0, &di.Format, D3DPOOL_MANAGED);

		result = D3DXCreateTextureFromFileExA(Dx3DDevice, FilePath, di.Width, di.Height, D3DX_DEFAULT, 0,
			di.Format, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, nullptr, nullptr, &Texture);

		if (result != D3D_OK)
		{
			ERROR_LOG((std::string("Failed to load image for texture with path: ") + std::string(FilePath)).c_str());
			return;
		}

		D3DXCreateSprite(Dx3DDevice, &Sprite);
	}

	void SetPos(D3DXVECTOR2 pos) {
		this->Position = pos;
		this->PosVec3 = { Position.x, Position.y, 1.0f };
	}
	D3DXVECTOR2 GetPos() {
		return Position;
	}

	void SetSize(D3DXVECTOR2 size) {
		this->Size = size;
		this->Scale = { size.x / this->OriginalSize.x, size.y / this->OriginalSize.y };
	}
	D3DXVECTOR2 GetSize() {
		return Size;
	}
	
	//Recommend not using this because setsize uses auto scaling anyway
	void SetScale(D3DXVECTOR2 scale) {


		this->Scale = scale;
	}
	D3DXVECTOR2 GetScale() {
		return Scale;
	}

	void SetRotation(float angle) {
		this->Rotation = angle;
	}
	float GetRotation() {
		return Rotation;
	}

	LPDIRECT3DTEXTURE9 GetTexture() {
		return Texture;
	}
	LPD3DXSPRITE GetSprite() {
		return Sprite;
	}
	D3DXVECTOR3* GetPos3() {
		return &PosVec3;
	}

	void Update() {

		D3DXVECTOR2 SpriteCenter = D3DXVECTOR2((OriginalSize.x * Scale.x) / 2, (OriginalSize.y * Scale.y) / 2);
		D3DXVECTOR2 Trans = D3DXVECTOR2(Position.x - SpriteCenter.x, Position.y - SpriteCenter.y);
		D3DXMATRIX Matrix;
		D3DXMatrixTransformation2D(&Matrix, NULL, 0.0, &Scale, &SpriteCenter, Rotation, &Trans);
		Sprite->SetTransform(&Matrix);
	}
	D3DXVECTOR3 PosVec3;
	LPD3DXSPRITE Sprite = nullptr;
	D3DXVECTOR2 Position{ 0,0 };
	D3DXVECTOR2 Size{ 0,0 };
	D3DXVECTOR2 Scale{ 1,1 };
	D3DXVECTOR2 OriginalSize;
	float Rotation = 0;
	LPDIRECT3DTEXTURE9 Texture = nullptr;
};

class Dx3DRender {
public:
	Dx3DRender(const char* WindowName, Vec2_t WindowsSize, bool Vsync, int (*WndProcCallback)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam), void (*RenderLoopCallback)(void*, double), void (*SecondaryLoopCallback)(double));

	void StartLoops();

	LPDIRECT3DDEVICE9 GetDevice();
	Vec2_t GetResolution();
	HWND GetWindowHandle();

	bool Exit = false;
	double FramesPerSecondAvg = 0;
	int FramesToAvg = 30;
	//Actual rendering
	void OnLostDevice();
	void OnResetDevice();

	Vec2_t GetStringSize(const char* FontName, const char* Text);
	void DrawString(int16_t x, int16_t y, color_t color, const char* FontName, bool outlined, bool centered, const char* text, ...);
	void DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color_t color);
	void DrawFilledBox(int16_t x, int16_t y, int16_t width, int16_t height, color_t color);
	//use DrawBox without thickness argument if thickness == 1
	void DrawBox(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, color_t color);
	void DrawBox(int16_t x, int16_t y, int16_t width, int16_t height, color_t color);
	void DrawGradientBox(int16_t x, int16_t y, int16_t width, int16_t height, color_t color1, color_t color2, color_t color3, color_t color4);
	//use RenderDrawType_Filled for filledcircle, RenderDrawType_Gradient for gradient circle and RenderDrawType_Outlined for outlined circle
	void DrawCircle(int16_t x, int16_t y, int16_t radius, uint16_t points, RenderDrawType flags, color_t color1, color_t color2);
	void DrawCircleSector(int16_t x, int16_t y, int16_t radius, uint16_t points, uint16_t angle1, uint16_t angle2, color_t color1, color_t color2);
	void DrawRing(int16_t x, int16_t y, int16_t radius1, int16_t radius2, uint16_t points, RenderDrawType flags, color_t color1, color_t color2);
	void DrawRingSector(int16_t x, int16_t y, int16_t radius1, int16_t radius2, uint16_t points, uint16_t angle1, uint16_t angle2, color_t color1, color_t color2);
	void DrawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, RenderDrawType flags, color_t color1, color_t color2, color_t color3);
	bool LoadFont(const char* FontIdentifier, const char* szName, uint8_t iSize, uint16_t FontWeight = FW_NORMAL, uint16_t Charset = DEFAULT_CHARSET, bool Italic = false, bool AntiAliased = false);
	void DrawImage(Image* Texture);

private:
	//Internal loops
	void MainLoop();
	void SecondaryLoop();

	//Weird ass wndproc stuff
	static LRESULT WINAPI WndProcInternal(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WINAPI WndProcWrapped(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void BeginDraw();
	void EndDraw();
	inline void PushRenderState(const D3DRENDERSTATETYPE dwState, DWORD dwValue);

	bool Running = false;
	const char* WndName;
	WNDCLASSEX WndClass;
	HWND hWnd;
	MSG Msg;
	LPDIRECT3D9 Dx3D;
	LPDIRECT3DDEVICE9 Dx3DDevice = NULL;
	D3DPRESENT_PARAMETERS Dx3DParams;
	WndProcFunc WndProcCb;
	RenderLoopFunc RenderLoopCb;
	SecondaryLoopFunc SecondaryLoopCb;
	std::map<const char*, ID3DXFont*> Fonts;
	Vec2_t Resolution;
	Vec2_t Position;
	bool RenderThreadCompleted = false;
	bool SecondaryThreadCompleted = false;
	std::condition_variable ThreadController;
	std::mutex ThreadControllerMtx;
protected:
	struct SinCos_t {
		float flSin = 0.f, flCos = 0.f;
	};
	struct RenderState_t {
		D3DRENDERSTATETYPE dwState;
		DWORD dwValue;
	};

	struct Vertex_t {
		Vertex_t() { }
		Vertex_t(int _x, int _y, color_t _color) {
			x = static_cast<float>(_x);
			y = static_cast<float>(_y);
			z = 0;
			rhw = 1;
			color = _color.color;
		}
		Vertex_t(float _x, float _y, color_t _color) {
			x = _x;
			y = _y;
			z = 0;
			rhw = 1;
			color = _color.color;
		}

		float x = 0.f;
		float y = 0.f;
		float z = 0.f;
		float rhw = 0.f;
		color_t color = 0;
	};

	std::map<uint16_t, std::vector<SinCos_t>> m_SinCosContainer;
	//we dont need to calculate sin and cos every frame, we just calculate it one time
	SinCos_t* GetSinCos(uint16_t key)
	{
		if (!m_SinCosContainer.count(key))
		{
			std::vector<SinCos_t> temp_array;
			uint16_t i = 0;
			for (float angle = 0.0; angle <= 2 * D3DX_PI && i < key - 1; angle += (2 * D3DX_PI) / key)
				temp_array.push_back(SinCos_t{ sin(angle), cos(angle) });

			m_SinCosContainer.insert(std::pair<uint16_t, std::vector<SinCos_t>>(key, temp_array));
		}

		return m_SinCosContainer[key].data();
	}

	bool m_bUseDynamicSinCos = false;
	std::vector<RenderState_t> RenderStates;
};