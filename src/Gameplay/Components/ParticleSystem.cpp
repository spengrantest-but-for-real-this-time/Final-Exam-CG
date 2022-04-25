#include "ParticleSystem.h"
#include "Utils/JsonGlmHelpers.h"
#include "Application/Timing.h"
#include "Application/Application.h"
#include "Utils/ImGuiHelper.h"
#include "Graphics/DebugDraw.h"
#include "imgui_internal.h"

ParticleSystem::ParticleSystem() :
	IComponent(),
	_hasInit(false),
	_maxParticles(1000),
	_numParticles(0),
	_particleBuffers(),
	_feedbackBuffers(),
	_query(0),
	_currentVertexBuffer(0),
	_currentFeedbackBuffer(1),
	_updateShader(nullptr),
	_renderShader(nullptr),
	_gravity({ 0, 0, -9.81f }),
	_emitters(),
	_needsUpload(true),
	_needsResize(false)
{ }

ParticleSystem::~ParticleSystem()
{
	if (_hasInit) {
		glDeleteBuffers(2, _particleBuffers);
		glDeleteTransformFeedbacks(2, _feedbackBuffers);
		glDeleteQueries(1, &_query);
		_updateShader = nullptr;
		_renderShader = nullptr;
	}
}

void ParticleSystem::Update()
{
	// If we haven't previously initialized our data, initialize it now
	if (!_hasInit) {
		_updateShader->Bind();

		int i = 0;
		glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, &i);

		// We essentially use double buffering, hence the 2 buffers
		glCreateTransformFeedbacks(2, _feedbackBuffers);
		glCreateBuffers(2, _particleBuffers);
		glCreateVertexArrays(2, _updateVaos);
		glCreateVertexArrays(2, _renderVaos);

		size_t dataSize = (_maxParticles + _emitters.size()) * sizeof(ParticleData);

		for (int ix = 0; ix < 2; ix++) {
			glBindVertexArray(_updateVaos[ix]);

			// Set up our first transform feedback buffer to write to the first buffer
			glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _feedbackBuffers[ix]);
			glBindBuffer(GL_ARRAY_BUFFER, _particleBuffers[ix]);
			glBufferData(GL_ARRAY_BUFFER, dataSize, nullptr, GL_DYNAMIC_DRAW);
			glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, _particleBuffers[ix]);

			// Enable our attributes
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glEnableVertexAttribArray(3);
			glEnableVertexAttribArray(4);
			glEnableVertexAttribArray(5);
			glEnableVertexAttribArray(6);
			glEnableVertexAttribArray(7);

			glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Type)); // type
			glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, TexID)); // tex ID
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Position)); // position
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Velocity)); // velocity
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Color)); // color 
			glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Lifetime)); // metadata 
			glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Metadata)); // metadata 
			glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Metadata2)); // metadata 


			glBindVertexArray(_renderVaos[ix]);
			glBindBuffer(GL_ARRAY_BUFFER, _particleBuffers[ix]);

			// Enable type, position and color 
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glEnableVertexAttribArray(4); 
			glEnableVertexAttribArray(6);
			glEnableVertexAttribArray(7);
			glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Type)); // type
			glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, TexID)); // type
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Position)); // position
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Color)); // color 
			glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Metadata)); // metadata 
			glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, Metadata2)); // metadata 
		}

		glBindVertexArray(0);


		// We create a query object to track the number of particles we're simulating
		glGenQueries(1, &_query);
	}

	if (_needsResize) {
		size_t dataSize = (_maxParticles + _emitters.size()) * sizeof(ParticleData);
		glNamedBufferData(_particleBuffers[0], dataSize, nullptr, GL_DYNAMIC_DRAW);
		glNamedBufferData(_particleBuffers[1], dataSize, nullptr, GL_DYNAMIC_DRAW);
		_needsUpload = true;
		_needsResize = false;
	}

	if (_needsUpload) {
		glBindVertexArray(0);

		// Allocate some temp space for particles, so we can init the emitters
		size_t dataSize = (_emitters.size()) * sizeof(ParticleData);
		ParticleData* data = new ParticleData[_emitters.size()];
		memset(data, 0, dataSize);

		// Add all emitter to the the particle list at the beginning
		for (int ix = 0; ix < _emitters.size(); ix++) {
			data[ix] = _emitters[ix];
		}

		// Update just the first few elements, we'll restart transform feedback with it
		for (int ix = 0; ix < 2; ix++) {
			glNamedBufferSubData(_particleBuffers[ix], 0, dataSize, data);
		}

		// We no longer need the CPU copy
		delete[] data;
	}

	// Disable rasterization, this is update only
	glEnable(GL_RASTERIZER_DISCARD);

	// Bind the update shader and send our relevant uniforms
	_updateShader->Bind();
	_updateShader->SetUniform("u_Gravity", _gravity); 
	_updateShader->SetUniformMatrix("u_ModelMatrix", GetGameObject()->GetTransform()); 

	glBindVertexArray(_updateVaos[_currentVertexBuffer]);

	// Bind the buffer and transform feedback
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _feedbackBuffers[_currentFeedbackBuffer]);

	// Our particles are points that we're simulating
	glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, _query); 
	glBeginTransformFeedback(GL_POINTS);

	// If this is our first pass, or we have fresh emitter data, we use drawArrays 
	// to get the initial state, otherwise we use transform feedback for updating
	if (!_hasInit || _needsUpload ) {
		glDrawArrays(GL_POINTS, 0, _emitters.size());
	}
	else {
		glDrawTransformFeedback(GL_POINTS, _feedbackBuffers[_currentVertexBuffer]);
	}

	// End of transform feedback
	glEndTransformFeedback();
	glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN); 

	// Use our query to get the number of particles
	glGetQueryObjectuiv(_query, GL_QUERY_RESULT, &_numParticles);
	if (_numParticles >= _emitters.size()) {
		_numParticles -= _emitters.size();
	}
	else {
		_numParticles = 0;
	}

	// Clean up our state
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	glBindVertexArray(0);

	// Re-enable rasterization for later OpenGL calls
	glDisable(GL_RASTERIZER_DISCARD);

	_hasInit = true;
	_needsUpload = false;

	// Double-buffering, swap which buffers we're operating on
	_currentVertexBuffer = _currentFeedbackBuffer;
	_currentFeedbackBuffer = (_currentFeedbackBuffer + 1) & 0x01;
}

