/*
 * NetPulse.cpp
 *
 *  Created on: Mar 7, 2015
 *      Author: practicing01
 */

#include <Urho3D/Urho3D.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/Scene/Node.h>

#include "NetPulse.h"

NetPulse::NetPulse(Context* context) :
LogicComponent(context)
{
	elapsedTime_ = 0.0f;
	pulseInterval_ = 1.0f;
	network_ = GetSubsystem<Network>();
	serverConnection_ = network_->GetServerConnection();
	SetUpdateEventMask(USE_UPDATE);
	SubscribeToEvent(E_NETWORKMESSAGE, HANDLER(NetPulse, HandleNetworkMessage));
}

void NetPulse::Update(float timeStep)
{
	elapsedTime_ += timeStep;

	if (elapsedTime_ >= pulseInterval_)//Pulse every x seconds.
	{
		elapsedTime_ = 0.0f;

		msg_.Clear();

		msg_.WriteFloat(GetSubsystem<Time>()->GetElapsedTime());

		if (network_->IsServerRunning())
		{
			network_->BroadcastMessage(MSG_PULSE, false, false, msg_);
		}
		else if (network_->GetServerConnection())
		{
			serverConnection_->SendMessage(MSG_PULSE, false, false, msg_);
		}
	}
}

void NetPulse::HandleNetworkMessage(StringHash eventType, VariantMap& eventData)
{
	using namespace NetworkMessage;

	int msgID = eventData[P_MESSAGEID].GetInt();

    if (msgID == MSG_PULSE)
    {
    	Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

        const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
        MemoryBuffer msg(data);
        float time = msg.ReadFloat();

        for (int x = 0; x < connections_.Size(); x++)
        {
        	PulseConnections* conn = connections_[x];
        	if (conn->connection_ == sender)
        	{
        		conn->lagTime_ = Abs( (time - conn->lastPulseTime_) - pulseInterval_ );
        		conn->lastPulseTime_ = time;
        		return;
        	}
        }

        PulseConnections* conn = new PulseConnections;
        conn->connection_ = sender;
        conn->lastPulseTime_ = time;
        conn->lagTime_ = 0;

        connections_.Push(conn);
    }
}

void NetPulse::ClearConnections()
{
	connections_.Clear();
}

void NetPulse::RemoveConnection(Connection* conn)
{
	for (int x = 0; x < connections_.Size(); x++)
	{
		if (connections_[x]->connection_ == conn)
		{
			connections_.Remove(connections_[x]);
			return;
		}
	}
}
