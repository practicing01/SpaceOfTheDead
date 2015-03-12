/*
 * Server.cpp
 *
 *  Created on: Feb 24, 2015
 *      Author: practicing01
 */

#include "Server.h"

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
#include <Urho3D/Navigation/Navigable.h>
#include <Urho3D/Navigation/NavigationMesh.h>
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
#include "SceneObjectMoveTo.h"

Server::Server(Context* context, Urho3DPlayer* main, bool dedicated) :
    Object(context)
{
	context->RegisterFactory<MoveTowards>();
	context->RegisterFactory<NetPulse>();
	context->RegisterFactory<SceneObjectMoveTo>();

	main_ = main;
	elapsedTime_ = 0.0f;

	main_->network_->StartServer(9001);

	dedServer_ = dedicated;

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
	SubscribeToEvent(ravenMech_->GetChild("cannon"), E_NODECOLLISION, HANDLER(Server, HandleNodeCollision));
	SubscribeToEvent(ravenMech_->GetChild("cannon"), E_NODECOLLISIONEND, HANDLER(Server, HandleNodeCollisionEnd));
	SubscribeToEvent(ravenMech_, E_NODECOLLISIONSTART, HANDLER(Server, HandleNodeCollisionStart));

	netPulseDummy_ = new Node(context_);
	scene_->AddChild(netPulseDummy_);
	netPulseDummy_->AddComponent(new NetPulse(context_), 0, LOCAL);

	network_ = GetSubsystem<Network>();
	clientIDCount_ = 1;//0 will be server.

	zSpawns_ = main_->scene_->GetChild("zSpawns");

	patientZero_ = main_->scene_->GetChild("zombie");
	patientZero_->AddComponent(new SceneObjectMoveTo(context_), 0, LOCAL);

	for (int x = 0 ; x < 200; x++)
	{
		Zombie* zee = new Zombie;
		zee->zombie_ = patientZero_->Clone(LOCAL);
		zombies_.Push(zee);
		SubscribeToEvent(E_SCENEOBJECTMOVETOCOMPLETE, HANDLER(Server, HandleMoveToComplete));
	}

	zSpawnInterval_ = 1.0f;

	navMesh_ = main_->scene_->CreateComponent<NavigationMesh>();
	main_->scene_->CreateComponent<Navigable>();
	navMesh_->SetPadding(Vector3(0.0f, 10.0f, 0.0f));
	navMesh_->Build();

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

	SubscribeToEvent(E_UPDATE, HANDLER(Server, HandleUpdate));
	SubscribeToEvent(E_TOUCHBEGIN, HANDLER(Server, TouchDown));
	SubscribeToEvent(E_TOUCHMOVE, HANDLER(Server, TouchDrag));
	SubscribeToEvent(E_TOUCHEND, HANDLER(Server, TouchUp));
	SubscribeToEvent(E_CLIENTDISCONNECTED, HANDLER(Server, HandleClientDisconnect));
	SubscribeToEvent(E_CLIENTCONNECTED, HANDLER(Server, HandleClientConnect));
	SubscribeToEvent(E_NETWORKMESSAGE, HANDLER(Server, HandleNetworkMessage));
}

Server::~Server()
{
}

void Server::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	using namespace Update;

	float timeStep = eventData[P_TIMESTEP].GetFloat();

	//LOGERRORF("server loop");
	elapsedTime_ += timeStep;

	if (elapsedTime_ >= zSpawnInterval_)
	{
		elapsedTime_ = 0.0f;
		for (int x = 0; x < 10; x++)
		{
			SpawnZombie();
		}
		//MoveZombies(timeStep);
	}

	//MoveZombies(timeStep);

	if (main_->input_->GetKeyDown(SDLK_ESCAPE))
	{
		main_->GetSubsystem<Engine>()->Exit();
	}

}

