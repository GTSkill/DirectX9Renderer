#pragma once
#include "Renderer.hpp"


//Most of the internal stuff
Dx3DRender::Dx3DRender(const char* WindowName, Vec2_t WindowsSize, bool Vsync, int (*WndProcCallback)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam), void (*RenderLoopCallback)(void*, double), void (*SecondaryLoopCallback)(double)) {
	//Save wndproc callback so it can be used later
	this->WndProcCb = WndProcCallback;
	this->RenderLoopCb = RenderLoopCallback;
	this->SecondaryLoopCb = SecondaryLoopCallback;
	this->WndName = WindowName;
	this->Resolution = WindowsSize;
	this->Position = { 100, 100 };

	//Create window
	this->WndClass = { sizeof(WNDCLASSEX), CS_CLASSDC, this->WndProcInternal, 0L, 0L, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, WindowName, NULL };
	RegisterClassEx(&this->WndClass);                 //WS_POPUP
	this->hWnd = CreateWindow(WindowName, WindowName, NULL, 0, 0, 0, 0, NULL, NULL, this->WndClass.hInstance, NULL);

	//Set reservied userdata ptr to this class so we can access it later
	SetWindowLongPtr(this->hWnd, GWLP_USERDATA, (LONG_PTR)this);

	//Create directX shit
	if ((this->Dx3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) {
		UnregisterClass(WindowName, this->WndClass.hInstance);
		ERROR_LOG("Failed creating directX");
	}
	//Set directX shit
	ZeroMemory(&this->Dx3DParams, sizeof(this->Dx3DParams));
	Dx3DParams.Windowed = true;    // program fullscreen, not windowed
	Dx3DParams.SwapEffect = D3DSWAPEFFECT_DISCARD;    // discard old frames
	Dx3DParams.hDeviceWindow = hWnd;    // set the window to be used by Direct3D
	Dx3DParams.BackBufferFormat = D3DFMT_X8R8G8B8;    // set the back buffer format to 32-bit
	Dx3DParams.BackBufferWidth = WindowsSize.x;    // set the width of the buffer
	Dx3DParams.BackBufferHeight = WindowsSize.y;    // set the height of the buffer
	Dx3DParams.BackBufferCount = 1;
	Dx3DParams.MultiSampleQuality = 4;
    Dx3DParams.MultiSampleType = D3DMULTISAMPLE_NONMASKABLE;
	Dx3DParams.EnableAutoDepthStencil = true;
	Dx3DParams.AutoDepthStencilFormat = D3DFMT_D16;
	Dx3DParams.PresentationInterval = Vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;//;

	//Create device this does the actual rendering
	if (this->Dx3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &this->Dx3DParams, &this->Dx3DDevice) < 0) {
		this->Dx3D->Release();
		UnregisterClass(WindowName, this->WndClass.hInstance);
		ERROR_LOG("Failed creating device");
	}

	//I have no idea why I need to do this but the documentation told me to
	D3DADAPTER_IDENTIFIER9 AdapterIdentifier;
	this->Dx3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &AdapterIdentifier);

	//WinAPI window stuff
	ZeroMemory(&this->Msg, sizeof(this->Msg));
	ShowWindow(this->hWnd, SW_SHOWDEFAULT);
	UpdateWindow(this->hWnd);
	SetWindowPos(this->hWnd, NULL, this->Position.x, this->Position.y, this->Resolution.x, this->Resolution.y, 0);
}

void Dx3DRender::StartLoops() {
	std::thread SecondaryThread(&Dx3DRender::SecondaryLoop, this);
	this->MainLoop();
}

