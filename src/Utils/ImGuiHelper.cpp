#include "ImGuiHelper.h"

// Needed for OpenGl
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "imgui_internal.h"

#include <Logging.h>

#include <GLM/glm.hpp>
#include "StringUtils.h"

GLFWwindow* ImGuiHelper::_window = nullptr;

ShaderProgram::Sptr ImGuiHelper::_linearDepthShader = nullptr;
ShaderProgram::Sptr ImGuiHelper::_arraySliceShader = nullptr;

bool CheckboxEx(ImGuiID id, ImVec2 pos, bool* value) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;

	const float square_sz = ImGui::GetFrameHeight();
	const ImRect total_bb(pos, ImVec2(pos.x + square_sz, pos.y + square_sz));

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
	if (pressed)
	{
		*value = !(*value);
		ImGui::MarkItemEdited(id);
	}

	ImGui::RenderNavHighlight(total_bb, id);
	ImGui::RenderFrame(total_bb.Min, total_bb.Max, ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);
	ImU32 check_col = ImGui::GetColorU32(ImGuiCol_CheckMark);
	if (*value)
	{
		const float pad = ImMax(1.0f, (float)(int)(square_sz / 6.0f));
		ImGui::RenderCheckMark(ImVec2(pos.x + pad, pos.y + pad), check_col, square_sz - pad * 2.0f);
	}

	return pressed;
}

void ImGuiHelper::Init(GLFWwindow* window) {
	LOG_ASSERT(_window == nullptr, "Init has already been called! Should only be called once per application");
	// Store the window
	_window = window;

	// Creates a new ImGUI context
	ImGui::CreateContext();
	// Gets our ImGUI input/output 
	ImGuiIO& io = ImGui::GetIO();
	// Enable keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Allow docking to our window
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Allow multiple viewports (so we can drag ImGui off our window)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	// Set up the ImGui implementation for OpenGL
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	// Dark mode FTW
	ImGui::StyleColorsDark();

	// Get our imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	//style.Alpha = 1.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.8f;
	}

	_linearDepthShader = ShaderProgram::Create();
	_linearDepthShader->LoadShaderPartFromFile("shaders/vertex_shaders/imgui_vs.glsl", ShaderPartType::Vertex);
	const char* fs = 
		"#version 450\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"layout (binding = 0) uniform sampler2D Texture;\n"
		"layout (location = 1) uniform vec2 DepthPlanes;\n"
		"layout (location = 0) out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"	float ndc = texelFetch(Texture, ivec2(textureSize(Texture, 9) * Frag_UV.st), 0).r * 2.0 - 1.0;\n"
		"   float depth = (2 * DepthPlanes.x * DepthPlanes.y) / (DepthPlanes.y + DepthPlanes.x - ndc * (DepthPlanes.y - DepthPlanes.x));\n"
		"   depth = (depth - DepthPlanes.x) / (DepthPlanes.y - DepthPlanes.x);\n"
		"   Out_Color = vec4(depth, depth, depth, 1.0);\n"
		"}\n";
	_linearDepthShader->LoadShaderPart(fs, ShaderPartType::Fragment);
	_linearDepthShader->Link();

	_arraySliceShader = ShaderProgram::Create();
	_arraySliceShader->LoadShaderPartFromFile("shaders/vertex_shaders/imgui_vs.glsl", ShaderPartType::Vertex);
	const char* fs2 =
		"#version 450\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"layout (binding = 1) uniform sampler2DArray Texture;\n"
		"layout (location = 1) uniform uint Slice;\n"
		"layout (location = 0) out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"   Out_Color = texture(Texture, vec3(Frag_UV, Slice));\n"
		"}\n";
	_arraySliceShader->LoadShaderPart(fs2, ShaderPartType::Fragment);
	_arraySliceShader->Link();
}

void ImGuiHelper::Cleanup() {
	if (_window != nullptr) {
		// Cleanup the ImGui implementation
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		// Destroy our ImGui context
		ImGui::DestroyContext();

		_window = nullptr;
	} else {
		LOG_WARN("Called ImGuiHelper::Cleanup without initializing, seems sus");
	}
}

bool ImGuiHelper::WarningButton(const char* text, const ImVec2& size) {
	ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));

	bool result = ImGui::Button(text, size);
	ImGui::PopStyleColor(3);
	return result;
}

