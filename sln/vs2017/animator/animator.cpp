/*
Experimental program to create an animated cubemap.
Does not animate due to lack of shader support on WindowImposter. There could be another shader that supports animated cubemaps.
*/
#include <iostream>

#include <VTFFile.h>
#include <VTFLib.h>

void PressKeyToContinue()
{
	int junk = getchar();
}

int main(int argc, char* argv[])
{
	const char* argv2[] = {"", "mpa30_cubemap.vtf", "sky_csgo_night02b_cubemap.vtf"};
	argv = (char**)argv2;
	argc = 3;

	int fileCount = argc - 1;

	vlByte** buffers = new vlByte*[fileCount * 7];

	vlUInt width;
	vlUInt height;

	for (int i = 0; i < fileCount; i++)
	{
		auto vtf = VTFLib::CVTFFile();
		vtf.Load(argv[i + 1]);
		if (vtf.IsLoaded() == false)
		{
			printf("error loading %s\n", argv[i + 1]);
			std::terminate();
		}
		width = vtf.GetWidth();
		height = vtf.GetHeight();
		
		for (int j = 0; j < 7; j++)
		{
			vlByte* buffer = (vlByte*)malloc(4 * width * height);
			buffers[i * 7 + j] = buffer;
			vtf.ConvertToRGBA8888(vtf.GetData(0, j, 0, 0), buffer, width, height, vtf.GetFormat());
		}
	}
	
	//vlByte* main_buffer = (vlByte*)malloc(4 * width * height * 7 * fileCount);
	auto output = VTFLib::CVTFFile();

	SVTFCreateOptions options;
	memset(&options, 0, sizeof(options));
	options.uiVersion[0] = 7;
	options.uiVersion[1] = 2;
	options.uiFlags = 0x0004 | 0x0008;
	options.ImageFormat = VTFImageFormat::IMAGE_FORMAT_DXT5;
	options.bThumbnail = true;

	printf("Building VTF\n");
	bool success;
	//success = output.Create(width, height, 7 * fileCount, 1, 1, (vlByte**)&buffers, options);
	success = output.Create(width, height, fileCount, 7, 1, IMAGE_FORMAT_RGBA8888, false, false, false);
	if (!success)
	{
		printf("Create Error %s\n", vlGetLastError());
		PressKeyToContinue();
		std::terminate();
	}

	output.SetFlag(tagVTFImageFlag::TEXTUREFLAGS_CLAMPS, true);
	output.SetFlag(tagVTFImageFlag::TEXTUREFLAGS_CLAMPT, true);
	output.SetFlag(tagVTFImageFlag::TEXTUREFLAGS_ENVMAP, true);
	output.SetFlag(tagVTFImageFlag::TEXTUREFLAGS_NOMIP, true);
	output.SetFlag(tagVTFImageFlag::TEXTUREFLAGS_NOLOD, true);

	for (int i = 0; i < fileCount; i++)
	{
		for (int j = 0; j < 7; j++)
		{
			auto data = buffers[i * 7 + j];
			output.SetData(i, j, 0, 0, data);
		}
	}

	char output_name[FILENAME_MAX];
	snprintf(output_name, sizeof(output_name), "animated.vtf");
	success = output.Save(output_name);
	if (!success)
	{
		printf("Save Error %s\n", vlGetLastError());
		PressKeyToContinue();
		std::terminate();
	}

	printf("done\n");
	PressKeyToContinue();
}