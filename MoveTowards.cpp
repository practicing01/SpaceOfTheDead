/*
 * MoveTowards.cpp
 *
 *  Created on: Feb 4, 2015
 *      Author: practicing01
 */

#include <Urho3D/Urho3D.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Node.h>
#include "MoveTowards.h"
#include <Urho3D/IO/Log.h>

MoveTowards::MoveTowards(Context* context) :
LogicComponent(context)
{
	isMoving_ = false;
	// Only the scene update event is needed: unsubscribe from the rest for optimization
	SetUpdateEventMask(USE_UPDATE);
}

void MoveTowards::MoveToward(Vector3 direction, float speed, float speedRamp)
{
	isMoving_ = true;
	speed_ = speed;
	speedRamp_ = speedRamp;
	direction_ = direction.Normalized();
}

void MoveTowards::Stop()
{
	isMoving_ = false;
}

void MoveTowards::Update(float timeStep)
{
	if (isMoving_)
	{
		if (speedRamp_ > speed_)
		{
			speedRamp_ -= timeStep;
			if (speedRamp_ < speed_)
			{
				speedRamp_ = speed_;
			}
		}

		node_->SetPosition(node_->GetPosition() + (direction_ * (speedRamp_ * timeStep) ) );
	}
}
