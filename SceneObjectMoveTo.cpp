/*
 * SceneObjectMoveTo.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: practicing01
 */

#include <Urho3D/Urho3D.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/Core/Variant.h>

#include "SceneObjectMoveTo.h"

SceneObjectMoveTo::SceneObjectMoveTo(Context* context) :
		LogicComponent(context)
{
	isMoving_ = false;
	// Only the scene update event is needed: unsubscribe from the rest for optimization
	SetUpdateEventMask(USE_UPDATE);
}

void SceneObjectMoveTo::OnMoveToComplete()
{
	VariantMap vm;
	vm[SceneObjectMoveToComplete::P_NODE] = node_;
	SendEvent(E_SCENEOBJECTMOVETOCOMPLETE,vm);
}

void SceneObjectMoveTo::MoveTo(Vector3 dest, float speed, bool stopOnCompletion)
{
	moveToSpeed_ = speed;
	moveToDest_ = dest;
	moveToLoc_ = node_->GetWorldPosition();
	moveToDir_ = dest - moveToLoc_;
	moveToDir_.Normalize();
	moveToTravelTime_ = (moveToDest_ - moveToLoc_).Length() / speed;
	moveToElapsedTime_ = 0;
	moveToStopOnTime_ = stopOnCompletion;
	isMoving_ = true;
}

void SceneObjectMoveTo::Update(float timeStep)
{
	if (isMoving_ == true)
	{
		inderp_ = timeStep * moveToSpeed_;
		remainingDist_ = (node_->GetWorldPosition() - moveToDest_).Length();
		node_->SetWorldPosition(node_->GetWorldPosition().Lerp(moveToDest_, inderp_ / remainingDist_));
		moveToElapsedTime_ += timeStep;
		if (moveToElapsedTime_ >= moveToTravelTime_)
		{
			isMoving_ = false;
			if (moveToStopOnTime_ == true)
			{
			}
			OnMoveToComplete();
		}
	}
}
