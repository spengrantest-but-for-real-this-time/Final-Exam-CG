#include "Texture2DArray.h"
#include <stb_image.h>
#include <Logging.h>
#include "GLM/glm.hpp"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/Base64.h"

/// <summary>
/// Get the number of mipmap levels required for a texture of the given size
/// </summary>
/// <param name="width">The width of the texture in pixels</param>
/// <param name="height">The height of the texture in pixels</param>
/// <returns>Number of mip levels required for the texture</returns>
inline int CalcRequiredMipLevels(int width, int height) {
	return (1 + floor(log2(glm::max(width, height))));
}

nlohmann::json Texture2DArray::ToJson() const {
	nlohmann::json result = {
		{ "wrap_s",  ~_description.HorizontalWrap },
		{ "wrap_t",  ~_description.VerticalWrap },
		{ "filter_min",       ~_description.MinificationFilter },
		{ "filter_mag",       ~_description.MagnificationFilter },
		{ "anisotropic",       _description.MaxAnisotropic },
		{ "generate_mipmaps",  _description.GenerateMipMaps },
		{ "x_split",           _description.XDivisions },
		{ "y_split",           _description.YDivisions },
	};

	if (!_description.Filename.empty()) {
		result["filename"] = _description.Filename;
	}
	else if (_pixelType != PixelType::Unknown) {
		result["size_x"] = _description.Width;
		result["size_y"] = _description.Width;

		result["format"] = ~_description.FormatHint;
		result["pixel_type"] = ~_pixelType;
		if (_description.Width * _description.Height * _description.XDivisions * _description.YDivisions > 0 && _description.FormatHint != PixelFormat::Unknown) {
			// TODO
			//size_t dataSize = GetTexelSize(_description.FormatHint, _pixelType) * _description.Width * _description.Height * _description.XDivisions * _description.YDivisions;
			//uint8_t* dataStore = new uint8_t[dataSize];
			//glGetTextureImage(_rendererId, 0, *_description.FormatHint, *_pixelType, dataSize, dataStore);
			//result["data"] = Base64::Encode(dataStore, dataSize);
		}
	}

	return result;
}

Texture2DArray::Sptr Texture2DArray::FromJson(const nlohmann::json& data)
{
	Texture2DArrayDescription descr = Texture2DArrayDescription();
	descr.Filename = data["filename"];
	descr.HorizontalWrap = JsonParseEnum(WrapMode, data, "wrap_s", WrapMode::ClampToEdge);
	descr.VerticalWrap   = JsonParseEnum(WrapMode, data, "wrap_t", WrapMode::ClampToEdge);
	descr.MinificationFilter  = JsonParseEnum(MinFilter, data, "filter_min", MinFilter::NearestMipNearest);
	descr.MagnificationFilter = JsonParseEnum(MagFilter, data, "filter_mag", MagFilter::Linear);
	descr.MaxAnisotropic      = JsonGet(data, "anisotropic", 0.0f);
	descr.GenerateMipMaps = JsonGet(data, "generate_mipmaps", false);
	descr.XDivisions = JsonGet(data, "x_split", descr.XDivisions);
	descr.YDivisions = JsonGet(data, "y_split", descr.YDivisions);

	Texture2DArray::Sptr result = std::make_shared<Texture2DArray>(descr);

	// If we embedded data into the JSON, load it now
	if (descr.Filename.empty() && data.contains("data") && data["data"].is_string()) {
		PixelType type = JsonParseEnum(PixelType, data, "pixel_type", PixelType::Unknown);
		try {
			size_t layers = descr.XDivisions * descr.YDivisions;
			std::string rawData = Base64::Decode(data['data'].get<std::string>());
			result->LoadData(descr.Width, descr.Height, layers, descr.FormatHint, type, rawData.data());
		}
		catch (std::runtime_error()) {
			LOG_WARN("JSON blob had data, but failed to load to texture");
		}
	}

	return result;
}

Texture2DArray::Texture2DArray(const Texture2DArrayDescription& description) :
	ITexture(TextureType::_2DArray),
	_description(description),
	_pixelType(PixelType::Unknown)
{
	_SetTextureParams();
	if (!description.Filename.empty()) {
		_LoadDataFromFile();
	}
}