void ParticleSystem::Render()
{
	// Make sure that we've actually initialized our stuff
	if (_hasInit) {

		if (Atlas != nullptr) {
			Atlas->Bind(0);
		}

		// We're using our particle rendering shader
		_renderShader->Bind();

		// Make sure no VAOs are bound
		glBindVertexArray(_renderVaos[_currentVertexBuffer]);

		//glDisable(GL_DEPTH_TEST);
		
		glDisable(GL_BLEND);
		glEnablei(GL_BLEND, 0);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(false);
		glEnable(GL_DEPTH_TEST);

		// Bind the current feedback buffer as our drawing buffer
		glBindBuffer(GL_ARRAY_BUFFER, _particleBuffers[_currentVertexBuffer]); 

		// Draw our particles using whatever data we have in transform feedback buffer
		glDrawTransformFeedback(GL_POINTS, _feedbackBuffers[_currentVertexBuffer]);

		glBindVertexArray(0);

		glEnable(GL_DEPTH_TEST);
	}
}

void ParticleSystem::Reset() {
	_needsUpload = true;
}

void ParticleSystem::SetMaxParticles(uint32_t value)
{
	_maxParticles = value;
	_needsUpload = true;
	_needsResize = true;
}

uint32_t ParticleSystem::GetMaxParticles() const {
	return _maxParticles;
}

void ParticleSystem::AddEmitter(const ParticleData& emitter)
{
	_emitters.push_back(emitter); 
	_needsUpload = true;
}

