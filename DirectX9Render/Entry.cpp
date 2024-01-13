#define DIRECTINPUT_VERSION 0x0800
#include "Renderer.hpp"
#include "Utils.hpp"


//This is the function callback windows uses for window events like clicking the X moving resizing etc
int WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}


void RenderLoop(void* RenderPtr, double DeltaTime) {//Any rendering has to be done in here dont try to render in the other thread, watch out for thread safety otherwise you may casue undefined behavior
	Dx3DRender* Render = reinterpret_cast<Dx3DRender*>(RenderPtr);

	Render->DrawString(20, 20, color_t(255, 255, 255), "Segoe", true, false, std::to_string(Render->FramesPerSecondAvg).c_str( ));
	Render->DrawCircle(500, 500, 300, 100, RenderDrawType_Filled, color_t(55, 55, 55), color_t(55, 55, 55));
}

void SecondaryLoop(double DeltaTime) {//Could be used for physics or something

}



int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	Utils::CreateConsole("Debug console");

	Dx3DRender* Render = new Dx3DRender("DirectX9", { 1000, 1000 }, false, WndProc, RenderLoop, SecondaryLoop);
	Render->LoadFont("Segoe", "Segoe UI", 22, FW_BOLD, DEFAULT_CHARSET, false, true);
	Render->StartLoops( );

	return 0;
}