Texture2DArray::Texture2DArray(const std::string& filePath, uint32_t splitX, uint32_t splitY) :
	ITexture(TextureType::_2DArray),
	_description(Texture2DArrayDescription()),
	_pixelType(PixelType::Unknown)
{
	_description.Filename = filePath;
	_description.XDivisions = splitX;
	_description.YDivisions = splitY;
	_SetTextureParams();
	_LoadDataFromFile();
}

int Texture2DArray::GetLevels() const {
	return _description.XDivisions * _description.YDivisions;
}

void Texture2DArray::SetMinFilter(MinFilter value) {
	_description.MinificationFilter = value;
	glTextureParameteri(_rendererId, GL_TEXTURE_MIN_FILTER, *_description.MinificationFilter);
}

void Texture2DArray::SetMagFilter(MagFilter value) {
	_description.MagnificationFilter = value;
	glTextureParameteri(_rendererId, GL_TEXTURE_MAG_FILTER, *_description.MagnificationFilter);
}

void Texture2DArray::SetAnisoLevel(float value) {
	if (value != _description.MaxAnisotropic) {
		_description.MaxAnisotropic = glm::clamp(value, 1.0f, ITexture::GetLimits().MAX_ANISOTROPY);
		glTextureParameterf(_rendererId, GL_TEXTURE_MAX_ANISOTROPY, _description.MaxAnisotropic);

		if (_description.GenerateMipMaps) {
			glGenerateTextureMipmap(_rendererId);
		}
	}
}

void Texture2DArray::LoadData(uint32_t width, uint32_t height, uint32_t layers, PixelFormat format, PixelType type, void* data, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ) {
	// Ensure the rectangle we're setting is within the bounds of the image
	LOG_ASSERT((width + offsetX) <= _description.Width, "Pixel bounds are outside of the X extents of the image!");
	LOG_ASSERT((height + offsetY) <= _description.Height, "Pixel bounds are outside of the Y extents of the image!");

	_description.FormatHint = format;
	_pixelType = type;

	// Align the data store to the size of a single component to ensure we don't get weirdness with images that aren't RGBA
	// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glPixelStore.xhtml
	int componentSize = (GLint)GetTexelComponentSize(type);
	glPixelStorei(GL_PACK_ALIGNMENT, componentSize);

	// Upload our data to our image
	glTextureSubImage3D(_rendererId, 0, offsetX, offsetY, offsetZ, width, height, layers, (GLenum)format, (GLenum)type, data);

	// If requested, generate mip-maps for our texture
	if (_description.GenerateMipMaps) {
		glGenerateTextureMipmap(_rendererId);
	}
}

