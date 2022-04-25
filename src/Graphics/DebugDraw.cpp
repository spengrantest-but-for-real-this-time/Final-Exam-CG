#include "Graphics/DebugDraw.h"

DebugDrawer::DebugDrawer() :
	_colorStack(std::stack<glm::vec3>()),
	_transformStack(std::stack<glm::mat4>()),
	_viewProjection(glm::mat4(1.0f)),
	_lineOffset(0),
	_triangleOffset(0)
{
	_linesVBO = VertexBuffer::Create(BufferUsage::DynamicDraw);
	_linesVBO->LoadData<VertexPosCol>(nullptr, LINE_BATCH_SIZE * 2);
	_linesVAO = VertexArrayObject::Create();
	_linesVAO->AddVertexBuffer(_linesVBO, VertexPosCol::V_DECL);

	_trisVBO = VertexBuffer::Create(BufferUsage::DynamicDraw);
	_trisVBO->LoadData<VertexPosCol>(nullptr, TRI_BATCH_SIZE * 3);
	_trisVAO = VertexArrayObject::Create();
	_trisVAO->AddVertexBuffer(_trisVBO, VertexPosCol::V_DECL);

	_colorStack.push(glm::vec3(1.0f));
	_transformStack.push(glm::mat4(1.0f));
}

void DebugDrawer::PushColor(const glm::vec3& color) {
	_colorStack.push(color);
}

glm::vec3 DebugDrawer::PopColor() {
	LOG_ASSERT(_colorStack.size() > 1, "Attempting to pop more colors than you are pushing! Check your code!");
	glm::vec3 result = _colorStack.top();
	_colorStack.pop();
	return result;
}

void DebugDrawer::PushWorldMatrix(const glm::mat4& value) {
	FlushAll();
	_transformStack.push(value);
}

void DebugDrawer::PopWorldMatrix() {
	LOG_ASSERT(_transformStack.size() > 1, "Attempting to pop more transforms than you are pushing! Check your code!");
	FlushAll();
	_transformStack.pop();

}

void DebugDrawer::DrawLine(const glm::vec3& p1, const glm::vec3& p2) {
	DrawLine(p1, p2, _colorStack.top(), _colorStack.top());
}

void DebugDrawer::DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& color) {
	DrawLine(p1, p2, color, color);
}

void DebugDrawer::DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& color1, const glm::vec3& color2)
{
	_lineBuffer[_lineOffset + 0].Color = glm::vec4(color1, 1.0f);
	_lineBuffer[_lineOffset + 0].Position = p1;
	_lineBuffer[_lineOffset + 1].Color = glm::vec4(color2, 1.0f);
	_lineBuffer[_lineOffset + 1].Position = p2;

	_lineOffset += 2;
	if (_lineOffset >= LINE_BATCH_SIZE) {
		FlushLines();
	}
}

void DebugDrawer::FlushLines()
{
	if (_lineOffset > 0) {
		__Shader->Bind();
		__Shader->SetUniformMatrix("u_MVP", _viewProjection * _transformStack.top());
		int restorePoint = 0;
		glLineWidth(2.0f);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &restorePoint);
		VertexArrayObject::Unbind();
		_linesVBO->LoadData<VertexPosCol>(_lineBuffer, LINE_BATCH_SIZE * 2);
		_linesVAO->Bind();
		glDrawArrays((GLenum)DrawMode::LineList, 0, _lineOffset);
		_linesVAO->Unbind();
		_lineOffset = 0;
		if (restorePoint != 0) {
			glBindVertexArray(restorePoint);
		}
	}
}

void DebugDrawer::DrawTri(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3) {
	glm::vec3 c = _colorStack.top();
	DrawTri(p1, p2, p3, c, c, c);
}

void DebugDrawer::DrawTri(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& color) {
	DrawTri(p1, p2, p3, color, color, color);
}

void DebugDrawer::DrawTri(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3)
{
	_triBuffer[_triangleOffset + 0].Color = glm::vec4(c1, 1.0f);
	_triBuffer[_triangleOffset + 0].Position = p1;
	_triBuffer[_triangleOffset + 1].Color = glm::vec4(c2, 1.0f);
	_triBuffer[_triangleOffset + 1].Position = p2;
	_triBuffer[_triangleOffset + 2].Color = glm::vec4(c3, 1.0f);
	_triBuffer[_triangleOffset + 2].Position = p3;

	_triangleOffset += 3;
	if (_triangleOffset >= TRI_BATCH_SIZE) {
		FlushTris();
	}
}

void DebugDrawer::FlushTris()
{
	if (_triangleOffset > 0) {
		__Shader->Bind();
		__Shader->SetUniformMatrix("u_MVP", _viewProjection * _transformStack.top());
		int restorePoint = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &restorePoint);
		VertexArrayObject::Unbind();
		_trisVBO->LoadData<VertexPosCol>(_triBuffer, TRI_BATCH_SIZE * 3);
		_trisVAO->Bind();
		glDrawArrays((GLenum)DrawMode::TriangleList, 0, _triangleOffset);
		_trisVAO->Unbind();
		_triangleOffset = 0;
		if (restorePoint != 0) {
			glBindVertexArray(restorePoint);
		}
	}
}