void Server::TouchDown(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchBegin;

	Ray cameraRay = cameraNode_->GetComponent<Camera>()->GetScreenRay(
			(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
			(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());

	PODVector<RayQueryResult> results;

	RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, 1000.0f,
			DRAWABLE_GEOMETRY);

	scene_->GetComponent<Octree>()->Raycast(query);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			//LOGERRORF(results[x].node_->GetName().CString());
			if (results[x].node_->GetName() == "upButt")
			{
				//do stuffs with butt
				ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runUp");
				ravenMech_->GetChild("cannon")->SetRotation(Quaternion(45.0f, 180.0f, 0.0f));
				//ravenMech_->GetComponent<MoveTowards>()->MoveToward(Vector3::LEFT, 1.0f);
			}
			if (results[x].node_->GetName() == "downButt")
			{
				//do stuffs with butt
				ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runDown");
				ravenMech_->GetChild("cannon")->SetRotation(Quaternion(-45.0f, 0.0f, 0.0f));
				//ravenMech_->GetComponent<MoveTowards>()->MoveToward(Vector3::RIGHT, 1.0f);
			}
			if (results[x].node_->GetName() == "leftButt")
			{
				//do stuffs with butt
				ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runLeft");
				ravenMech_->GetChild("cannon")->SetRotation(Quaternion(0.0f, 90.0f, 0.0f));
				//ravenMech_->GetComponent<MoveTowards>()->MoveToward(Vector3::BACK, 1.0f);
			}
			if (results[x].node_->GetName() == "rightButt")
			{
				//do stuffs with butt
				ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runRight");
				ravenMech_->GetChild("cannon")->SetRotation(Quaternion(0.0f, -90.0f, 0.0f));
				//ravenMech_->GetComponent<MoveTowards>()->MoveToward(Vector3::FORWARD, 1.0f);
			}
		}
	}

	//Send animation message.
	msg_.Clear();
	msg_.WriteInt(0);//Server ClientID
	msg_.WriteString(ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation().CString());
	network_->BroadcastMessage(MSG_ANIM, true, true, msg_);

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
				vectoria_ = (victoria_ - ravenMech_->GetPosition()).Normalized();
				ravenMech_->GetComponent<MoveTowards>()->MoveToward(vectoria_, mechSpeed_, mechSpeed_);

				msg_.Clear();
				msg_.WriteInt(0);//Server ClientID
				msg_.WriteVector3(vectoria_);
				msg_.WriteFloat(0.0f);//no additional lag because it's from server.
				network_->BroadcastMessage(MSG_MOVE, true, true, msg_);
			}
			else//Outside of the map.
			{
				victoria_ = Vector3::ZERO;
				victoria_.y_ = 0.5f;
				vectoria_ = (victoria_ - ravenMech_->GetPosition()).Normalized();
				ravenMech_->GetComponent<MoveTowards>()->MoveToward(vectoria_, mechSpeed_, mechSpeed_);

				msg_.Clear();
				msg_.WriteInt(0);//Server ClientID
				msg_.WriteVector3(vectoria_);
				msg_.WriteFloat(0.0f);//no additional lag because it's from server.
				network_->BroadcastMessage(MSG_MOVE, true, true, msg_);
			}
		}
	}
}

void Server::TouchUp(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchEnd;

	if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runUp")
	{
		ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("standUp");
		ravenMech_->GetComponent<MoveTowards>()->Stop();
	}
	else if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runDown")
	{
		ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("standDown");
		ravenMech_->GetComponent<MoveTowards>()->Stop();
	}
	else if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runLeft")
	{
		ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("standLeft");
		ravenMech_->GetComponent<MoveTowards>()->Stop();
	}
	else if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() == "runRight")
	{
		ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("standRight");
		ravenMech_->GetComponent<MoveTowards>()->Stop();
	}

	//Send animation message.
	msg_.Clear();
	msg_.WriteInt(0);//Server ClientID
	msg_.WriteString(ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation().CString());
	network_->BroadcastMessage(MSG_ANIM, true, true, msg_);

	msg_.Clear();
	msg_.WriteInt(0);//Server ClientID
	msg_.WriteVector3(ravenMech_->GetPosition());
	network_->BroadcastMessage(MSG_STOP, true, true, msg_);
}

