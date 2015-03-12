/*
 * NetPulse.h
 *
 *  Created on: Mar 7, 2015
 *      Author: practicing01
 */

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>

// All Urho3D classes reside in namespace Urho3D
using namespace Urho3D;

const int MSG_PULSE = 8008135;

class NetPulse: public LogicComponent
{
	OBJECT(NetPulse);
public:
	NetPulse(Context* context);
	/// Handle scene update. Called by LogicComponent base class.
	virtual void Update(float timeStep);
    void HandleNetworkMessage(StringHash eventType, VariantMap& eventData);
    void ClearConnections();
    void RemoveConnection(Connection* conn);

	float elapsedTime_;
	float pulseInterval_;

	typedef struct
	{
		Connection* connection_;
		float lastPulseTime_;
		float lagTime_;
	}PulseConnections;

	Vector<PulseConnections*> connections_;

	Network* network_;
	Connection* serverConnection_;
	VectorBuffer msg_;
};