void ParticleSystem::RenderImGui()
{
	LABEL_LEFT(ImGui::LabelText, "Particle Count", "%u", _numParticles);

	Application& app = Application::Get();

	LABEL_LEFT(ImGui::DragFloat3, "Gravity", &_gravity.x, 0.01f);
	uint32_t minParticles = _emitters.size();
	_needsResize |= LABEL_LEFT(ImGui::DragScalarN, "Max Particles", ImGuiDataType_U32, &_maxParticles, 1, 10.0f, &minParticles);

	ImGui::Separator();
	ImGui::Text("Emitters:");

	if (Atlas == nullptr) {
		if (ImGui::Button("Load default atlas")) {
			Atlas = std::make_shared<Texture2DArray>("textures/particles.png", 2, 2);
		}
	}


	// We can't add or edit emitters once the system has started
	for (int ix = 0; ix < _emitters.size(); ix++) {
		auto& emitter = _emitters[ix];

		ImGui::PushID(&emitter);
		static char buffer[255];
		sprintf_s(buffer, "%s###Emitter", (~emitter.Type).c_str());
		ImGuiID id = ImGui::GetID(buffer);
		bool open = ImGui::CollapsingHeader(buffer, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton);

		int delta = 0;
		if (ImGuiHelper::HeaderMoveButtons(id, &delta)) {
			if (delta < 0 && ix > 0) {
				ParticleData temp = _emitters[ix - 1];
				_emitters[ix - 1] = _emitters[ix];
				_emitters[ix] = temp;
				ImGui::PopID();
				_needsUpload = true;
				break;
			}
			else if (delta > 0 && ix < _emitters.size() - 1) {
				ParticleData temp = _emitters[_emitters.size() - 1];
				_emitters[_emitters.size() - 1] = _emitters[ix];
				_emitters[ix] = temp;
				ImGui::PopID();
				_needsUpload = true;
				break;
			}
		}
	
		if (open) {

			_needsUpload |= LABEL_LEFT(ImGui::DragFloat3, "Position  ", &emitter.Position.x, 0.1f);
			_needsUpload |= LABEL_LEFT(ImGui::ColorEdit4, "Color     ", &emitter.Color.x);

			ImGui::Columns(4);

			glm::vec2 size = glm::vec2(ImGui::GetContentRegionAvailWidth());

			if (Atlas != nullptr) {
				for (int ix = 0; ix < Atlas->GetLevels(); ix++) {
					ImGui::PushID(ix);
					ImGuiHelper::DrawTextureArraySlice(Atlas, ix, size, ix == emitter.TexID ? ImVec4(1, 0, 1, 1) : ImVec4(0, 0, 0, 1));
					if (ImGui::IsItemClicked()) {
						emitter.TexID = ix;
						_needsUpload = true;
					}
					ImGui::PopID();
					ImGui::NextColumn();
				}
			}
			ImGui::Columns(1);

			switch (emitter.Type)
			{
			case ParticleType::StreamEmitter: 
				{
					float spawnRate = 1.0f / emitter.StreamEmitterData.Timer;
					if (LABEL_LEFT(ImGui::DragFloat, "Spawn Rate", &spawnRate, 0.1f, 0.1f)) {
						emitter.Lifetime = 1.0f / spawnRate;
						emitter.StreamEmitterData.Timer = emitter.Lifetime;
						_needsUpload = true;
					}

					_needsUpload |= LABEL_LEFT(ImGui::DragFloat3, "Velocity  ", &emitter.StreamEmitterData.Velocity.x, 0.01f);
					_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Size      ", &emitter.StreamEmitterData.SizeRange.x, 0.1f, 0.01f);
					_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Lifetime  ", &emitter.StreamEmitterData.LifeRange.x, 0.1f, 0.0f);

					glm::vec4 pos4 = GetGameObject()->GetTransform() * glm::vec4(emitter.Position, 1.0f);
					glm::vec3 pos = pos4 / pos4.w;
					glm::vec3 p2 = pos + emitter.StreamEmitterData.Velocity;
					DebugDrawer::Get().DrawLine(pos, p2);
				}
				break;
			case ParticleType::SphereEmitter:
				{
					float spawnRate = 1.0f / emitter.SphereEmitterData.Timer;
					if (LABEL_LEFT(ImGui::DragFloat, "Spawn Rate", &spawnRate, 0.1f, 0.1f)) {
						emitter.Lifetime = 1.0f / spawnRate;
						emitter.SphereEmitterData.Timer = emitter.Lifetime;
						_needsUpload = true;
					}

					_needsUpload |= LABEL_LEFT(ImGui::DragFloat,  "Velocity  ", &emitter.SphereEmitterData.Velocity, 0.01f);
					_needsUpload |= LABEL_LEFT(ImGui::DragFloat,  "Radius    ", &emitter.SphereEmitterData.Radius, 0.01f);
					_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Size      ", &emitter.SphereEmitterData.SizeRange.x, 0.1f, 0.01f);
					_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Lifetime  ", &emitter.SphereEmitterData.LifeRange.x, 0.1f, 0.0f);

					glm::vec4 pos4 = GetGameObject()->GetTransform() * glm::vec4(emitter.Position, 1.0f);
					glm::vec3 pos = pos4 / pos4.w;
					DebugDrawer::Get().DrawWireCircle(pos, glm::vec3(1.0f, 0.0f, 0.0f), emitter.SphereEmitterData.Radius);
					DebugDrawer::Get().DrawWireCircle(pos, glm::vec3(0.0f, 1.0f, 0.0f), emitter.SphereEmitterData.Radius);
					DebugDrawer::Get().DrawWireCircle(pos, glm::vec3(0.0f, 0.0f, 1.0f), emitter.SphereEmitterData.Radius);
				}
				break;
			case ParticleType::BoxEmitter:
			{
				float spawnRate = 1.0f / emitter.BoxEmitterData.Timer;
				if (LABEL_LEFT(ImGui::DragFloat, "Spawn Rate", &spawnRate, 0.1f, 0.1f)) {
					emitter.Lifetime = 1.0f / spawnRate;
					emitter.BoxEmitterData.Timer = emitter.Lifetime;
					_needsUpload = true;
				}

				_needsUpload |= LABEL_LEFT(ImGui::DragFloat3, "Velocity  ", &emitter.BoxEmitterData.Velocity.x, 0.01f);
				_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Size      ", &emitter.BoxEmitterData.SizeRange.x, 0.1f, 0.01f);
				_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Lifetime  ", &emitter.BoxEmitterData.LifeRange.x, 0.1f, 0.0f);
				_needsUpload |= LABEL_LEFT(ImGui::DragFloat3, "H. Extents", &emitter.BoxEmitterData.HalfExtents.x, 0.1f, 0.0f);

				glm::vec4 pos4 = GetGameObject()->GetTransform() * glm::vec4(emitter.Position, 1.0f);
				glm::vec3 pos = pos4 / pos4.w;
				DebugDrawer::Get().DrawWireCube(pos, emitter.BoxEmitterData.HalfExtents);
			}
			break;
			case ParticleType::ConeEmitter:
			{
				float spawnRate = 1.0f / emitter.ConeEmitterData.Timer;
				if (LABEL_LEFT(ImGui::DragFloat, "Spawn Rate", &spawnRate, 0.1f, 0.1f)) {
					emitter.Lifetime = 1.0f / spawnRate;
					emitter.ConeEmitterData.Timer = emitter.Lifetime;
					_needsUpload = true;
				}

				_needsUpload |= LABEL_LEFT(ImGui::DragFloat3, "Velocity  ", &emitter.ConeEmitterData.Velocity.x, 0.01f);
				float angleDeg = glm::degrees(emitter.ConeEmitterData.Angle);
				if (LABEL_LEFT(ImGui::DragFloat, "Angle     ", &angleDeg, 0.01f)) {
					emitter.ConeEmitterData.Angle = glm::radians(angleDeg);
					_needsUpload = true;
				}
				_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Size      ", &emitter.ConeEmitterData.SizeRange.x, 0.1f, 0.01f);
				_needsUpload |= LABEL_LEFT(ImGui::DragFloat2, "Lifetime  ", &emitter.ConeEmitterData.LifeRange.x, 0.1f, 0.0f);

				glm::vec4 pos4 = GetGameObject()->GetTransform() * glm::vec4(emitter.Position, 1.0f);
				glm::vec3 pos = pos4 / pos4.w;
				glm::vec3 dir = glm::mat3(GetGameObject()->GetTransform()) * emitter.ConeEmitterData.Velocity;
				DebugDrawer::Get().DrawWireCone(pos, dir, glm::degrees(emitter.ConeEmitterData.Angle));
			}
			break;

			default:
				ImGui::Text("Unknown emitter type or no editor available");
				break;
			}

			if (ImGuiHelper::WarningButton("Delete")) {
				_emitters.erase(_emitters.begin() + ix);
				ix--;
				_needsUpload = true;
			}
		}

		ImGui::PopID();
	}

	ImGui::Separator();
	const char* ComboOptions = "Stream Emitter\0Sphere Emitter\0Box Emitter\0Cone Emitter\0";

	int i = ImGui::GetStateStorage()->GetInt(ImGui::GetID("add_emitter"), 0);
	if (ImGui::Combo("", &i, ComboOptions)) {
		ImGui::GetStateStorage()->SetInt(ImGui::GetID("add_emitter"), i);
	}
	ImGui::SameLine();
	if (ImGui::Button("Add")) {
		ParticleData emitter;
		emitter.TexID      = 0;
		emitter.Position   = glm::vec3(0.0f);
		emitter.Color      = glm::vec4(1.0f);
		emitter.Lifetime   = 1.0f;
		emitter.Metadata.x = 1.0f;

		if (i == 0) {
			emitter.Type                        = ParticleType::StreamEmitter;
			emitter.StreamEmitterData.Velocity  = glm::vec3(0.0f);
			emitter.StreamEmitterData.LifeRange = { 1.0f, 1.0f };
			emitter.StreamEmitterData.SizeRange = { 1.0f, 1.0f };
			_emitters.push_back(emitter);
			_needsUpload = true;
		}

		else if (i == 1) {
			emitter.Type                        = ParticleType::SphereEmitter;
			emitter.SphereEmitterData.Velocity  = 0.0f;
			emitter.SphereEmitterData.Radius    = 1.0f;
			emitter.SphereEmitterData.Timer     = 1.0f;
			emitter.SphereEmitterData.SizeRange = { 1.0f, 1.0f };
			emitter.SphereEmitterData.LifeRange = { 1.0f, 1.0f };
			_emitters.push_back(emitter);
			_needsUpload = true;
		}

		else if (i == 2) {
			emitter.Type                       = ParticleType::BoxEmitter;
			emitter.BoxEmitterData.Velocity    = { 1.0f, 1.0f, 1.0f };
			emitter.BoxEmitterData.LifeRange   = { 1.0f, 1.0f };
			emitter.BoxEmitterData.SizeRange   = { 1.0f, 1.0f };
			emitter.BoxEmitterData.HalfExtents = { 1.0f, 1.0f, 1.0f };
			_emitters.push_back(emitter);
			_needsUpload = true;
		}
		else if (i == 3) {
			emitter.Type = ParticleType::ConeEmitter;
			emitter.ConeEmitterData.Velocity  = glm::vec3(0.0f, 0.0f, 1.0f);
			emitter.ConeEmitterData.Angle     = glm::radians(30.0f);
			emitter.ConeEmitterData.LifeRange = { 1.0f, 1.0f };
			emitter.ConeEmitterData.SizeRange = { 1.0f, 1.0f };
			_emitters.push_back(emitter);
			_needsUpload = true;
		}
	}

}