void Server::TouchDrag(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchMove;

	Ray cameraRay = cameraNode_->GetComponent<Camera>()->GetScreenRay(
			(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
			(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());

	PODVector<RayQueryResult> results;

	RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, 1000.0f,
			DRAWABLE_GEOMETRY);

	scene_->GetComponent<Octree>()->Raycast(query);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			if (results[x].node_->GetName() == "upButt")
			{
				//do stuffs with butt
				if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runUp")
				{
					ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runUp");
					ravenMech_->GetChild("cannon")->SetRotation(Quaternion(45.0f, 180.0f, 0.0f));
					//Send animation message.
					msg_.Clear();
					msg_.WriteInt(0);//Server ClientID
					msg_.WriteString(ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation().CString());
					network_->BroadcastMessage(MSG_ANIM, true, true, msg_);
				}
			}
			if (results[x].node_->GetName() == "downButt")
			{
				//do stuffs with butt
				if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runDown")
				{
					ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runDown");
					ravenMech_->GetChild("cannon")->SetRotation(Quaternion(-45.0f, 0.0f, 0.0f));
					//Send animation message.
					msg_.Clear();
					msg_.WriteInt(0);//Server ClientID
					msg_.WriteString(ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation().CString());
					network_->BroadcastMessage(MSG_ANIM, true, true, msg_);
				}
			}
			if (results[x].node_->GetName() == "leftButt")
			{
				//do stuffs with butt
				if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runLeft")
				{
					ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runLeft");
					ravenMech_->GetChild("cannon")->SetRotation(Quaternion(0.0f, 90.0f, 0.0f));
					//Send animation message.
					msg_.Clear();
					msg_.WriteInt(0);//Server ClientID
					msg_.WriteString(ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation().CString());
					network_->BroadcastMessage(MSG_ANIM, true, true, msg_);
				}
			}
			if (results[x].node_->GetName() == "rightButt")
			{
				//do stuffs with butt
				if (ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation() != "runRight")
				{
					ravenMech_->GetComponent<AnimatedSprite2D>()->SetAnimation("runRight");
					ravenMech_->GetChild("cannon")->SetRotation(Quaternion(0.0f, -90.0f, 0.0f));
					//Send animation message.
					msg_.Clear();
					msg_.WriteInt(0);//Server ClientID
					msg_.WriteString(ravenMech_->GetComponent<AnimatedSprite2D>()->GetAnimation().CString());
					network_->BroadcastMessage(MSG_ANIM, true, true, msg_);
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
				vectoria_ = (victoria_ - ravenMech_->GetPosition()).Normalized();
				ravenMech_->GetComponent<MoveTowards>()->MoveToward(vectoria_, mechSpeed_, mechSpeed_);

				msg_.Clear();
				msg_.WriteInt(0);//Server ClientID
				msg_.WriteVector3(vectoria_);
				msg_.WriteFloat(0.0f);//no additional lag because it's from server.
				network_->BroadcastMessage(MSG_MOVE, true, true, msg_);
			}
			else//Outside of the map.
			{
				victoria_ = Vector3::ZERO;
				victoria_.y_ = 0.5f;
				vectoria_ = (victoria_ - ravenMech_->GetPosition()).Normalized();
				ravenMech_->GetComponent<MoveTowards>()->MoveToward(vectoria_, mechSpeed_, mechSpeed_);

				msg_.Clear();
				msg_.WriteInt(0);//Server ClientID
				msg_.WriteVector3(vectoria_);
				msg_.WriteFloat(0.0f);//no additional lag because it's from server.
				network_->BroadcastMessage(MSG_MOVE, true, true, msg_);
			}
		}
	}

}

void Server::HandleClientDisconnect(StringHash eventType, VariantMap& eventData)
{
	//LOGERRORF("client diconnected from server");

	using namespace NetworkMessage;

	Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

	netPulseDummy_->GetComponent<NetPulse>()->RemoveConnection(sender);

	//Let other clients know.

	msg_.Clear();

	for (int x = 0; x < players_.Size(); x++)
	{
		if (players_[x]->connection_ == sender)
		{
			msg_.WriteInt(players_[x]->ClientID_);
			players_[x]->mech_->RemoveAllChildren();
			players_[x]->mech_->RemoveAllComponents();
			players_[x]->mech_->Remove();
			players_.Remove(players_[x]);
			break;
		}
	}

	network_->BroadcastMessage(MSG_CLIENTDISCO, true, true, msg_);
}

void Server::HandleClientConnect(StringHash eventType, VariantMap& eventData)
{
	//LOGERRORF("client connected to server");

	using namespace NetworkMessage;

	Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

	//Alert the other clients.
	msg_.Clear();
	msg_.WriteInt(clientIDCount_);
	network_->BroadcastMessage(MSG_NEWCLIENT, true, true, msg_);

	Player* newPlayer = new Player;
	players_.Push(newPlayer);
	newPlayer->ClientID_ = clientIDCount_;
	newPlayer->mech_ = ravenMech_->Clone(LOCAL);
	newPlayer->connection_ = sender;

	sender->SendMessage(MSG_CLIENTIDSELF, true, true, msg_);

	clientIDCount_++;

	//Send the new client the other clients infos.
	for (int x = 0; x < players_.Size()-1; x++)
	{
		msg_.Clear();
		msg_.WriteInt(players_[x]->ClientID_);
		sender->SendMessage(MSG_NEWCLIENT, true, true, msg_);
	}

	SubscribeToEvent(newPlayer->mech_->GetChild("cannon"), E_NODECOLLISION, HANDLER(Server, HandleNodeCollision));
	SubscribeToEvent(newPlayer->mech_->GetChild("cannon"), E_NODECOLLISIONEND, HANDLER(Server, HandleNodeCollisionEnd));
}

