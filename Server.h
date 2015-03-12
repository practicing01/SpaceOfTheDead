/*
 * Server.h
 *
 *  Created on: Feb 24, 2015
 *      Author: practicing01
 */

#pragma once

#include <Urho3D/Urho3D.h>

#include <Urho3D/Core/Object.h>
#include "Urho3DPlayer.h"
#include <Urho3D/Navigation/Navigable.h>
#include <Urho3D/Navigation/NavigationMesh.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/UI/Text.h>

#include "NetPulse.h"

using namespace Urho3D;

const int MSG_MOVE = 1337;
const int MSG_STOP = 101;
const int MSG_NEWCLIENT = 7175;
const int MSG_CLIENTDISCO = 800;
const int MSG_CLIENTIDSELF = 7357;
const int MSG_ANIM = 900913;

class Server : public Object
{
	OBJECT(Server);
public:
	Server(Context* context, Urho3DPlayer* main, bool dedicated);
	~Server();

	void HandleUpdate(StringHash eventType, VariantMap& eventData);
	void TouchDown(StringHash eventType, VariantMap& eventData);
	void TouchUp(StringHash eventType, VariantMap& eventData);
	void TouchDrag(StringHash eventType, VariantMap& eventData);
	void HandleClientDisconnect(StringHash eventType, VariantMap& eventData);
	void HandleClientConnect(StringHash eventType, VariantMap& eventData);
    void HandleNetworkMessage(StringHash eventType, VariantMap& eventData);
    void HandleMoveToComplete(StringHash eventType, VariantMap& eventData);
	void HandleNodeCollisionStart(StringHash eventType, VariantMap& eventData);
	void HandleNodeCollision(StringHash eventType, VariantMap& eventData);
	void HandleNodeCollisionEnd(StringHash eventType, VariantMap& eventData);
    void SpawnZombie();
    void MoveZombies(float timeStep);
    int CalculateAnimDir(const Vector3& start, const Vector3& end);
	void UpdateScore();

	Urho3DPlayer* main_;
	SharedPtr<Scene> scene_;
    SharedPtr<Node> cameraNode_;
    SharedPtr<Node> ravenMech_;
    SharedPtr<Node> zSpawns_;
    SharedPtr<Node> patientZero_;
	SharedPtr<Text> text_;
	Vector3 vectoria_;
	Vector3 victoria_;
	float elapsedTime_;
	float mechSpeed_;
	float zSpawnInterval_;
	float zMoveRandyDeviation_;
	bool dedServer_;
	int score_;
	int topScore_;
	int health_;

	SharedPtr<Node> netPulseDummy_;
	VectorBuffer msg_;
	Network* network_;
	int clientIDCount_;

	typedef struct
	{
		int ClientID_;
		SharedPtr<Node> mech_;
		Connection* connection_;
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

	NavigationMesh* navMesh_;
};
