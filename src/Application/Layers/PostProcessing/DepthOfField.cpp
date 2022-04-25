#include "DepthOfField.h"

#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"
#include "../RenderLayer.h"
#include "Application/Application.h"

DepthOfField::DepthOfField() :
	PostProcessingLayer::Effect(),
	_shader(nullptr)
{
	Name = "Depth of Field";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/depth_of_field.glsl" }
	});
}

DepthOfField::~DepthOfField() = default;

void DepthOfField::Apply(const Framebuffer::Sptr& gBuffer)
{
	_shader->Bind();
	gBuffer->BindAttachment(RenderTargetAttachment::Depth, 1);
}

void DepthOfField::RenderImGui()
{
	const auto& cam = Application::Get().CurrentScene()->MainCamera;

	if (cam != nullptr) {
		ImGui::DragFloat("Focal Depth", &cam->FocalDepth, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("Lens Dist. ", &cam->LensDepth,  0.01f, 0.001f, 50.0f);
		ImGui::DragFloat("Aperture   ", &cam->Aperture,   0.1f, 0.1f, 60.0f);
	}
}

DepthOfField::Sptr DepthOfField::FromJson(const nlohmann::json& data)
{
	DepthOfField::Sptr result = std::make_shared<DepthOfField>();
	result->Enabled = JsonGet(data, "enabled", true);
	return result;
}

nlohmann::json DepthOfField::ToJson() const
{
	return {
		{ "enabled", Enabled }
	};
}
