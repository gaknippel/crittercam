////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Dolly camera path: keyframe nodes, played back as a smooth curve.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include <vector>

namespace IGCS
{
	class CameraPath
	{
	public:
		CameraPath();

		void addNode(const DirectX::XMFLOAT3& position, DirectX::FXMVECTOR rotationQuaternion, float fov);
		bool deleteLastNode();
		void clear();
		int nodeCount() const { return static_cast<int>(_nodes.size()); }

		// Starts playback from the first node. Needs at least 2 nodes.
		bool startPlayback();
		void stopPlayback() { _playing = false; }
		bool isPlaying() const { return _playing; }

		// Calculates the camera pose for the current time. Returns false when (non-looping) playback just finished,
		// in which case the pose of the last node is returned.
		bool evaluate(DirectX::XMFLOAT3& position, DirectX::XMVECTOR& rotationQuaternion, float& fov);

	private:
		struct Node
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 rotation;		// quaternion
			float fov;
		};

		const Node& nodeAt(int index, bool loop) const;
		double secondsSinceStart();

		std::vector<Node> _nodes;
		bool _playing;
		bool _startPending;					// start the clock on the first evaluate, so key handling delays don't eat into the path
		LARGE_INTEGER _startTime;
		LARGE_INTEGER _frequency;
	};
}