void Dx3DRender::MainLoop() {
	TimingClock DeltaTimer;
	while (this->Msg.message != WM_QUIT) {
		this->RenderThreadCompleted = false;
		if (PeekMessageA(&this->Msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&this->Msg);
			DispatchMessage(&this->Msg);
			continue;
		}

		this->Dx3DDevice->SetRenderState(D3DRS_ZENABLE, true);

		//this->Dx3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		//this->Dx3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
		this->Dx3DDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
		this->Dx3DDevice->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0); //You can edit the background color here


		if (this->Dx3DDevice->BeginScene() >= 0) {
			this->BeginDraw();

			//DeltaTime
			static int ElapsedFrames = 0;
			static double ElapsedTime = 0;
			
			double DeltaTime = DeltaTimer.Elapsed();
			DeltaTimer.Restart();

			++ElapsedFrames;
			ElapsedTime += DeltaTime;

			this->RenderLoopCb((void*)this, DeltaTime);

			if (ElapsedFrames >= this->FramesToAvg) {
				FramesPerSecondAvg = 1 / (ElapsedTime / ElapsedFrames);
				ElapsedFrames = 0;
				ElapsedTime = 0;
			}

			this->EndDraw();
			this->Dx3DDevice->EndScene();
		}
		if (this->Dx3DDevice->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST && this->Dx3DDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
			//Handle device lost and not reset kinda dont know
		}
		
		this->RenderThreadCompleted = true;
		this->ThreadController.notify_all();
	}
	if (this->Dx3DDevice)
		this->Dx3DDevice->Release();
	if (this->Dx3D)
		this->Dx3D->Release();
	DestroyWindow(this->Msg.hwnd);
	UnregisterClass(this->WndName, this->WndClass.hInstance);
}

void Dx3DRender::SecondaryLoop() {
	const unsigned int fixedDeltaTime = 1000 / 60;
	const double FloatDeltaTime = 1.f / 60.f;
	TimingClock DeltaTimer;
	static std::condition_variable condition_variable;
	static std::mutex mutex;
	while (true)
	{
		auto start = std::chrono::high_resolution_clock::now();
		// Do stuff
		this->SecondaryLoopCb(DeltaTimer.Elapsed());
		DeltaTimer.Restart();
		std::unique_lock<std::mutex> mutex_lock(mutex);
		std::this_thread::sleep_for(std::chrono::milliseconds(fixedDeltaTime) - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start));
	}
}

LRESULT WINAPI Dx3DRender::WndProcInternal(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	Dx3DRender* DxRender = reinterpret_cast<Dx3DRender*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (DxRender) return DxRender->WndProcWrapped(hWnd, msg, wParam, lParam);
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
LRESULT WINAPI Dx3DRender::WndProcWrapped(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return this->WndProcCb(hWnd, msg, wParam, lParam);
}


LPDIRECT3DDEVICE9 Dx3DRender::GetDevice() {
	return this->Dx3DDevice;
}
Vec2_t Dx3DRender::GetResolution() {
	return this->Resolution;
}
HWND Dx3DRender::GetWindowHandle() {
	return this->hWnd;
}


//Actual rendering functions
void Dx3DRender::BeginDraw() {
	PushRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
	PushRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	PushRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	PushRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	PushRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	PushRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	PushRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	PushRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

	this->Dx3DDevice->SetTexture(0, NULL);
	this->Dx3DDevice->SetPixelShader(NULL);
	this->Dx3DDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
}

void Dx3DRender::EndDraw() {
	//pop render states
	for (auto a : this->RenderStates)
		this->Dx3DDevice->SetRenderState(a.dwState, a.dwValue);
	this->RenderStates.clear();
}

void Dx3DRender::PushRenderState(const D3DRENDERSTATETYPE dwState, DWORD dwValue)
{
	DWORD dwTempValue;
	this->Dx3DDevice->GetRenderState(dwState, &dwTempValue);
	this->RenderStates.push_back({ dwState, dwTempValue });
	this->Dx3DDevice->SetRenderState(dwState, dwValue);
}

void Dx3DRender::OnLostDevice() { }
void Dx3DRender::OnResetDevice() { }

Vec2_t Dx3DRender::GetStringSize(const char* FontName, const char* text) {
	ID3DXFont* Font = this->Fonts[FontName];
	const size_t size = strlen(text);

	RECT size_rect = { 0 };
	Font->DrawTextA(NULL, text, (INT)size, &size_rect, DT_CALCRECT | DT_NOCLIP, 0);

	return Vec2_t(size_rect.right - size_rect.left, size_rect.bottom - size_rect.top);
}

void Dx3DRender::DrawString(int16_t x, int16_t y, color_t color, const char* FontName, bool outlined, bool centered, const char* text, ...) {
	ID3DXFont* Font = this->Fonts[FontName];
	va_list args;
	char buf[256];
	va_start(args, text);
	vsprintf_s(buf, text, args);
	va_end(args);

	const size_t size = strlen(buf);

	RECT rect = { x, y };

	if (centered)
	{
		RECT size_rect = { 0 };
		Font->DrawTextA(NULL, buf, (INT)size, &size_rect, DT_CALCRECT | DT_NOCLIP, 0);

		rect.left -= size_rect.right / 2;
		rect.top -= size_rect.bottom / 2;
	}

	if (outlined)
	{
		//black with alpha from "color"
		auto outline_color = static_cast<const color_t>((color.color >> 24) << 24);

		rect.top++;
		Font->DrawTextA(NULL, buf, (INT)size, &rect, DT_NOCLIP, outline_color);//x; y + 1
		rect.left++; rect.top--;
		Font->DrawTextA(NULL, buf, (INT)size, &rect, DT_NOCLIP, outline_color);//x + 1; y
		rect.left--; rect.top--;
		Font->DrawTextA(NULL, buf, (INT)size, &rect, DT_NOCLIP, outline_color);//x; y - 1
		rect.left--; rect.top++;
		Font->DrawTextA(NULL, buf, (INT)size, &rect, DT_NOCLIP, outline_color);//x - 1; y
		rect.left++;
	}

	Font->DrawTextA(NULL, buf, (INT)size, &rect, DT_NOCLIP, color);
}

void Dx3DRender::DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color_t color) {
	Vertex_t pVertex[2];
	pVertex[0] = Vertex_t(x1, y1, color);
	pVertex[1] = Vertex_t(x2, y2, color);
	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, pVertex, sizeof(Vertex_t));
}