void ParticleSystem::OnLoad()
{
	// There are the things we want the feedback buffers to track
	const char const* varyings[8] = {
		"out_Type",
		"out_TexID",
		"out_Position",
		"out_Color",
		"out_Lifetime",
		"out_Velocity",
		"out_Metadata",
		"out_Metadata2"
	};

	// This is our transform feedback shader
	_updateShader = ShaderProgram::Create();
	_updateShader->LoadShaderPartFromFile("shaders/vertex_shaders/particles_sim_vs.glsl", ShaderPartType::Vertex);
	_updateShader->LoadShaderPartFromFile("shaders/geometry_shaders/particle_sim_gs.glsl", ShaderPartType::Geometry);
	_updateShader->RegisterVaryings(varyings, 8, true); // Here we call glTransformFeedbackVaryings, and let it know we want interleaved data
	_updateShader->Link();

	// This shader will render the particles
	_renderShader = ShaderProgram::Create();
	_renderShader->LoadShaderPartFromFile("shaders/vertex_shaders/particles_render_vs.glsl", ShaderPartType::Vertex);
	_renderShader->LoadShaderPartFromFile("shaders/geometry_shaders/particle_render_gs.glsl", ShaderPartType::Geometry);
	_renderShader->LoadShaderPartFromFile("shaders/fragment_shaders/particles_render_fs.glsl", ShaderPartType::Fragment);
	_renderShader->Link();
}

