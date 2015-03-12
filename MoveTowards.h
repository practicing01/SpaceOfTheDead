/*
 * MoveTowards.h
 *
 *  Created on: Feb 4, 2015
 *      Author: practicing01
 */

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Scene/LogicComponent.h>

// All Urho3D classes reside in namespace Urho3D
using namespace Urho3D;

class MoveTowards: public LogicComponent
{
	OBJECT(MoveTowards);
public:
	MoveTowards(Context* context);
	/// Handle scene update. Called by LogicComponent base class.
	virtual void Update(float timeStep);
	void MoveToward(Vector3 direction, float speed, float speedRamp);
	void Stop();

	bool isMoving_;
	float speed_;
	float speedRamp_;
	Vector3 direction_;
};