void Dx3DRender::DrawFilledBox(int16_t x, int16_t y, int16_t width, int16_t height, color_t color) {
	Vertex_t pVertex[4];
	pVertex[0] = Vertex_t(x, y, color);
	pVertex[1] = Vertex_t(x + width, y, color);
	pVertex[2] = Vertex_t(x, y + height, color);
	pVertex[3] = Vertex_t(x + width, y + height, color);
	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pVertex, sizeof(Vertex_t));
}

void Dx3DRender::DrawBox(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, color_t color) {
	this->DrawFilledBox(x, y, width, thickness, color);
	this->DrawFilledBox(x, y, thickness, height, color);
	this->DrawFilledBox(x + width - thickness, y, thickness, height, color);
	this->DrawFilledBox(x, y + height - thickness, width, thickness, color);
}

void Dx3DRender::DrawBox(int16_t x, int16_t y, int16_t width, int16_t height, color_t color) {
	Vertex_t pVertex[5];
	pVertex[0] = Vertex_t(x, y, color);
	pVertex[1] = Vertex_t(x + width, y, color);
	pVertex[2] = Vertex_t(x + width, y + height, color);
	pVertex[3] = Vertex_t(x, y + height, color);
	pVertex[4] = pVertex[0];
	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, 4, pVertex, sizeof(Vertex_t));
}

void Dx3DRender::DrawGradientBox(int16_t x, int16_t y, int16_t width, int16_t height, color_t color1, color_t color2, color_t color3, color_t color4) {
	Vertex_t pVertex[4];
	pVertex[0] = Vertex_t(x, y, color1);
	pVertex[1] = Vertex_t(x + width, y, color2);
	pVertex[2] = Vertex_t(x, y + height, color3);
	pVertex[3] = Vertex_t(x + width, y + height, color4);
	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pVertex, sizeof(Vertex_t));
}

