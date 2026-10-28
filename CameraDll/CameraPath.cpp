////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Dolly camera path: keyframe nodes, played back as a smooth curve.
//
// Position and FoV: Catmull-Rom spline through the nodes (the curve passes through every node).
// Rotation: quaternion squad, the rotation equivalent of a smooth spline, so there's no 'kink' at the nodes.
// Timing: every segment (node to node) takes the same time, set in the client. Non-looping paths ease in and out.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "CameraPath.h"
#include "Globals.h"
#include "GameConstants.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace IGCS
{
	CameraPath::CameraPath() : _playing(false), _startPending(false)
	{
		QueryPerformanceFrequency(&_frequency);
		_startTime.QuadPart = 0;
	}


	void CameraPath::addNode(const XMFLOAT3& position, FXMVECTOR rotationQuaternion, float fov)
	{
		Node node;
		node.position = position;
		XMVECTOR q = XMQuaternionNormalize(rotationQuaternion);
		if (!_nodes.empty())
		{
			// keep every quaternion in the same hemisphere as the previous one, so we always rotate the short way round.
			XMVECTOR previous = XMLoadFloat4(&_nodes.back().rotation);
			if (XMVectorGetX(XMQuaternionDot(previous, q)) < 0.0f)
			{
				q = XMVectorNegate(q);
			}
		}
		XMStoreFloat4(&node.rotation, q);
		node.fov = fov;
		_nodes.push_back(node);
	}


	bool CameraPath::deleteLastNode()
	{
		if (_nodes.empty())
		{
			return false;
		}
		_nodes.pop_back();
		if (_nodes.size() < 2)
		{
			_playing = false;
		}
		return true;
	}


	void CameraPath::clear()
	{
		_nodes.clear();
		_playing = false;
	}


	void CameraPath::shift(const XMFLOAT3& offset)
	{
		for (Node& node : _nodes)
		{
			node.position.x += offset.x;
			node.position.y += offset.y;
			node.position.z += offset.z;
		}
	}


	bool CameraPath::startPlayback()
	{
		if (_nodes.size() < 2)
		{
			return false;
		}
		_playing = true;
		_startPending = true;
		return true;
	}


	double CameraPath::secondsSinceStart()
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		if (_startPending)
		{
			_startTime = now;
			_startPending = false;
		}
		return static_cast<double>(now.QuadPart - _startTime.QuadPart) / static_cast<double>(_frequency.QuadPart);
	}


	const CameraPath::Node& CameraPath::nodeAt(int index, bool loop) const
	{
		const int count = static_cast<int>(_nodes.size());
		if (loop)
		{
			index = ((index % count) + count) % count;
		}
		else
		{
			index = std::clamp(index, 0, count - 1);
		}
		return _nodes[index];
	}


	bool CameraPath::evaluate(XMFLOAT3& position, XMVECTOR& rotationQuaternion, float& fov)
	{
		const int count = static_cast<int>(_nodes.size());
		if (count < 2)
		{
			_playing = false;
			return false;
		}
		const Settings& settings = Globals::instance().settings();
		const bool loop = settings.dollyLoop;
		const double segmentSeconds = std::clamp(settings.dollySegmentSeconds, DOLLY_SEGMENT_SECONDS_MIN, DOLLY_SEGMENT_SECONDS_MAX);
		const int segmentCount = loop ? count : count - 1;		// a loop also travels from the last node back to the first
		const double totalSeconds = segmentSeconds * segmentCount;
		double elapsed = secondsSinceStart();
		bool stillPlaying = true;

		double progress;	// 0..1 over the whole path
		if (loop)
		{
			progress = fmod(elapsed, totalSeconds) / totalSeconds;
		}
		else
		{
			if (elapsed >= totalSeconds)
			{
				elapsed = totalSeconds;
				stillPlaying = false;
				_playing = false;
			}
			progress = elapsed / totalSeconds;
			// smootherstep: starts and ends at zero speed, like a real dolly.
			progress = progress * progress * progress * (progress * (progress * 6.0 - 15.0) + 10.0);
		}

		double pathPosition = progress * segmentCount;
		int segment = (std::min)(static_cast<int>(pathPosition), segmentCount - 1);		// (std::min) dodges the windows.h min macro
		float t = static_cast<float>(pathPosition - segment);

		const Node& n0 = nodeAt(segment - 1, loop);
		const Node& n1 = nodeAt(segment, loop);
		const Node& n2 = nodeAt(segment + 1, loop);
		const Node& n3 = nodeAt(segment + 2, loop);

		XMVECTOR p = XMVectorCatmullRom(XMLoadFloat3(&n0.position), XMLoadFloat3(&n1.position), XMLoadFloat3(&n2.position), XMLoadFloat3(&n3.position), t);
		XMStoreFloat3(&position, p);

		XMVECTOR q0 = XMLoadFloat4(&n0.rotation);
		XMVECTOR q1 = XMLoadFloat4(&n1.rotation);
		XMVECTOR q2 = XMLoadFloat4(&n2.rotation);
		XMVECTOR q3 = XMLoadFloat4(&n3.rotation);
		XMVECTOR a, b, c;
		XMQuaternionSquadSetup(&a, &b, &c, q0, q1, q2, q3);
		rotationQuaternion = XMQuaternionNormalize(XMQuaternionSquad(q1, a, b, c, t));

		XMVECTOR f = XMVectorCatmullRom(XMVectorReplicate(n0.fov), XMVectorReplicate(n1.fov), XMVectorReplicate(n2.fov), XMVectorReplicate(n3.fov), t);
		fov = std::clamp(XMVectorGetX(f), FOV_MIN, FOV_MAX);
		return stillPlaying;
	}


	void CameraPath::sampleForDisplay(std::vector<XMFLOAT3>& curve, std::vector<XMFLOAT3>& nodes, std::vector<XMFLOAT3>& aims) const
	{
		const int SAMPLES_PER_SEGMENT = 24;
		const float AIM_LENGTH = 1.0f;		// meters
		curve.clear();
		nodes.clear();
		aims.clear();
		const int count = static_cast<int>(_nodes.size());
		const bool loop = Globals::instance().settings().dollyLoop;
		const int segmentCount = count < 2 ? 0 : (loop ? count : count - 1);
		for (int segment = 0; segment < segmentCount; segment++)
		{
			const XMVECTOR p0 = XMLoadFloat3(&nodeAt(segment - 1, loop).position);
			const XMVECTOR p1 = XMLoadFloat3(&nodeAt(segment, loop).position);
			const XMVECTOR p2 = XMLoadFloat3(&nodeAt(segment + 1, loop).position);
			const XMVECTOR p3 = XMLoadFloat3(&nodeAt(segment + 2, loop).position);
			for (int i = 0; i < SAMPLES_PER_SEGMENT; i++)
			{
				XMFLOAT3 point;
				XMStoreFloat3(&point, XMVectorCatmullRom(p0, p1, p2, p3, static_cast<float>(i) / SAMPLES_PER_SEGMENT));
				curve.push_back(point);
			}
		}
		if (segmentCount > 0)
		{
			curve.push_back(nodeAt(segmentCount, loop).position);
		}
		for (const Node& node : _nodes)
		{
			nodes.push_back(node.position);
			XMFLOAT3 aim;
			XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMLoadFloat4(&node.rotation));
			XMStoreFloat3(&aim, XMVectorAdd(XMLoadFloat3(&node.position), XMVectorScale(forward, AIM_LENGTH)));
			aims.push_back(aim);
		}
	}
}
