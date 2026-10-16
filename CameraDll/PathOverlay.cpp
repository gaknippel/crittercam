////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Draws the dolly path in a transparent click-through window on top of the game (needs borderless/windowed mode).
// It doesn't touch the game's renderer: the path is projected with our own camera (the one the game renders) and drawn with GDI.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "PathOverlay.h"
#include "Globals.h"
#include "GameConstants.h"
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>

using namespace DirectX;

extern "C" LPBYTE g_cameraOverrideBuffer;

namespace IGCS::PathOverlay
{
	static const COLORREF KEY_COLOR = RGB(255, 0, 255);		// transparent in the overlay, never drawn with
	static const COLORREF PATH_COLOR = RGB(255, 170, 0);
	static const COLORREF AIM_COLOR = RGB(0, 220, 255);
	static const COLORREF NODE_COLOR = RGB(255, 255, 255);
	static const float NEAR_Z = 0.05f;
	static const int NODE_RADIUS = 5;
	static const DWORD REDRAW_INTERVAL_MS = 8;

	static std::mutex s_mutex;
	static std::vector<XMFLOAT3> s_curve, s_nodes, s_aims;	// guarded by s_mutex
	static std::atomic_bool s_visible = false;

	struct View
	{
		XMVECTOR position, right, up, forward;
		float scale, centerX, centerY;
	};