void ImGuiHelper::ResourceDragSource(const IResource* resource, const std::string& name)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		std::string typeName = StringTools::SanitizeClassName(typeid(*resource).name());
		ImGui::SetDragDropPayload(typeName.c_str(), &resource->GetGUID(), sizeof(Guid));
		ImGui::Text(name.c_str());
		ImGui::EndDragDropSource();
	}
}

void ImGuiHelper::HeaderCheckbox(ImGuiID headerId, bool* value)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiContext& g = *GImGui;
	ImGuiItemHoveredDataBackup last_item_backup;
	float button_size = g.FontSize;
	float button_x = ImMax(window->DC.LastItemRect.Min.x, window->DC.LastItemRect.Max.x - g.Style.FramePadding.x * 2.0f - button_size);
	float button_y = window->DC.LastItemRect.Min.y;
	CheckboxEx(window->GetID((void*)((intptr_t)headerId + 1)), ImVec2(button_x, button_y), value);
	last_item_backup.Restore();
}

bool VertArrowEx(ImGuiID id, ImVec2 pos, bool positive, int* value) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;

	const float square_sz = ImGui::GetFrameHeight();
	const ImRect total_bb(pos, ImVec2(pos.x + square_sz, pos.y + square_sz));

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
	if (pressed)
	{
		*value = positive ? -1 : 1;
		ImGui::MarkItemEdited(id);
	}

	ImGui::RenderNavHighlight(total_bb, id);
	ImGui::RenderFrame(total_bb.Min, total_bb.Max, ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);
	ImU32 check_col = ImGui::GetColorU32(ImGuiCol_CheckMark);

	const float pad = ImMax(1.0f, (float)(int)(square_sz / 6.0f));
	ImGui::RenderArrow(ImVec2(pos.x + pad, pos.y + pad), positive ? ImGuiDir_::ImGuiDir_Up : ImGuiDir_Down);

	return pressed;
}

bool ImGuiHelper::HeaderMoveButtons(ImGuiID headerId, int* delta)
{
	if (delta != nullptr) {
		*delta = 0;
	}

	bool result = false;

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiContext& g = *GImGui;
	ImGuiItemHoveredDataBackup last_item_backup;
	float button_size = g.FontSize;
	float button_x = ImMax(window->DC.LastItemRect.Min.x, window->DC.LastItemRect.Max.x - g.Style.FramePadding.x * 2.0f - (button_size * 2.0f));
	float button_y = window->DC.LastItemRect.Min.y;
	result |= VertArrowEx(window->GetID((void*)((intptr_t)headerId + 1)), ImVec2(button_x, button_y), true, delta);
	result |= VertArrowEx(window->GetID((void*)((intptr_t)headerId + 2)), ImVec2(button_x + button_size, button_y), false, delta);
	last_item_backup.Restore();

	return result;
}

bool ImGuiHelper::DrawTextureDrop(Texture2D::Sptr& image, ImVec2 size)
{
	if (image != nullptr) {
		ImGui::Image(
			(ImTextureID)image->GetHandle(),
			size,
			ImVec2(0, 1), ImVec2(1, 0)
		);
	}
	else {
		ImGui::BeginChildFrame(ImGui::GetID("img"), size);
		ImGui::EndChildFrame();
	}
	return ImGuiHelper::ResourceDragTarget<Texture2D>(image);
}

void ImGuiHelper::DrawLinearDepthTexture(const Texture2D::Sptr& image, const glm::ivec2& size, float zNear, float zFar)
{
	struct Data {
		int programId;
		int restoreProgram;
		glm::vec2 nearFar;
		ImVec2 dispPos;
		ImVec2 dispSize;
	};

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	Data* temp = new Data();
	temp->programId = _linearDepthShader->GetHandle();
	temp->nearFar = glm::vec2(zNear, zFar);
	temp->dispPos = ImGui::GetWindowViewport()->Pos;
	temp->dispSize = ImGui::GetWindowViewport()->Size;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
		Data* data = static_cast<Data*>(cmd->UserCallbackData);

		glGetIntegerv(GL_CURRENT_PROGRAM, &data->restoreProgram);
		glUseProgram(data->programId);
		glUniform2fv(1, 1, &data->nearFar.x);
	}, temp);
	ImGui::Image((ImTextureID)image->GetHandle(), ImVec2(size.x, size.y), ImVec2(0, 1), ImVec2(1, 0));
	drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
		Data* data = static_cast<Data*>(cmd->UserCallbackData);
		glUseProgram(data->restoreProgram); 
		delete cmd->UserCallbackData;
	}, temp);

	ImGui::PopStyleVar();
}

