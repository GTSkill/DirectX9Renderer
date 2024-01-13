#pragma once
#include "Utils.hpp"


namespace Utils {
	void CreateConsole(const char* Title) {
//#ifdef _DEBUG
		AllocConsole();
		freopen_s((FILE**)stdin, ("CONIN$"), ("r"), stdin);
		freopen_s((FILE**)stdout, ("CONOUT$"), ("w"), stdout);
		SetConsoleTitleA("Debug console");
//#endif
	}

	Keystate_t GetKeyInfo(int Keycode) {
		SHORT Keystate = GetKeyState(Keycode);
		bool Toggled = Keystate & 1;
		bool Pressed = Keystate & 0x8000;
		return Keystate_t{ Keystate, Toggled, Pressed };
	}

	Vec2_t GetMousePos(HWND hWnd) {
		POINT MousePos;
		GetCursorPos(&MousePos);
		ScreenToClient(hWnd, &MousePos);
		return Vec2_t((float)MousePos.x, (float)MousePos.y);
	}

	float DegreeToRadian(float Degree) {
		return (Degree * D3DX_PI / 180.f);
	}

	float RadianToDegree(float Radian) {
		return (Radian * (180.f / D3DX_PI));
	}

	bool InRect(Vec2_t MousePos, Vec2_t Position, Vec2_t Size, bool Centered) {
		Position = Centered ? Vec2_t{ Position.x - Size.x / 2.f, Position.y - Size.y / 2.f } : Position;
		if (MousePos.x > Position.x && MousePos.y > Position.y)
			if (MousePos.x < Position.x + Size.x && MousePos.y < Position.y + Size.y)
				return true;

		return false;
	}
}