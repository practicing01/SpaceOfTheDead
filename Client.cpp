/*
 * Client.cpp
 *
 *  Created on: Feb 24, 2015
 *      Author: practicing01
 */

#include "Client.h"

#include <Urho3D/Urho3D.h>

#include <Urho3D/Urho2D/AnimationSet2D.h>
#include <Urho3D/Urho2D/AnimatedSprite2D.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Physics/PhysicsEvents.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Sprite.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Graphics/Viewport.h>

#include <Urho3D/DebugNew.h>

#include "MoveTowards.h"
#include "NetPulse.h"
#include "Server.h"
#include "SceneObjectMoveTo.h"

Client::Client(Context* context, Urho3DPlayer* main, String ipAddress) :
    Object(context)
{
	context->RegisterFactory<MoveTowards>();
	context->RegisterFactory<NetPulse>();
	context->RegisterFactory<SceneObjectMoveTo>();

	main_ = main;
	elapsedTime_ = 0.0f;

	main_->network_->Connect(ipAddress, 9001, 0);

	//Load scene.

	main_->cameraNode_->RemoveAllChildren();
	main_->cameraNode_->RemoveAllComponents();
	main_->cameraNode_->Remove();

	File loadFile(context_,main->filesystem_->GetProgramDir()
			+ "Data/Scenes/solcommCity.xml", FILE_READ);
	main_->scene_->LoadXML(loadFile);

	main_->cameraNode_ = main_->scene_->GetChild("RavenMech")->GetChild("camera");
	main_->viewport_->SetCamera(main_->cameraNode_->GetComponent<Camera>());

	//Load moveButt scene.

	scene_ = new Scene(main_->GetContext());
	cameraNode_ = new Node(main_->GetContext());

	File loadFile2(context_,main->filesystem_->GetProgramDir()
			+ "Data/Scenes/moveButts.xml", FILE_READ);
	scene_->LoadXML(loadFile2);

	cameraNode_ = scene_->GetChild("camera");

	ravenMech_ = main_->scene_->GetChild("RavenMech");
	ravenMech_->AddComponent(new MoveTowards(context_), 0, LOCAL);
	mechSpeed_ = 1.0f;

	SubscribeToEvent(ravenMech_->GetChild("cannon"), E_NODECOLLISION, HANDLER(Client, HandleNodeCollision));
	SubscribeToEvent(ravenMech_->GetChild("cannon"), E_NODECOLLISIONEND, HANDLER(Client, HandleNodeCollisionEnd));

	mechSelf_ = 0;

	zSpawns_ = main_->scene_->GetChild("zSpawns");

	patientZero_ = main_->scene_->GetChild("zombie");
	patientZero_->AddComponent(new SceneObjectMoveTo(context_), 0, LOCAL);

	for (int x = 0 ; x < 200; x++)
	{
		Zombie* zee = new Zombie;
		zee->zombie_ = patientZero_->Clone(LOCAL);
		zombies_.Push(zee);
		SubscribeToEvent(E_SCENEOBJECTMOVETOCOMPLETE, HANDLER(Client, HandleMoveToComplete));
	}

	zSpawnInterval_ = 1.0f;

	zMoveRandyDeviation_ = 1.0f;

	score_ = 0;
	health_ = 100;
	topScore_ = 0;

	text_ = new Text(context_);
	text_->SetFont(main_->cache_->GetResource<Font>("Fonts/Anonymous Pro.ttf"),
			12);
	text_->SetColor(Color(1.0f, 0.5f, 0.0f));
	text_->SetPosition(0, 0);
	text_->SetHorizontalAlignment(HA_LEFT);
	text_->SetVerticalAlignment(VA_TOP);
	GetSubsystem<UI>()->GetRoot()->AddChild(text_);
	UpdateScore();

	SubscribeToEvent(E_UPDATE, HANDLER(Client, HandleUpdate));
    SubscribeToEvent(E_SERVERCONNECTED, HANDLER(Client, HandleServerConnect));
    SubscribeToEvent(E_SERVERDISCONNECTED, HANDLER(Client, HandleServerDisconnect));
	SubscribeToEvent(E_NETWORKMESSAGE, HANDLER(Client, HandleNetworkMessage));
	SubscribeToEvent(E_TOUCHBEGIN, HANDLER(Client, TouchDown));
	SubscribeToEvent(E_TOUCHMOVE, HANDLER(Client, TouchDrag));
	SubscribeToEvent(E_TOUCHEND, HANDLER(Client, TouchUp));
}

