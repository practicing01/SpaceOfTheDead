/*
 * Client.h
 *
 *  Created on: Feb 24, 2015
 *      Author: practicing01
 */

#pragma once

#include <Urho3D/Urho3D.h>

#include <Urho3D/Core/Object.h>
#include "Urho3DPlayer.h"
#include <Urho3D/UI/Text.h>

#include "NetPulse.h"

using namespace Urho3D;

class Client : public Object
{
	OBJECT(Client);
public:
	Client(Context* context, Urho3DPlayer* main, String ipAddress);
	~Client();

	void HandleUpdate(StringHash eventType, VariantMap& eventData);
	void HandleServerConnect(StringHash eventType, VariantMap& eventData);
	void HandleServerDisconnect(StringHash eventType, VariantMap& eventData);
    void HandleNetworkMessage(StringHash eventType, VariantMap& eventData);
	void TouchDown(StringHash eventType, VariantMap& eventData);
	void TouchUp(StringHash eventType, VariantMap& eventData);
	void TouchDrag(StringHash eventType, VariantMap& eventData);
    void HandleMoveToComplete(StringHash eventType, VariantMap& eventData);
	void HandleNodeCollisionStart(StringHash eventType, VariantMap& eventData);
	void HandleNodeCollision(StringHash eventType, VariantMap& eventData);
	void HandleNodeCollisionEnd(StringHash eventType, VariantMap& eventData);
    void SpawnZombie();
    void MoveZombies(float timeStep);
    int CalculateAnimDir(const Vector3& start, const Vector3& end);
	void UpdateScore();

	Urho3DPlayer* main_;
	float elapsedTime_;
	float mechSpeed_;
	float zSpawnInterval_;
	float zMoveRandyDeviation_;

	Vector3 vectoria_;
	Vector3 victoria_;

	SharedPtr<Scene> scene_;
    SharedPtr<Node> cameraNode_;
	SharedPtr<Node> netPulseDummy_;
    SharedPtr<Node> ravenMech_;
    SharedPtr<Node> mechSelf_;
    SharedPtr<Node> zSpawns_;
    SharedPtr<Node> patientZero_;
	SharedPtr<Text> text_;
	VectorBuffer msg_;
	int score_;
	int topScore_;
	int health_;

	int clientIDSelf_;

	typedef struct
	{
		int ClientID_;
		SharedPtr<Node> mech_;
	}Player;

	Vector<Player*> players_;

	typedef struct
	{
		Node* zombie_;
		PODVector<Vector3> currentPath_;
		float speed_;
	}Zombie;

	Vector<Zombie*> zombies_;
	Vector<Zombie*> spawnedZombies_;
};