void Texture2DArray::_LoadDataFromFile() {
	LOG_ASSERT(_description.Width + _description.Height == 0, "This texture has already been configured with a size! Cannot re-allocate memory!");

	if (!_description.Filename.empty()) {
		// Variables that will store properties about our image
		int width, height, numChannels;
		const int targetChannels = GetTexelComponentCount(_description.FormatHint);

		// Use STBI to load the image
		stbi_set_flip_vertically_on_load(true);
		uint8_t* data = stbi_load(_description.Filename.c_str(), &width, &height, &numChannels, targetChannels);

		// If we could not load any data, warn and return null
		if (data == nullptr) {
			LOG_WARN("STBI Failed to load image from \"{}\"", _description.Filename);
			return ;
		}

		if (width % _description.XDivisions != 0) {
			stbi_image_free(data);
			LOG_ERROR("Could not load image, X dimension not equal divisor");
			return;
		}
		if (height % _description.YDivisions != 0) {
			stbi_image_free(data);
			LOG_ERROR("Could not load image, Y dimension not equal divisor");
			return;
		}

		if (width * height == 0) {
			stbi_image_free(data);
			LOG_ERROR("Image empty, skipping");
			return;
		}

		// We should estimate a good format for our data

		// numChannels will store the number of channels in the image on disk, if we overrode that we should use the override value
		if (targetChannels != 0)
			numChannels = targetChannels;

		// We'll determine a recommended format for the image based on number of channels
		// We hinted that we wanted a certain number of channels, but we're not guaranteed
		// that all those channels exist (ex: loading an RGB image but requesting RGBA)
		InternalFormat internal_format = GetInternalFormatForChannels8(numChannels);
		PixelFormat    image_format = GetPixelFormatForChannels(numChannels);

		// This is one of those poorly documented things in OpenGL
		if ((numChannels * width) % 4 != 0) {
			LOG_WARN("The alignment of a horizontal line is not a multiple of 4, this will require a call to glPixelStorei(GL_PACK_ALIGNMENT)");
		}

		// Update our description to match what we loaded
		_description.Format = internal_format;
		_description.Width = width;
		_description.Height = height;

		// Allocates our memory
		_SetTextureParams();


		uint64_t layers = _description.XDivisions * _description.YDivisions;
		uint64_t xSize = width / _description.XDivisions;
		uint64_t ySize = height / _description.YDivisions;

		size_t texelSize = GetTexelSize(_description.FormatHint, PixelType::UByte);

		uint64_t size = xSize * ySize;

		uint8_t* repack = (uint8_t*)malloc(width * height * texelSize);

		// We need to remap our 2D image to 3D space
		uint64_t sourceLoc{ 0 }, xLoc{ 0 }, yLoc{ 0 };
		for (uint64_t iz = 0; iz < layers; iz++) {
			for (uint64_t iy = 0; iy < ySize; iy++) {
				for (uint64_t ix = 0; ix < xSize; ix++) {
					xLoc = (iz % _description.XDivisions) * xSize + ix;
					yLoc = (iz / _description.YDivisions) * ySize + iy;
					memcpy(
						repack + (iz * size + iy * xSize + ix) * texelSize,
						data + (yLoc * width + xLoc) * texelSize,
						texelSize
					);
				}
			}
		}

		_CrtCheckMemory();

		// Upload data to our texture
		LoadData(xSize, ySize, layers, image_format, PixelType::UByte, repack);

		free(repack);

		// We now have data in the image, we can clear the STBI data
		stbi_image_free(data);
	}
	
	SetDebugName(_description.Filename);
}

void Texture2DArray::_SetTextureParams() {
	// If the anisotropy is negative, we assume that we want max anisotropy
	if (_description.MaxAnisotropic < 0.0f) {
		_description.MaxAnisotropic = ITexture::GetLimits().MAX_ANISOTROPY;
	}

	// Make sure the size is greater than zero and that we have a format specified before trying to set parameters
	if ((_description.Width * _description.Height * _description.XDivisions * _description.YDivisions > 0) && _description.Format != InternalFormat::Unknown) {

		size_t sliceWidth = _description.Width / _description.XDivisions;
		size_t sliceHeight = _description.Height / _description.YDivisions;

		// Calculate how many layers of storage to allocate based on whether mipmaps are enabled or not
		int layers = _description.GenerateMipMaps ? CalcRequiredMipLevels(sliceWidth, sliceHeight) : 1;
		// Allocates the memory for our texture
		glTextureStorage3D(_rendererId, layers, (GLenum)_description.Format, sliceWidth, sliceHeight, _description.XDivisions * _description.YDivisions);

		glTextureParameteri(_rendererId, GL_TEXTURE_MIN_FILTER, (GLenum)_description.MinificationFilter);
		glTextureParameteri(_rendererId, GL_TEXTURE_MAG_FILTER, (GLenum)_description.MagnificationFilter);
		glTextureParameterf(_rendererId, GL_TEXTURE_MAX_ANISOTROPY, _description.MaxAnisotropic);

		glTextureParameteri(_rendererId, GL_TEXTURE_WRAP_S, (GLenum)_description.HorizontalWrap);
		glTextureParameteri(_rendererId, GL_TEXTURE_WRAP_T, (GLenum)_description.VerticalWrap);
	}
}

Texture2DArray::Sptr Texture2DArray::LoadFromFile(const std::string& path, const Texture2DArrayDescription& description, bool forceRgba) {
	// Create a copy of the description and change filename to the path
	Texture2DArrayDescription desc = description;
	desc.Filename = path;

	// Create a texture from the description (it'll load the file)
	Texture2DArray::Sptr result = std::make_shared<Texture2DArray>(desc);

	return result;
}