void Server::HandleNetworkMessage(StringHash eventType, VariantMap& eventData)
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
        float lagTime = 0.0f;

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

    	msg_.Clear();
    	msg_.WriteInt(clientID);
    	msg_.WriteVector3(vectoria_);
    	msg_.WriteFloat(lagTime);
        network_->BroadcastMessage(MSG_MOVE, true, true, msg_);

        for (int x = 0; x < players_.Size(); x++)
        {
        	if (players_[x]->connection_ == sender)
        	{
        		players_[x]->mech_->GetComponent<MoveTowards>()->MoveToward(vectoria_, mechSpeed_, mechSpeed_ + lagTime);
        		break;
        	}
        }
    }
    else if (msgID == MSG_STOP)
    {
    	Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

        const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
        MemoryBuffer msg(data);
        int clientID =  msg.ReadInt();
        vectoria_ = msg.ReadVector3();

    	msg_.Clear();
    	msg_.WriteInt(clientID);
    	msg_.WriteVector3(vectoria_);
        network_->BroadcastMessage(MSG_STOP, true, true, msg_);

        for (int x = 0; x < players_.Size(); x++)
        {
        	if (players_[x]->connection_ == sender)
        	{
        		players_[x]->mech_->GetComponent<MoveTowards>()->Stop();
        		players_[x]->mech_->SetPosition(vectoria_);
        		break;
        	}
        }
    }
    else if (msgID == MSG_ANIM)
    {
    	Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

        const PODVector<unsigned char>& data = eventData[P_DATA].GetBuffer();
        MemoryBuffer msg(data);
        int clientID =  msg.ReadInt();
        String anim = msg.ReadString();

    	msg_.Clear();
    	msg_.WriteInt(clientID);
    	msg_.WriteString(anim);
        network_->BroadcastMessage(MSG_ANIM, true, true, msg_);

        for (int x = 0; x < players_.Size(); x++)
        {
        	if (players_[x]->connection_ == sender)
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

void Server::SpawnZombie()
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

	//victoria_ = navMesh_->FindNearestPoint(ravenMech_->GetPosition(), Vector3(1.0f, 1.0f, 1.0f));
	//navMesh_->FindPath(zee->currentPath_, zSpawn->GetWorldPosition(), victoria_);
}

void Server::MoveZombies(float timeStep)
{
	for (int x = 0; x < spawnedZombies_.Size(); x++)
	{

		float randySpeed = Random(0.5f, 1.5f);
		//vectoria_ = (ravenMech_->GetPosition() - spawnedZombies_[x]->zombie_->GetPosition()).Normalized();
		//spawnedZombies_[x]->zombie_->GetComponent<MoveTowards>()->MoveToward(vectoria_, randySpeed, randySpeed);
		spawnedZombies_[x]->zombie_->GetComponent<SceneObjectMoveTo>()->MoveTo(ravenMech_->GetPosition(), randySpeed, true);

		int animDir = CalculateAnimDir(spawnedZombies_[x]->zombie_->GetWorldPosition(), victoria_);

		if (animDir == 0)
		{
			spawnedZombies_[x]->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runUp");
		}
		else if (animDir == 1)
		{
			spawnedZombies_[x]->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runDown");
		}
		else if (animDir == 2)
		{
			spawnedZombies_[x]->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runLeft");
		}
		else if (animDir == 3)
		{
			spawnedZombies_[x]->zombie_->GetComponent<AnimatedSprite2D>()->SetAnimation("runRight");
		}

		/*if (spawnedZombies_[x]->currentPath_.Size())
		{
			victoria_ = spawnedZombies_[x]->currentPath_[0];
			float move = zSpeed_ * timeStep;
			float distance = (spawnedZombies_[x]->zombie_->GetPosition() - victoria_).Length();
			if (move > distance)
				move = distance;

			//spawnedZombies_[x]->zombie_->LookAt(victoria_, Vector3::UP);
			spawnedZombies_[x]->zombie_->Translate(Vector3::FORWARD * move);

			// Remove waypoint if reached it
			if (distance < 0.1f)
				spawnedZombies_[x]->currentPath_.Erase(0);
		}*/
	}
}

void Server::HandleMoveToComplete(StringHash eventType, VariantMap& eventData)
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

int Server::CalculateAnimDir(const Vector3& start, const Vector3& end)
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

void Server::HandleNodeCollision(StringHash eventType, VariantMap& eventData)
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

void Server::HandleNodeCollisionStart(StringHash eventType, VariantMap& eventData)
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

void Server::HandleNodeCollisionEnd(StringHash eventType, VariantMap& eventData)
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

void Server::UpdateScore()
{
	if (score_ > topScore_)
	{
		topScore_ = score_;
	}
	text_->SetText(
			"Zombies Nullified: " + String(score_) + " Longest Streak: "
			+ String(topScore_) + " Structural Integrity: " + String(health_));
}
