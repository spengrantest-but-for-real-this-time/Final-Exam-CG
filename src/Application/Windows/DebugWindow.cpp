#include "DebugWindow.h"
#include "Application/Application.h"
#include "Application/ApplicationLayer.h"
#include "Application/Layers/RenderLayer.h"

DebugWindow::DebugWindow() :
	IEditorWindow()
{
	Name = "Debug";
	SplitDirection = ImGuiDir_::ImGuiDir_None;
	SplitDepth = 0.5f;
	Requirements = EditorWindowRequirements::Menubar;
}

DebugWindow::~DebugWindow() = default;

void DebugWindow::RenderMenuBar() 
{
	Application& app = Application::Get();
	RenderLayer::Sptr renderLayer = app.GetLayer<RenderLayer>(); 

	BulletDebugMode physicsDrawMode = app.CurrentScene()->GetPhysicsDebugDrawMode();
	if (BulletDebugDraw::DrawModeGui("Physics Debug Mode:", physicsDrawMode)) { 
		app.CurrentScene()->SetPhysicsDebugDrawMode(physicsDrawMode);
	}

	ImGui::Separator();

	RenderFlags flags = renderLayer->GetRenderFlags();
	bool changed = false;
	bool temp = *(flags & RenderFlags::EnableColorCorrection);

	bool temp2 = *(flags & RenderFlags::EnableAlbedo);

	bool temp3 = *(flags & RenderFlags::EnableDiffuse);

	bool temp4 = *(flags & RenderFlags::EnableSpecular);

	bool temp5 = *(flags & RenderFlags::EnableEmissive);


	if (ImGui::Checkbox("Enable Color Correction", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::EnableColorCorrection) | (temp ? RenderFlags::EnableColorCorrection : RenderFlags::None);
	}

	if (ImGui::Checkbox("Enable Albedo", &temp2)) {
		changed = true;
		flags = (flags & ~*RenderFlags::EnableAlbedo) | (temp2 ? RenderFlags::EnableAlbedo: RenderFlags::None);
	}


	if (ImGui::Checkbox("Enable Diffuse", &temp3)) {
		changed = true;
		flags = (flags & ~*RenderFlags::EnableDiffuse) | (temp3 ? RenderFlags::EnableDiffuse: RenderFlags::None);
	}

	if (ImGui::Checkbox("Enable Specular", &temp4)) {
		changed = true;
		flags = (flags & ~*RenderFlags::EnableSpecular) | (temp4 ? RenderFlags::EnableSpecular: RenderFlags::None);
	}

	if (ImGui::Checkbox("Enable Emissive", &temp5)) {
		changed = true;
		flags = (flags & ~*RenderFlags::EnableEmissive) | (temp5 ? RenderFlags::EnableEmissive: RenderFlags::None);
	}




	if (changed) {
		renderLayer->SetRenderFlags(flags);
	}
}
