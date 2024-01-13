#pragma once
#include <Windows.h>
#include <stdio.h>
#include <d3dx9math.h>
#include <chrono>

#define DBG_UNINITIALIZED reinterpret_cast<void*>(0xcdcdcdcdcdcdcdcd)

#ifdef _DEBUG
#define CONSOLE_LOG(x) printf("[*] %s", x)
#define ERROR_LOG(x) printf("[-] %s", x)
#else
#define CONSOLE_LOG(x) 1
#define ERROR_LOG(x) 1
#endif


struct Vec2_t {
	Vec2_t() { x = 0.f; y = 0.f; }
	Vec2_t(D3DXVECTOR2 D3DXVector) {
		x = D3DXVector.x;
		y = D3DXVector.y;
	}
	Vec2_t(float _x, float _y) {
		x = _x;
		y = _y;
	}
	float x, y;
	D3DXVECTOR2 GetDXVector() {
		return D3DXVECTOR2(x, y);
	}

	float Magnitude() {
		return sqrt(x * x + y * y);
	}

	Vec2_t operator + (const Vec2_t& v) const {
		return Vec2_t(x + v.x, y + v.y);
	}

	Vec2_t operator - (const Vec2_t& v) const {
		return Vec2_t(x - v.x, y - v.y);
	}

	Vec2_t operator * (float f) const {
		return Vec2_t(x * f, y * f);
	}

	Vec2_t operator / (float f) const {
		return Vec2_t(x / f, y / f);
	}
};

struct Vec3_t {
	float x;
	float y;
	float z;

	// constructors
	Vec3_t(float new_x, float new_y, float new_z) {
		x = new_x;
		y = new_y;
		z = new_z;
	}
};

struct TimingClock {
	std::chrono::high_resolution_clock::time_point BeginTime = std::chrono::high_resolution_clock::now();
	TimingClock() {
		BeginTime = std::chrono::high_resolution_clock::now();
	}

	double Elapsed() {
		return std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::high_resolution_clock::now() - BeginTime).count() / 1000000000.f;
	}

	void Restart() {
		BeginTime = std::chrono::high_resolution_clock::now();
	}
};


struct Keystate_t {
	SHORT Keystate;
	bool Toggled;
	bool Pressed;
};

namespace Utils {
	void CreateConsole(const char* Title);
	Keystate_t GetKeyInfo(int Keycode);
	Vec2_t GetMousePos(HWND hWnd);
	float DegreeToRadian(float Degree);
	float RadianToDegree(float Radian);
	bool InRect(Vec2_t MousePos, Vec2_t Position, Vec2_t Size, bool Centered = false);
}