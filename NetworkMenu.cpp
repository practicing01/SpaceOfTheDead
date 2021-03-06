/*
 * NetworkMenu.cpp
 *
 *  Created on: Feb 24, 2015
 *      Author: practicing01
 */

#include "NetworkMenu.h"

#include <Urho3D/Urho3D.h>

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Sprite.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/Graphics/Viewport.h>

#include <Urho3D/DebugNew.h>

#include "Server.h"
#include "Client.h"

NetworkMenu::NetworkMenu(Context* context, Urho3DPlayer* main) :
    Object(context)
{
	main_ = main;
	elapsedTime_ = 0.0f;

	main_->cameraNode_->RemoveAllChildren();
	main_->cameraNode_->RemoveAllComponents();
	main_->cameraNode_->Remove();

	File loadFile(context_,main->filesystem_->GetProgramDir()
			+ "Data/Scenes/networkMenu.xml", FILE_READ);
	main_->scene_->LoadXML(loadFile);

	main_->cameraNode_ = main_->scene_->GetChild("camera");
	main_->viewport_->SetCamera(main_->cameraNode_->GetComponent<Camera>());

	UI* ui = GetSubsystem<UI>();

	XMLFile* style = main_->cache_->GetResource<XMLFile>("UI/DefaultStyle.xml");
	ui->GetRoot()->SetDefaultStyle(style);

    SharedPtr<UIElement> layoutRoot = ui->LoadLayout(main_->cache_->GetResource<XMLFile>("UI/ipAddress.xml"));
    ui->GetRoot()->AddChild(layoutRoot);

	SubscribeToEvent(E_UPDATE, HANDLER(NetworkMenu, HandleUpdate));
	SubscribeToEvent(E_TOUCHBEGIN, HANDLER(NetworkMenu, TouchDown));
	SubscribeToEvent(E_TEXTFINISHED, HANDLER(NetworkMenu, HandleTextFinished));
}

NetworkMenu::~NetworkMenu()
{
}

void NetworkMenu::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	using namespace Update;

	//float timeStep = eventData[P_TIMESTEP].GetFloat();

	//LOGERRORF("loop");
	//elapsedTime_ += timeStep;

}

void NetworkMenu::TouchDown(StringHash eventType, VariantMap& eventData)
{
	using namespace TouchBegin;

	Ray cameraRay = main_->cameraNode_->GetComponent<Camera>()->GetScreenRay(
			(float) eventData[P_X].GetInt() / main_->graphics_->GetWidth(),
			(float) eventData[P_Y].GetInt() / main_->graphics_->GetHeight());

	PODVector<RayQueryResult> results;

	RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, 1000.0f,
			DRAWABLE_GEOMETRY);

	main_->scene_->GetComponent<Octree>()->Raycast(query);

	if (results.Size())
	{
		for (int x = 0; x < results.Size(); x++)
		{
			//LOGERRORF(results[x].node_->GetName().CString());
			if (results[x].node_->GetName() == "hostButt")
			{
				//do stuffs with butt
				main_->scene_->RemoveAllChildren();
				main_->scene_->RemoveAllComponents();
				main_->GetSubsystem<UI>()->GetRoot()->RemoveAllChildren();
				main_->logicStates_.Remove(this);
				main_->logicStates_.Push(new Server(context_, main_, false));
				delete this;
				return;
			}
			else if (results[x].node_->GetName() == "joinButt")
			{
				//do stuffs with butt
				main_->scene_->RemoveAllChildren();
				main_->scene_->RemoveAllComponents();
				main_->GetSubsystem<UI>()->GetRoot()->RemoveAllChildren();
				main_->logicStates_.Remove(this);
				main_->logicStates_.Push(new Client(context_, main_, ipAddress_));
				delete this;
				return;
			}
		}
	}
}

void NetworkMenu::HandleTextFinished(StringHash eventType, VariantMap& eventData)
{
	using namespace TextFinished;

	ipAddress_ = eventData[P_TEXT].GetString();

}
