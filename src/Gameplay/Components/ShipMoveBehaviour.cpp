#include "ShipMoveBehaviour.h"

#include "Gameplay/GameObject.h"

#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"
#include "GLM/gtc/quaternion.hpp"


ShipMoveBehaviour::ShipMoveBehaviour() :
	IComponent(),
	Center(0.0f),
	Angle(0.0f),
	Tilt(30.0f),
	Radius(3.0f),
	Speed(2.0f)
{ }

ShipMoveBehaviour::~ShipMoveBehaviour() = default;

void ShipMoveBehaviour::RenderImGui() {
	LABEL_LEFT(ImGui::DragFloat3, "Center ", &Center.x, 0.01f);
	LABEL_LEFT(ImGui::DragFloat,  "Angle  ", &Angle,  1.0f);
	LABEL_LEFT(ImGui::DragFloat,  "Tilt   ", &Tilt,   1.00f);
	LABEL_LEFT(ImGui::DragFloat,  "Radius ", &Radius, 1.00f);
	LABEL_LEFT(ImGui::DragFloat,  "Speed  ", &Speed,  1.00f);
}

nlohmann::json ShipMoveBehaviour::ToJson() const {
	return {
		{ "center", Center },
		{ "angle", Angle },
		{ "tilt", Tilt },
		{ "radius", Radius },
		{ "speed", Speed },
	};
}

ShipMoveBehaviour::Sptr ShipMoveBehaviour::FromJson(const nlohmann::json & blob) {
	ShipMoveBehaviour::Sptr result = std::make_shared<ShipMoveBehaviour>();
	result->Center = blob["center"];
	result->Angle = blob["angle"];
	result->Tilt = blob["tilt"];
	result->Radius = blob["radius"];
	result->Speed = blob["speed"];
	return result;
}

void ShipMoveBehaviour::Update(float deltaTime) {

	glm::vec3 pos = Center + glm::vec3(
		glm::sin(glm::radians(Angle)), 
		glm::cos(glm::radians(Angle)),
		glm::cos(glm::radians(Angle / 2.0f)) / Radius
	) * Radius;
	Angle += Speed * deltaTime;

	glm::quat rotX = glm::angleAxis(glm::radians(-Angle - 90), glm::vec3(0, 0, 1));
	glm::quat rotY = glm::angleAxis(glm::radians(Tilt), glm::vec3(0, 1, 0));
	glm::quat rotZ = glm::angleAxis(-glm::sin(glm::radians(Angle / 2.0f)) / Radius, glm::vec3(1, 0, 0));
	glm::quat currentRot = rotX * rotY * rotZ;

	GetGameObject()->SetPostion(pos);
	GetGameObject()->SetRotation(currentRot);
}