void ImGuiHelper::DrawTextureArraySlice(const Texture2DArray::Sptr& image, uint32_t slice , const glm::ivec2& size, const ImVec4& border)
{
	if (image != nullptr) {

		struct Data {
			int programId;
			int restoreProgram;
			int texId;
			int restoreTexId;
			uint32_t slice;
			ImVec2 dispPos;
			ImVec2 dispSize;
		};

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImVec2 bbSize = ImVec2(size.x, size.y);
		if (border.w > 0) {
			bbSize.x += 2;
			bbSize.y += 2;
		}
		ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + bbSize.x, window->DC.CursorPos.y + bbSize.y));


		ImGui::ItemSize(bb);
		if (!ImGui::ItemAdd(bb, 0))
			return;

		Data* temp = new Data();
		temp->programId = _arraySliceShader->GetHandle();
		temp->dispPos = ImGui::GetWindowViewport()->Pos;
		temp->dispSize = ImGui::GetWindowViewport()->Size;
		temp->slice = slice;
		temp->texId = image->GetHandle();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		if (border.w > 0) {
			drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(border), 0.0f);
			bb.Min.x += 1;
			bb.Min.y += 1;
			bb.Max.x -= 1;
			bb.Max.y -= 1;
		}

		drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
			Data* data = static_cast<Data*>(cmd->UserCallbackData);

			glGetIntegerv(GL_CURRENT_PROGRAM, &data->restoreProgram);
			glGetIntegeri_v(GL_TEXTURE_BINDING_2D, 0, &data->restoreTexId);
			glUseProgram(data->programId);
			glUniform1uiv(1, 1, &data->slice);
			glBindTextureUnit(1, data->texId);
		}, temp);


		drawList->PrimReserve(6, 4);
		drawList->PrimRectUV(bb.Min, bb.Max, ImVec2(0, 1), ImVec2(1, 0), 0xFFFFFFFF);

		//ImGui::Image(0, ImVec2(size.x, size.y), ImVec2(0, 1), ImVec2(1, 0));
		drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
			Data* data = static_cast<Data*>(cmd->UserCallbackData);
			glUseProgram(data->restoreProgram);
			glBindTextureUnit(1, data->restoreTexId);
			delete cmd->UserCallbackData;
		}, temp);

		ImGui::PopStyleVar();
	}
	else 
	{
		ImGui::BeginChildFrame(ImGui::GetID("img"), ImVec2(size.x, size.y));
		ImGui::EndChildFrame();
	}
}

void ImGuiHelper::StartFrame() {
	LOG_ASSERT(_window != nullptr, "You must initialize ImGuiHelper before use!");

	// Implementation new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	// ImGui context new frame
	ImGui::NewFrame();

}

void ImGuiHelper::EndFrame() {
	LOG_ASSERT(_window != nullptr, "You must initialize ImGuiHelper before use!");

	// Make sure ImGui knows how big our window is
	ImGuiIO& io = ImGui::GetIO();
	int width{ 0 }, height{ 0 };
	glfwGetWindowSize(_window, &width, &height);
	io.DisplaySize = ImVec2((float)width, (float)height);

	// Render all of our ImGui elements
	ImGui::Render();
	ImDrawData* data = ImGui::GetDrawData();

	float L = data->DisplayPos.x;
	float R = data->DisplayPos.x + data->DisplaySize.x;
	float T = data->DisplayPos.y;
	float B = data->DisplayPos.y + data->DisplaySize.y;
	const float ortho_projection[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,                 0.0f,   0.0f },
		{ 0.0f,             2.0f / (T - B),       0.0f,   0.0f },
		{ 0.0f,             0.0f,                -1.0f,   0.0f },
		{ (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
	};
	glProgramUniformMatrix4fv(_linearDepthShader->GetHandle(), 0, 1, GL_FALSE, &ortho_projection[0][0]);
	glProgramUniformMatrix4fv(_arraySliceShader->GetHandle(), 0, 1, GL_FALSE, &ortho_projection[0][0]);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// If we have multiple viewports enabled (can drag into a new window)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// Update the windows that ImGui is using
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Restore our gl context
		glfwMakeContextCurrent(_window);
	}
}