void Dx3DRender::DrawCircle(int16_t x, int16_t y, int16_t radius, uint16_t points, RenderDrawType flags, color_t color1, color_t color2) {
	const bool gradient = (flags & RenderDrawType_Gradient);
	const bool filled = (flags & RenderDrawType_Filled) || gradient;
	Vertex_t* verticles = new Vertex_t[points + gradient + 1];
	SinCos_t* pSinCos = m_bUseDynamicSinCos ? nullptr : GetSinCos(points);
	if (gradient)
		verticles[0] = Vertex_t(x, y, color2);

	for (uint16_t i = gradient; i < points + gradient; i++) {
		if (m_bUseDynamicSinCos) {
			const float angle = (2 * D3DX_PI) / points * (i - gradient);
			verticles[i] = Vertex_t(x + cos(angle) * radius, y + sin(angle) * radius, color1);
		}
		else {
			verticles[i] = Vertex_t(x + pSinCos[i - gradient].flCos * radius, y + pSinCos[i - gradient].flSin * radius, color1);
		}

		if (filled) {
			static const float angle = D3DX_PI / 180.f, flSin = sin(angle), flCos = cos(angle);

			verticles[i - gradient].x = static_cast<float>(x + flCos * (verticles[i - gradient].x - x) - flSin * (verticles[i - gradient].y - y));
			verticles[i - gradient].y = static_cast<float>(y + flSin * (verticles[i - gradient].x - x) + flCos * (verticles[i - gradient].y - y));
		}
	}

	verticles[points + gradient] = verticles[gradient];
	this->Dx3DDevice->DrawPrimitiveUP(
		(flags & RenderDrawType_Outlined) ? D3DPT_LINESTRIP :
		filled ? D3DPT_TRIANGLEFAN : D3DPT_POINTLIST,
		points, verticles, sizeof(Vertex_t));
	delete[] verticles;
}

void Dx3DRender::DrawCircleSector(int16_t x, int16_t y, int16_t radius, uint16_t points, uint16_t angle1, uint16_t angle2, color_t color1, color_t color2) {
	angle1 += 270;
	angle2 += 270;
	if (angle1 > angle2)
		angle2 += 360;

	Vertex_t* verticles = new Vertex_t[points + 2];
	const float stop = 2 * D3DX_PI * static_cast<float>(angle2) / 360.f;
	verticles[points + 1] = Vertex_t(x + cos(stop) * radius, y + sin(stop) * radius, color1);
	verticles[0] = Vertex_t(x, y, color2);
	float angle = 2 * D3DX_PI * static_cast<float>(angle1) / 360.f;
	const float step = ((2 * D3DX_PI * static_cast<float>(angle2) / 360.f) - angle) / points;
	for (uint16_t i = 1; i != points + 1; angle += step, i++)
		verticles[i] = Vertex_t(x + cos(angle) * radius, y + sin(angle) * radius, color1);

	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, points, verticles, sizeof(Vertex_t));
	delete[] verticles;
}

void Dx3DRender::DrawRing(int16_t x, int16_t y, int16_t radius1, int16_t radius2, uint16_t points, RenderDrawType flags, color_t color1, color_t color2)
{
	if (!(flags & RenderDrawType_Gradient))
		color2 = color1;
	if (flags & RenderDrawType_Outlined) {
		this->DrawCircle(x, y, radius1, points, RenderDrawType_Outlined, color1, 0);
		this->DrawCircle(x, y, radius2, points, RenderDrawType_Outlined, color2, 0);
		return;
	}

	constexpr uint8_t modifier = 4;
	Vertex_t* verticles = new Vertex_t[points * modifier];
	SinCos_t* pSinCos = m_bUseDynamicSinCos ? nullptr : GetSinCos(points);

	for (uint16_t i = 0; i < points; i++) {
		uint16_t it = i * modifier;
		if (m_bUseDynamicSinCos) {
			const float angle1 = (2 * D3DX_PI) / points * i;
			const float angle2 = (2 * D3DX_PI) / points * (i + 1);
			verticles[it] = Vertex_t(x + cos(angle1) * radius1, y + sin(angle1) * radius1, color1);
			verticles[it + 1] = Vertex_t(x + cos(angle1) * radius2, y + sin(angle1) * radius2, color2);
			verticles[it + 2] = Vertex_t(x + cos(angle2) * radius1, y + sin(angle2) * radius1, color1);
			verticles[it + 3] = Vertex_t(x + cos(angle2) * radius2, y + sin(angle2) * radius2, color2);
		}
		else {
			verticles[it] = Vertex_t(x + pSinCos[i].flCos * radius1, y + pSinCos[i].flSin * radius1, color1);
			verticles[it + 1] = Vertex_t(x + pSinCos[i].flCos * radius2, y + pSinCos[i].flSin * radius2, color2);
			verticles[it + 2] = Vertex_t(x + pSinCos[i + 1].flCos * radius1, y + pSinCos[i + 1].flSin * radius1, color1);
			verticles[it + 3] = Vertex_t(x + pSinCos[i + 1].flCos * radius2, y + pSinCos[i + 1].flSin * radius2, color2);
		}

		for (uint8_t a = 0; a < modifier; a++) {
			static const float angle = D3DX_PI / 180.f, flSin = sin(angle), flCos = cos(angle);
			verticles[it].x = static_cast<float>(x + flCos * (verticles[it].x - x) - flSin * (verticles[it].y - y));
			verticles[it].y = static_cast<float>(y + flSin * (verticles[it].x - x) + flCos * (verticles[it].y - y));
			++it;
		}
	}

	--points;
	verticles[points * modifier] = verticles[0];
	verticles[points * modifier + 1] = verticles[1];
	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, points * modifier, verticles, sizeof(Vertex_t));
	delete[] verticles;
}

