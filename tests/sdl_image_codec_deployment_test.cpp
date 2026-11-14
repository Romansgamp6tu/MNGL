#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
	void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
	{
		bytes.push_back(static_cast<std::uint8_t>(value));
		bytes.push_back(static_cast<std::uint8_t>(value >> 8));
	}

	void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
	{
		append_u16(bytes, static_cast<std::uint16_t>(value));
		append_u16(bytes, static_cast<std::uint16_t>(value >> 16));
	}

	void append_entry(std::vector<std::uint8_t>& bytes, std::uint16_t tag, std::uint16_t type,
	                   std::uint32_t count, std::uint32_t value)
	{
		append_u16(bytes, tag);
		append_u16(bytes, type);
		append_u32(bytes, count);
		append_u32(bytes, value);
	}

	std::vector<std::uint8_t> make_one_pixel_tiff()
	{
		std::vector<std::uint8_t> bytes{'I', 'I'};
		append_u16(bytes, 42);
		append_u32(bytes, 8);

		append_u16(bytes, 10);
		append_entry(bytes, 256, 4, 1, 1);   // ImageWidth
		append_entry(bytes, 257, 4, 1, 1);   // ImageLength
		append_entry(bytes, 258, 3, 1, 8);   // BitsPerSample
		append_entry(bytes, 259, 3, 1, 1);   // Compression: none
		append_entry(bytes, 262, 3, 1, 1);   // PhotometricInterpretation: BlackIsZero
		append_entry(bytes, 273, 4, 1, 134); // StripOffsets
		append_entry(bytes, 277, 3, 1, 1);   // SamplesPerPixel
		append_entry(bytes, 278, 4, 1, 1);   // RowsPerStrip
		append_entry(bytes, 279, 4, 1, 1);   // StripByteCounts
		append_entry(bytes, 284, 3, 1, 1);   // PlanarConfiguration
		append_u32(bytes, 0);                // No next IFD
		bytes.push_back(0x7f);                // One grayscale pixel
		return bytes;
	}
} // namespace

int main()
{
	if (!SDL_Init(0))
	{
		std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
		return 1;
	}

	const auto tiff = make_one_pixel_tiff();
	SDL_IOStream* stream = SDL_IOFromConstMem(tiff.data(), tiff.size());
	if (!stream)
	{
		std::cerr << "SDL_IOFromConstMem failed: " << SDL_GetError() << '\n';
		SDL_Quit();
		return 1;
	}

	SDL_Surface* surface = IMG_LoadTyped_IO(stream, true, "TIF");
	if (!surface)
	{
		std::cerr << "IMG_LoadTyped_IO(TIF) failed: " << SDL_GetError() << '\n';
		SDL_Quit();
		return 1;
	}

	const bool decoded = surface->w == 1 && surface->h == 1;
	SDL_DestroySurface(surface);
	SDL_Quit();
	if (!decoded)
	{
		std::cerr << "TIFF decoded with unexpected dimensions\n";
		return 1;
	}
	return 0;
}