	void update(const std::vector<XMFLOAT3>& curve, const std::vector<XMFLOAT3>& nodes, const std::vector<XMFLOAT3>& aims)
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_curve = curve;
		s_nodes = nodes;
		s_aims = aims;
	}


	void setVisible(bool visible)
	{
		s_visible = visible;
	}


	static XMFLOAT3 toCameraSpace(const View& view, const XMFLOAT3& worldPoint)
	{
		XMVECTOR d = XMVectorSubtract(XMLoadFloat3(&worldPoint), view.position);
		return XMFLOAT3(XMVectorGetX(XMVector3Dot(d, view.right)), XMVectorGetX(XMVector3Dot(d, view.up)), XMVectorGetX(XMVector3Dot(d, view.forward)));
	}


	static POINT project(const View& view, const XMFLOAT3& c)
	{
		// the game's fov is vertical, pixels are square: one scale for both axes. Clamped so GDI doesn't choke on huge coordinates.
		float x = std::clamp(view.centerX + c.x / c.z * view.scale, -1e6f, 1e6f);
		float y = std::clamp(view.centerY - c.y / c.z * view.scale, -1e6f, 1e6f);
		return { static_cast<LONG>(x), static_cast<LONG>(y) };
	}


	// Draws a world space line, cut off at the near plane so parts behind the camera don't wrap around the screen.
	static void drawLine(HDC dc, const View& view, const XMFLOAT3& worldA, const XMFLOAT3& worldB)
	{
		XMFLOAT3 a = toCameraSpace(view, worldA);
		XMFLOAT3 b = toCameraSpace(view, worldB);
		if (a.z < NEAR_Z && b.z < NEAR_Z)
		{
			return;
		}
		XMFLOAT3& behind = a.z < NEAR_Z ? a : b;
		const XMFLOAT3& front = a.z < NEAR_Z ? b : a;
		if (behind.z < NEAR_Z)
		{
			float t = (NEAR_Z - behind.z) / (front.z - behind.z);
			behind = XMFLOAT3(behind.x + (front.x - behind.x) * t, behind.y + (front.y - behind.y) * t, NEAR_Z);
		}
		POINT pa = project(view, a), pb = project(view, b);
		MoveToEx(dc, pa.x, pa.y, nullptr);
		LineTo(dc, pb.x, pb.y);
	}


	static void draw(HDC dc, int width, int height)
	{
		const LPBYTE camera = g_cameraOverrideBuffer;
		const float* m = reinterpret_cast<const float*>(camera + CAMERA_MATRIX_IN_STRUCT_OFFSET);
		const float fov = *reinterpret_cast<const float*>(camera + FOV_IN_STRUCT_OFFSET);
		View view;
		view.right = XMVectorSet(m[0], m[1], m[2], 0.0f);
		view.up = XMVectorSet(m[4], m[5], m[6], 0.0f);
		view.forward = XMVectorSet(m[8], m[9], m[10], 0.0f);
		view.position = XMVectorSet(m[12], m[13], m[14], 0.0f);
		view.scale = (height * 0.5f) / tanf(std::clamp(fov, FOV_MIN, FOV_MAX) * 0.5f);
		view.centerX = width * 0.5f;
		view.centerY = height * 0.5f;

		std::lock_guard<std::mutex> lock(s_mutex);
		HPEN pathPen = CreatePen(PS_SOLID, 2, PATH_COLOR);
		HPEN aimPen = CreatePen(PS_SOLID, 1, AIM_COLOR);
		HPEN nodePen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
		HBRUSH nodeBrush = CreateSolidBrush(NODE_COLOR);
		HGDIOBJ oldPen = SelectObject(dc, pathPen);
		for (size_t i = 1; i < s_curve.size(); i++)
		{
			drawLine(dc, view, s_curve[i - 1], s_curve[i]);
		}
		SelectObject(dc, aimPen);
		for (size_t i = 0; i < s_nodes.size() && i < s_aims.size(); i++)
		{
			drawLine(dc, view, s_nodes[i], s_aims[i]);
		}
		SelectObject(dc, nodePen);
		HGDIOBJ oldBrush = SelectObject(dc, nodeBrush);
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, NODE_COLOR);
		for (size_t i = 0; i < s_nodes.size(); i++)
		{
			XMFLOAT3 c = toCameraSpace(view, s_nodes[i]);
			if (c.z < NEAR_Z)
			{
				continue;
			}
			POINT p = project(view, c);
			Ellipse(dc, p.x - NODE_RADIUS, p.y - NODE_RADIUS, p.x + NODE_RADIUS + 1, p.y + NODE_RADIUS + 1);
			wchar_t label[8];
			int length = swprintf_s(label, L"%zu", i + 1);
			TextOutW(dc, p.x + NODE_RADIUS + 3, p.y - 2 * NODE_RADIUS - 2, label, length);
		}
		SelectObject(dc, oldBrush);
		SelectObject(dc, oldPen);
		DeleteObject(pathPen);
		DeleteObject(aimPen);
		DeleteObject(nodePen);
		DeleteObject(nodeBrush);
	}


	static DWORD WINAPI overlayThread(LPVOID)
	{
		const HWND game = Globals::instance().mainWindowHandle();
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW windowClass = { sizeof(windowClass) };
		windowClass.lpfnWndProc = DefWindowProcW;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = L"crittercamPathOverlay";
		RegisterClassExW(&windowClass);
		// owned by the game window: always above it and never above other apps. Transparent + layered = clicks and keys go to the game.
		HWND overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, windowClass.lpszClassName, L"",
									   WS_POPUP, 0, 0, 1, 1, game, nullptr, instance, nullptr);
		if (nullptr == overlay)
		{
			return 1;
		}
		SetLayeredWindowAttributes(overlay, KEY_COLOR, 0, LWA_COLORKEY);
		// no anti-aliased text: it would blend with the key color and leave a pink fringe.
		HFONT font = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
								 NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		HBRUSH keyBrush = CreateSolidBrush(KEY_COLOR);
		HDC memoryDC = nullptr;
		HBITMAP bitmap = nullptr;
		RECT placed = {};
		bool shown = false;

		while (Globals::instance().systemActive())
		{
			MSG message;
			while (PeekMessageW(&message, overlay, 0, 0, PM_REMOVE))
			{
				DispatchMessageW(&message);
			}
			Sleep(REDRAW_INTERVAL_MS);

			const bool show = s_visible && GetForegroundWindow() == game && !IsIconic(game);
			if (!show)
			{
				if (shown)
				{
					ShowWindow(overlay, SW_HIDE);
					shown = false;
				}
				continue;
			}
			RECT client;
			GetClientRect(game, &client);
			POINT topLeft = { 0, 0 };
			ClientToScreen(game, &topLeft);
			const int width = client.right, height = client.bottom;
			if (width <= 0 || height <= 0)
			{
				continue;
			}
			RECT target = { topLeft.x, topLeft.y, topLeft.x + width, topLeft.y + height };
			if (!EqualRect(&target, &placed) || nullptr == memoryDC)
			{
				SetWindowPos(overlay, nullptr, target.left, target.top, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
				if (nullptr != memoryDC)
				{
					DeleteObject(bitmap);
					DeleteDC(memoryDC);
				}
				HDC windowDC = GetDC(overlay);
				memoryDC = CreateCompatibleDC(windowDC);
				bitmap = CreateCompatibleBitmap(windowDC, width, height);
				ReleaseDC(overlay, windowDC);
				SelectObject(memoryDC, bitmap);
				SelectObject(memoryDC, font);
				placed = target;
			}
			if (!shown)
			{
				ShowWindow(overlay, SW_SHOWNOACTIVATE);
				shown = true;
			}
			// draw off screen, then copy in one go: no flicker.
			RECT all = { 0, 0, width, height };
			FillRect(memoryDC, &all, keyBrush);
			draw(memoryDC, width, height);
			HDC windowDC = GetDC(overlay);
			BitBlt(windowDC, 0, 0, width, height, memoryDC, 0, 0, SRCCOPY);
			ReleaseDC(overlay, windowDC);
		}
		if (nullptr != memoryDC)
		{
			DeleteObject(bitmap);
			DeleteDC(memoryDC);
		}
		DeleteObject(keyBrush);
		DeleteObject(font);
		DestroyWindow(overlay);
		return 0;
	}


	void start()
	{
		HANDLE thread = CreateThread(nullptr, 0, overlayThread, nullptr, 0, nullptr);
		if (nullptr != thread)
		{
			CloseHandle(thread);
		}
	}
}