Client::~Client()
{
}

void Client::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	using namespace Update;

	float timeStep = eventData[P_TIMESTEP].GetFloat();

	//LOGERRORF("client loop");
	elapsedTime_ += timeStep;

	if (elapsedTime_ >= zSpawnInterval_)
	{
		elapsedTime_ = 0.0f;
		for (int x = 0; x < 10; x++)
		{
			SpawnZombie();
		}
	}

	if (main_->input_->GetKeyDown(SDLK_ESCAPE))
	{
		main_->GetSubsystem<Engine>()->Exit();
	}
}

void Client::HandleServerConnect(StringHash eventType, VariantMap& eventData)
{
	//LOGERRORF("client connected to server");

	netPulseDummy_ = new Node(context_);
	main_->scene_->AddChild(netPulseDummy_);
	netPulseDummy_->AddComponent(new NetPulse(context_), 0, LOCAL);

	//Add server mech.
	Player* newPlayer = new Player;
	players_.Push(newPlayer);
	newPlayer->ClientID_ = 0;
	newPlayer->mech_ = ravenMech_;
}

void Client::HandleServerDisconnect(StringHash eventType, VariantMap& eventData)
{
	LOGERRORF("client diconnected from server");

	netPulseDummy_->GetComponent<NetPulse>()->ClearConnections();
}

void Client::HandleNetworkMessage(StringHash eventType, VariantMap& eventData)
{
	using namespace NetworkMessage;

	int msgID = eventData[P_MESSAGEID].GetInt();

    if (msgID == MSG_MOVE)
    {
    	Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

        const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
        MemoryBuffer msg(data);
        int clientID =  msg.ReadInt();
        vectoria_ = msg.ReadVector3();
        float lagTime = msg.ReadFloat();

        for (int x = 0; x < players_.Size(); x++)
        {
        	if (players_[x]->ClientID_ == clientID)
        	{
        		NetPulse* np = netPulseDummy_->GetComponent<NetPulse>();
        		for (int y = 0; y < np->connections_.Size(); y++)
        		{
        			if (np->connections_[y]->connection_ == sender)
        			{
        				//Add lag from this sender.
        				lagTime += np->connections_[y]->lagTime_;
        				break;
        			}
        		}

        		players_[x]->mech_->GetComponent<MoveTowards>()->MoveToward(vectoria_, mechSpeed_, mechSpeed_ + lagTime);
        		break;
        	}
        }
    }
    else if (msgID == MSG_STOP)
    {
    	//Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

    	const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
    	MemoryBuffer msg(data);
        int clientID =  msg.ReadInt();
    	vectoria_ = msg.ReadVector3();

    	for (int x = 0; x < players_.Size(); x++)
    	{
    		if (players_[x]->ClientID_ == clientID)
    		{
    			players_[x]->mech_->GetComponent<MoveTowards>()->Stop();
    			players_[x]->mech_->SetPosition(vectoria_);
    			break;
    		}
    	}
    }
    else if (msgID == MSG_NEWCLIENT)
    {
    	Player* newPlayer = new Player;
    	players_.Push(newPlayer);

    	const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
    	MemoryBuffer msg(data);
    	int clientID = msg.ReadInt();

    	newPlayer->ClientID_ = clientID;
    	newPlayer->mech_ = ravenMech_->Clone(LOCAL);

    	SubscribeToEvent(newPlayer->mech_->GetChild("cannon"), E_NODECOLLISION, HANDLER(Client, HandleNodeCollision));
    	SubscribeToEvent(newPlayer->mech_->GetChild("cannon"), E_NODECOLLISIONEND, HANDLER(Client, HandleNodeCollisionEnd));
    }
    else if (msgID == MSG_CLIENTDISCO)
    {
    	const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
    	MemoryBuffer msg(data);
    	int clientID = msg.ReadInt();

    	for (int x = 0; x < players_.Size(); x++)
    	{
    		if (players_[x]->ClientID_ == clientID)
    		{
    			players_[x]->mech_->RemoveAllChildren();
    			players_[x]->mech_->RemoveAllComponents();
    			players_[x]->mech_->Remove();
    			players_.Remove(players_[x]);
    			break;
    		}
    	}
    }
    else if (msgID == MSG_CLIENTIDSELF)
    {
    	const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
    	MemoryBuffer msg(data);
    	clientIDSelf_ = msg.ReadInt();

    	for (int x = 0; x < players_.Size(); x++)
    	{
    		if (players_[x]->ClientID_ == clientIDSelf_)
    		{
    			main_->cameraNode_ = players_[x]->mech_->GetChild("camera");
    			main_->viewport_->SetCamera(main_->cameraNode_->GetComponent<Camera>());
    			mechSelf_ = players_[x]->mech_;
    	    	SubscribeToEvent(mechSelf_, E_NODECOLLISIONSTART, HANDLER(Client, HandleNodeCollisionStart));
    			break;
    		}
    	}
    }
    else if (msgID == MSG_ANIM)
    {
    	//Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

    	const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
    	MemoryBuffer msg(data);
        int clientID =  msg.ReadInt();
        String anim = msg.ReadString();

    	for (int x = 0; x < players_.Size(); x++)
    	{
    		if (players_[x]->ClientID_ == clientID)
    		{
    			players_[x]->mech_->GetComponent<AnimatedSprite2D>()->SetAnimation(anim);
    			if (anim == "runUp")
    			{
    				players_[x]->mech_->GetChild("cannon")->SetRotation(Quaternion(45.0f, 180.0f, 0.0f));
    			}
    			else if (anim == "runDown")
    			{
    				players_[x]->mech_->GetChild("cannon")->SetRotation(Quaternion(-45.0f, 0.0f, 0.0f));
    			}
    			else if (anim == "runLeft")
    			{
    				players_[x]->mech_->GetChild("cannon")->SetRotation(Quaternion(0.0f, 90.0f, 0.0f));
    			}
    			else if (anim == "runRight")
    			{
    				players_[x]->mech_->GetChild("cannon")->SetRotation(Quaternion(0.0f, -90.0f, 0.0f));
    			}
    			break;
    		}
    	}
    }
}

