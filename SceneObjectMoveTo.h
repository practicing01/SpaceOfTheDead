/*
 * SceneObjectMoveTo.h
 *
 *  Created on: Dec 9, 2014
 *      Author: practicing01
 */

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Scene/LogicComponent.h>

// All Urho3D classes reside in namespace Urho3D
using namespace Urho3D;

//static const StringHash E_SCENEOBJECTMOVETOCOMPLETE("SceneObjectMoveToComplete");

EVENT(E_SCENEOBJECTMOVETOCOMPLETE, SceneObjectMoveToComplete)
{
   PARAM(P_NODE, Node);  //node
}

class SceneObjectMoveTo: public LogicComponent
{
	OBJECT(SceneObjectMoveTo);
public:
	SceneObjectMoveTo(Context* context);
	/// Handle scene update. Called by LogicComponent base class.
	virtual void Update(float timeStep);
	void OnMoveToComplete();
	void MoveTo(Vector3 dest, float speed, bool stopOnCompletion);

	bool moveToStopOnTime_;
	bool isMoving_;
	float moveToSpeed_;
	float moveToTravelTime_;
	float moveToElapsedTime_;
	float inderp_;
	float remainingDist_;
	Vector3 moveToDest_;
	Vector3 moveToLoc_;
	Vector3 moveToDir_;

};