void DebugDrawer::DrawWireCircle(const glm::vec3& pos, const glm::vec3& n, float radius, int segments /*= 24*/)
{
	glm::vec3 norm = glm::normalize(n);

	glm::vec3 x = glm::vec3(-norm.y, norm.x, norm.z);
	if (glm::dot(x, norm) > 0.001f) {
		x = glm::vec3(-norm.z, norm.x, norm.y);
	}
	glm::vec3 y = glm::normalize(glm::cross(norm, x));

	float step = glm::two_pi<float>() / segments;
	for (int ix = 0; ix <= segments; ix++) {
		glm::vec3 p1 = pos + (glm::cos(ix * step) * x + glm::sin(ix * step) * y) * radius;
		glm::vec3 p2 = pos + (glm::cos((ix + 1) * step) * x + glm::sin((ix + 1) * step) * y) * radius;
		DrawLine(p1, p2);
	}
}

void DebugDrawer::DrawWireCube(const glm::vec3& center, const glm::vec3& halfExtents)
{
	float x1, x2, y1, y2, z1, z2;
	x1 = center.x - halfExtents.x;
	x2 = center.x + halfExtents.x;
	y1 = center.y - halfExtents.y;
	y2 = center.y + halfExtents.y;
	z1 = center.z - halfExtents.z;
	z2 = center.z + halfExtents.z;

	DrawLine(glm::vec3(x1, y1, z1), glm::vec3(x1, y1, z2));
	DrawLine(glm::vec3(x2, y1, z1), glm::vec3(x2, y1, z2));
	DrawLine(glm::vec3(x1, y2, z1), glm::vec3(x1, y2, z2));
	DrawLine(glm::vec3(x2, y2, z1), glm::vec3(x2, y2, z2));

	DrawLine(glm::vec3(x1, y1, z1), glm::vec3(x1, y2, z1));
	DrawLine(glm::vec3(x1, y1, z2), glm::vec3(x1, y2, z2));
	DrawLine(glm::vec3(x2, y1, z1), glm::vec3(x2, y2, z1));
	DrawLine(glm::vec3(x2, y1, z2), glm::vec3(x2, y2, z2));

	DrawLine(glm::vec3(x1, y1, z1), glm::vec3(x2, y1, z1));
	DrawLine(glm::vec3(x1, y1, z2), glm::vec3(x2, y1, z2));
	DrawLine(glm::vec3(x1, y2, z1), glm::vec3(x2, y2, z1));
	DrawLine(glm::vec3(x1, y2, z2), glm::vec3(x2, y2, z2));
}

void DebugDrawer::DrawWireCone(const glm::vec3& origin, const glm::vec3& extents, float angleDeg, int segments)
{
	float length = glm::length(extents);
	float radius = glm::tan(glm::radians(angleDeg)) * length;
	DrawWireCircle(origin + extents, glm::normalize(extents), radius, segments);

	glm::vec3 norm = glm::normalize(extents);

	glm::vec3 x = glm::vec3(-norm.y, norm.x, norm.z);
	if (glm::dot(x, norm) > 0.001f) {
		x = glm::vec3(-norm.z, norm.x, norm.y);
	}
	glm::vec3 y = glm::normalize(glm::cross(norm, x));

	float step = glm::two_pi<float>() / (segments / 2);
	for (int ix = 0; ix <= segments / 2; ix++) {
		glm::vec3 p1 = origin;
		glm::vec3 p2 = (origin + extents) + (glm::cos((ix + 1) * step) * x + glm::sin((ix + 1) * step) * y) * radius;
		DrawLine(p1, p2);
	}
}

void DebugDrawer::FlushAll()
{
	FlushLines();
	FlushTris();
}

void DebugDrawer::SetViewProjection(const glm::mat4& viewProjection)
{
	_viewProjection = viewProjection;
}

DebugDrawer& DebugDrawer::Get() {
	if (__Instance == nullptr) {
		__Instance = new DebugDrawer();

		const char* vs_source = R"LIT(#version 450
				layout (location = 0) in vec3 inPosition;
				layout (location = 1) in vec4 inColor;

				layout (location = 0) out vec4 outColor;

				layout (location = 0) uniform mat4 u_MVP;

				void main() {
					gl_Position = u_MVP * vec4(inPosition, 1.0);
					outColor = inColor;
				}
			)LIT";
		const char* fs_source = R"LIT(#version 450
				layout (location=0) in  vec4 inColor;
				layout (location=0) out vec4 outColor;

				void main() {
					outColor = inColor;
				}
			)LIT";

		__Shader = ShaderProgram::Create();
		__Shader->LoadShaderPart(vs_source, ShaderPartType::Vertex);
		__Shader->LoadShaderPart(fs_source, ShaderPartType::Fragment);
		__Shader->Link();
	}
	return *__Instance;
}

void DebugDrawer::Uninitialize()
{
	if (__Instance != nullptr) {
		delete __Instance;
		__Instance = nullptr;
		__Shader = nullptr;
	}
}