void ParticleSystem::Awake() 
{

	_needsUpload = true;
}

nlohmann::json ParticleSystem::ToJson() const {
	nlohmann::json result = {
		{ "gravity", _gravity },
		{ "max_particles", _maxParticles },
		{ "atlas", Atlas ? Atlas->GetGUID().str() : "null" }
	};

	std::vector<float> metaData;
	metaData.resize(4 + 4 + 3);

	// Add emitters to the JSON data
	result["emitters"] = nlohmann::json();
	for (const auto& emitter : _emitters) {
		memcpy(metaData.data(), emitter.EmitterData, sizeof(float) * (4 + 4 + 3));
		nlohmann::json blob = {
			{ "type", ~emitter.Type },
			{ "tex_id", emitter.TexID },
			{ "position", emitter.Position },
			{ "color", emitter.Color },
			{ "spawn_rate", emitter.Lifetime },
			{ "meta", metaData },
		};
		result["emitters"].push_back(blob);
	}

	return result;
}

ParticleSystem::Sptr ParticleSystem::FromJson(const nlohmann::json& blob) {
	ParticleSystem::Sptr result = std::make_shared<ParticleSystem>();

	result->_gravity = JsonGet(blob, "gravity", result->_gravity);
	result->_maxParticles = JsonGet(blob, "max_particled", result->_maxParticles);
	result->Atlas = ResourceManager::Get<Texture2DArray>(Guid(JsonGet<std::string>(blob, "atlas", "null")));

	const float DEFAULT_META[4 + 4 + 3] = {
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f
	};

	if (blob.contains("emitters") && blob["emitters"].is_array()) {
		for (const auto& data : blob["emitters"]) {
			ParticleData emitter;
			emitter.Type     = JsonParseEnum(ParticleType, blob, "type", ParticleType::SphereEmitter);
			emitter.TexID    = JsonGet(data, "tex_id", 0);
			emitter.Position = JsonGet(data, "position", glm::vec3(0.0f));
			emitter.Color    = JsonGet(data, "color", glm::vec4(1.0f));
			emitter.Lifetime = JsonGet(data, "spawn_rate", 1.0f);

			// Extracting metadata as a float array
			memcpy(emitter.EmitterData, DEFAULT_META, sizeof(float) * (4 + 4 + 3));
			std::vector<float> meta = JsonGet(data, "meta", std::vector<float>());
			memcpy(emitter.EmitterData, meta.data(), std::min(meta.size(), 4ull + 4 + 3) * sizeof(float));

			result->_emitters.push_back(emitter);
		}
	}

	return result;
}
