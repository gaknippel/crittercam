////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Draws the dolly path in a transparent click-through window on top of the game (needs borderless/windowed mode).
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include <vector>

namespace IGCS::PathOverlay
{
	// Starts the overlay thread. Call once the game window handle is known.
	void start();
	// World space points to draw: the curve as a polyline, the nodes, and per node a point along its view direction.
	void update(const std::vector<DirectX::XMFLOAT3>& curve, const std::vector<DirectX::XMFLOAT3>& nodes, const std::vector<DirectX::XMFLOAT3>& aims);
	void setVisible(bool visible);
}