void Client::TouchDown(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchBegin;

	if (!mechSelf_)
	{
		return;
	}

	Ray cameraRay = cameraNode_->GetComponent<Camera>()->GetScreenRay(
			(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
			(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());

	PODVector<RayQueryResult> results;

	RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, 1000.0f,
			DRAWABLE_GEOMETRY);

	scene_->GetComponent<Octree>()->Raycast(query);

	//Send animation message.
	msg_.Clear();
	msg_.WriteInt(clientIDSelf_);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			if (results[x].node_->GetName() == "upButt")
			{
				msg_.WriteString("runUp");
			}
			if (results[x].node_->GetName() == "downButt")
			{
				msg_.WriteString("runDown");
			}
			if (results[x].node_->GetName() == "leftButt")
			{
				msg_.WriteString("runLeft");
			}
			if (results[x].node_->GetName() == "rightButt")
			{
				msg_.WriteString("runRight");
			}
		}
	}

	GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);

	//Cast again but for movement
	results.Clear();
	cameraRay = main_->cameraNode_->GetComponent<Camera>()->GetScreenRay(
				(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
				(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());
	query.ray_ = cameraRay;
	main_->scene_->GetComponent<Octree>()->Raycast(query);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			if (results[x].node_->GetName() == "street")
			{
				victoria_ = results[x].position_;
				victoria_.y_ = 0.5f;
				vectoria_ = (victoria_ - mechSelf_->GetPosition()).Normalized();

				msg_.Clear();
				msg_.WriteInt(clientIDSelf_);
				msg_.WriteVector3(vectoria_);

				GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_MOVE, true, true, msg_);
			}
			else//Outside of the map.
			{
				victoria_ = Vector3::ZERO;
				victoria_.y_ = 0.5f;
				vectoria_ = (victoria_ - mechSelf_->GetPosition()).Normalized();

				msg_.Clear();
				msg_.WriteInt(clientIDSelf_);
				msg_.WriteVector3(vectoria_);

				GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_MOVE, true, true, msg_);
			}
		}
	}
}

void Client::TouchUp(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchEnd;

	if (!mechSelf_)
	{
		return;
	}

	//Send animation message.
	msg_.Clear();
	msg_.WriteInt(clientIDSelf_);//Server ClientID

	if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runUp")
	{
		msg_.WriteString("standUp");
		GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
	}
	else if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runDown")
	{
		msg_.WriteString("standDown");
		GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
	}
	else if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runLeft")
	{
		msg_.WriteString("standLeft");
		GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
	}
	else if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runRight")
	{
		msg_.WriteString("standRight");
		GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
	}

	msg_.Clear();
	msg_.WriteInt(clientIDSelf_);//Server ClientID
	msg_.WriteVector3(mechSelf_->GetPosition());
	GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_STOP, true, true, msg_);
}

