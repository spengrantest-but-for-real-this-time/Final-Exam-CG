#define GLM_SWIZZLE 
#include "Application/Application.h"

extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x01;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x01;
}

int main(int argc, char** args) { 
	Logger::Init();

	// TODO: parse arguments?

	Application::Start(argc, args);

	Logger::Uninitialize();
}