void Dx3DRender::DrawRingSector(int16_t x, int16_t y, int16_t radius1, int16_t radius2, uint16_t points, uint16_t angle1, uint16_t angle2, color_t color1, color_t color2) {
	angle1 += 270;
	angle2 += 270;
	if (angle1 > angle2)
		angle2 += 360;
	const uint8_t modifier = 4;
	Vertex_t* verticles = new Vertex_t[points * modifier];
	const float start = 2 * D3DX_PI * angle1 / 360.f;
	const float stop = 2 * D3DX_PI * angle2 / 360.f;
	const float step = (stop - start) / points;
	SinCos_t sincos[2] = { sin(start), cos(start) };

	for (uint16_t i = 0; i < points; i++) {
		const float temp_angle = start + step * i;
		sincos[!(i % 2)] = { sin(temp_angle + step), cos(temp_angle + step) };
		const uint16_t it = i * modifier;
		verticles[it] = Vertex_t(x + sincos[0].flCos * radius1, y + sincos[0].flSin * radius1, color1);
		verticles[it + 1] = Vertex_t(x + sincos[1].flCos * radius2, y + sincos[1].flSin * radius2, color2);
		verticles[it + 2] = verticles[it];
		verticles[it + 3] = verticles[it + 1];
	}

	--points;
	verticles[0] = Vertex_t(x, y, color2);
	verticles[1] = Vertex_t(x + cos(start) * radius2, y + sin(start) * radius2, color2);
	verticles[points * modifier] = Vertex_t(x + cos(stop) * radius1 + 1, y + sin(stop) * radius1, color1);
	verticles[points * modifier + 1] = Vertex_t(x + cos(stop) * radius2 + 1, y + sin(stop) * radius2, color2);
	this->Dx3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, points * modifier, verticles, sizeof(Vertex_t));
	delete[] verticles;
}

void Dx3DRender::DrawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, RenderDrawType flags, color_t color1, color_t color2, color_t color3) {
	if (!(flags & RenderDrawType_Gradient)) {
		color2 = color1;
		color3 = color1;
	}

	Vertex_t pVertex[4];
	pVertex[0] = Vertex_t(x1, y1, color1);
	pVertex[1] = Vertex_t(x2, y2, color2);
	pVertex[2] = Vertex_t(x3, y3, color3);
	pVertex[3] = pVertex[0];
	this->Dx3DDevice->DrawPrimitiveUP(
		(flags & RenderDrawType_Outlined) ? D3DPT_LINESTRIP :
		(flags & RenderDrawType_Filled) ? D3DPT_TRIANGLEFAN :
		D3DPT_POINTLIST, 3, pVertex, sizeof(Vertex_t));
}

bool Dx3DRender::LoadFont(const char* FontIdentifier, const char* FontName, uint8_t FontSize, uint16_t FontWeight, uint16_t Charset, bool Italic, bool AntiAliased) {
	ID3DXFont* Font;
	if (D3DXCreateFontA(this->Dx3DDevice, FontSize, 0,
		FontWeight, 1, BOOL(Italic), Charset, OUT_DEFAULT_PRECIS,
		AntiAliased ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
		DEFAULT_PITCH, FontName, &Font) == D3D_OK) {
		this->Fonts.insert({ FontIdentifier, Font });
		return true;
	}
	return false;
}

void Dx3DRender::DrawImage(Image* Texture) {
	
	Texture->Update();
	LPD3DXSPRITE Sprite = Texture->GetSprite();
	Sprite->Begin(D3DXSPRITE_ALPHABLEND);
	Sprite->Draw(Texture->GetTexture(), 0, 0, 0, 0xFFFFFFFF);
	Sprite->End();
}