void Client::TouchDrag(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchMove;

	if (!mechSelf_)
	{
		return;
	}

	Ray cameraRay = cameraNode_->GetComponent<Camera>()->GetScreenRay(
			(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
			(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());

	PODVector<RayQueryResult> results;

	RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, 1000.0f,
			DRAWABLE_GEOMETRY);

	scene_->GetComponent<Octree>()->Raycast(query);

	//Send animation message.
	msg_.Clear();
	msg_.WriteInt(clientIDSelf_);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			if (results[x].node_->GetName() == "upButt")
			{
				//do stuffs with butt
				if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runUp")
				{
					msg_.WriteString("runUp");
					GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
				}
			}
			if (results[x].node_->GetName() == "downButt")
			{
				//do stuffs with butt
				if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runDown")
				{
					msg_.WriteString("runDown");
					GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
				}
			}
			if (results[x].node_->GetName() == "leftButt")
			{
				//do stuffs with butt
				if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runLeft")
				{
					msg_.WriteString("runLeft");
					GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
				}
			}
			if (results[x].node_->GetName() == "rightButt")
			{
				//do stuffs with butt
				if (mechSelf_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runRight")
				{
					msg_.WriteString("runRight");
					GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_ANIM, true, true, msg_);
				}
			}
		}
	}

	//Cast again but for movement
	results.Clear();
	cameraRay = main_->cameraNode_->GetComponent<Camera>()->GetScreenRay(
			(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
			(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());
	query.ray_ = cameraRay;
	main_->scene_->GetComponent<Octree>()->Raycast(query);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			if (results[x].node_->GetName() == "street")
			{
				victoria_ = results[x].position_;
				victoria_.y_ = 0.5f;
				vectoria_ = (victoria_ - mechSelf_->GetPosition()).Normalized();

				msg_.Clear();
				msg_.WriteInt(clientIDSelf_);
				msg_.WriteVector3(vectoria_);
				GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_MOVE, true, true, msg_);
			}
			else//Outside of the map.
			{
				victoria_ = Vector3::ZERO;
				victoria_.y_ = 0.5f;
				vectoria_ = (victoria_ - mechSelf_->GetPosition()).Normalized();

				msg_.Clear();
				msg_.WriteInt(clientIDSelf_);
				msg_.WriteVector3(vectoria_);

				GetSubsystem<Network>()->GetServerConnection()->SendMessage(MSG_MOVE, true, true, msg_);
			}
		}
	}

}

void Client::SpawnZombie()
{
	int randy = Random(0,zombies_.Size());
	Zombie* zee = zombies_[randy];
	if (!zee){return;}
	zombies_.Remove(zee);
	spawnedZombies_.Push(zee);
	zee->zombie_->SetVar("vectorIndex", spawnedZombies_.Size()-1);
	Node* zSpawn = zSpawns_->GetChild(Random(0,zSpawns_->GetNumChildren()));
	zee->zombie_->SetEnabled(true);
	zee->zombie_->SetPosition(zSpawn->GetWorldPosition());

	float randySpeed = Random(0.5f, 1.25f);
	vectoria_ = ravenMech_->GetPosition();
	victoria_.x_ = Random(vectoria_.x_ - zMoveRandyDeviation_, vectoria_.x_ + zMoveRandyDeviation_);
	victoria_.y_ = 0.5f;
	victoria_.z_ = Random(vectoria_.z_ - zMoveRandyDeviation_, vectoria_.z_ + zMoveRandyDeviation_);
	zee->zombie_->GetComponent<SceneObjectMoveTo>()->MoveTo(victoria_, randySpeed, true);

	int animDir = CalculateAnimDir(zSpawn->GetWorldPosition(), victoria_);

	if (animDir == 0)
	{
		zee->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runUp");
	}
	else if (animDir == 1)
	{
		zee->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runDown");
	}
	else if (animDir == 2)
	{
		zee->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runLeft");
	}
	else if (animDir == 3)
	{
		zee->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runRight");
	}
}

