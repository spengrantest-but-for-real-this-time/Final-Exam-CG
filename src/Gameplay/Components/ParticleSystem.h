#pragma once
#include "Gameplay/Components/IComponent.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2DArray.h"

ENUM(ParticleType, uint32_t,
	StreamEmitter = 0,
	SphereEmitter = 1,
	BoxEmitter    = 2,
	ConeEmitter   = 3,
	Particle      = 1 << 17
);

class ParticleSystem : public Gameplay::IComponent{
public:
	MAKE_PTRS(ParticleSystem);

	glm::vec3 _gravity;

	struct ParticleData {
		ParticleType Type;     // uint32_t, lower 16 bits for emitters, upper 16 for particles
		uint32_t     TexID;
		glm::vec3    Position;
		glm::vec4    Color;
		float        Lifetime; // For emitters, this is the time to next particle spawn

		union {
			float EmitterData[3 + 4 + 4];
			struct {
				glm::vec3    Velocity; // For emitters, this is initial velocity
				// For emitters, x is time to next particle, y is max deviation from direction in radians, z-w is lifetime range
				glm::vec4    Metadata;
				glm::vec4    Metadata2;
			};
			struct {
				glm::vec3 Velocity;
				float Timer;
				float Padding;
				glm::vec2 LifeRange;
				glm::vec2 SizeRange;
				glm::vec2 Padding2;
			} StreamEmitterData;
			struct {
				float Velocity;
				float Radius;
				float Padding;
				float Timer;
				float Padding2;
				glm::vec2 LifeRange;
				glm::vec2 SizeRange;
				glm::vec2 Padding3;
			} SphereEmitterData;
			struct {
				glm::vec3 Velocity;
				float Timer;
				glm::vec2 SizeRange;
				glm::vec2 LifeRange;
				glm::vec3 HalfExtents;
			} BoxEmitterData;
			struct {
				glm::vec3 Velocity;
				float Timer;
				float Angle; // In radians
				glm::vec2 LifeRange;
				glm::vec2 SizeRange;
				glm::vec2 Padding2;
			} ConeEmitterData;
		};
	};

	ParticleSystem();
	~ParticleSystem();

	void Update();
	void Render();

	void Reset();

	void SetMaxParticles(uint32_t value);
	uint32_t GetMaxParticles() const;

	Texture2DArray::Sptr Atlas;

	void AddEmitter(const ParticleData& emitter);

	// Inherited from IComponent

	virtual void RenderImGui() override;
	virtual void OnLoad() override;
	virtual void Awake() override;
	virtual nlohmann::json ToJson() const override;
	static ParticleSystem::Sptr FromJson(const nlohmann::json& blob);
	MAKE_TYPENAME(ParticleSystem);

protected:

	bool _hasInit;
	bool _needsUpload;
	bool _needsResize;

	uint32_t _maxParticles;
	GLuint _numParticles;

	uint32_t _particleBuffers[2];
	uint32_t _feedbackBuffers[2];
	uint32_t _updateVaos[2];
	uint32_t _renderVaos[2];
	uint32_t _query;

	uint32_t _currentVertexBuffer;
	uint32_t _currentFeedbackBuffer;

	ShaderProgram::Sptr _updateShader;
	ShaderProgram::Sptr _renderShader;

	std::vector<ParticleData> _emitters;
};