void Client::HandleMoveToComplete(StringHash eventType, VariantMap& eventData)
{
	using namespace SceneObjectMoveToComplete;

	Node* noed = (Node*)(eventData[P_NODE].GetPtr());

	float randySpeed = Random(0.5f, 2.0f);

	vectoria_ = ravenMech_->GetPosition();
	victoria_.x_ = Random(vectoria_.x_ - zMoveRandyDeviation_, vectoria_.x_ + zMoveRandyDeviation_);
	victoria_.y_ = 0.5f;
	victoria_.z_ = Random(vectoria_.z_ - zMoveRandyDeviation_, vectoria_.z_ + zMoveRandyDeviation_);

	noed->GetComponent<SceneObjectMoveTo>()->MoveTo(victoria_, randySpeed, true);

	int animDir = CalculateAnimDir(noed->GetWorldPosition(), victoria_);

	if (animDir == 0)
	{
		noed->GetComponent<AnimatedSprite2D>()->SetAnimation("runUp");
	}
	else if (animDir == 1)
	{
		noed->GetComponent<AnimatedSprite2D>()->SetAnimation("runDown");
	}
	else if (animDir == 2)
	{
		noed->GetComponent<AnimatedSprite2D>()->SetAnimation("runLeft");
	}
	else if (animDir == 3)
	{
		noed->GetComponent<AnimatedSprite2D>()->SetAnimation("runRight");
	}

}

int Client::CalculateAnimDir(const Vector3& start, const Vector3& end)
{
	float xDist = Abs(start.x_ - end.x_);
	float zDist = Abs(start.z_ - end.z_);

	if (xDist > zDist)//left/right
	{
		if (start.x_ < end.x_)//right
		{
			return 1;
		}
		else//left
		{
			return 0;
		}
	}
	else//up/down
	{
		if (start.z_ < end.z_)//up
		{
			return 3;
		}
		else//down
		{
			return 2;
		}
	}

	return 0;
}

void Client::HandleNodeCollision(StringHash eventType, VariantMap& eventData)
{
	using namespace NodeCollision;

	Node* noed = static_cast<RigidBody*>(eventData[P_BODY].GetPtr())->GetNode();
	Node* OtherNode = static_cast<Node*>(eventData[P_OTHERNODE].GetPtr());

	if (noed->GetName() == "cannon")
	{
		if (OtherNode->GetName() == "zombie")
		{
			OtherNode->GetChild("blood")->SetEnabled(true);
			int health = OtherNode->GetVar("Health").GetInt();
			health--;
			if (health < 0)
			{
				int vectorIndex = OtherNode->GetVar("vectorIndex").GetInt();
				Zombie* zee = spawnedZombies_[vectorIndex];
				spawnedZombies_.Remove(zee);
				zombies_.Push(zee);
				health = 100;
				zee->zombie_->SetEnabled(false);
				score_++;
				UpdateScore();
			}
			OtherNode->SetVar("Health", health);
		}
	}
}

void Client::HandleNodeCollisionStart(StringHash eventType, VariantMap& eventData)
{
	using namespace NodeCollisionStart;

	Node* noed = static_cast<RigidBody*>(eventData[P_BODY].GetPtr())->GetNode();
	Node* OtherNode = static_cast<Node*>(eventData[P_OTHERNODE].GetPtr());

	if (noed->GetName() == "RavenMech")
	{
		if (OtherNode->GetName() == "zombie")
		{
			int health = noed->GetVar("Health").GetInt();
			health--;
			if (health < 0)
			{
				health = 100;
				UpdateScore();
				score_ = 0;
			}
			noed->SetVar("Health", health);
			health_ = health;
		}
	}
}

void Client::HandleNodeCollisionEnd(StringHash eventType, VariantMap& eventData)
{
	using namespace NodeCollisionEnd;

	Node* noed = static_cast<RigidBody*>(eventData[P_BODY].GetPtr())->GetNode();
	Node* OtherNode = static_cast<Node*>(eventData[P_OTHERNODE].GetPtr());

	if (noed->GetName() == "cannon")
	{
		if (OtherNode->GetName() == "zombie")
		{
			OtherNode->GetChild("blood")->SetEnabled(false);
		}
	}
}

void Client::UpdateScore()
{
	if (score_ > topScore_)
	{
		topScore_ = score_;
	}
	text_->SetText(
			"Zombies Nullified: " + String(score_) + " Longest Streak: "
			+ String(topScore_) + " Structural Integrity: " + String(health_));
}
