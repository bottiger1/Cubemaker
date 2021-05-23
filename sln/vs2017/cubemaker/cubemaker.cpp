#include <iostream>

#include <VTFFile.h>
#include <VTFLib.h>
#include "half.h"

const char* g_banner = "Skybox to Cubemap Maker by Bottiger skial.com\n\n";

const char* g_usage = \
R"(Converts a group of skybox VTFs to a cubemap VTF that can be applied to a box around a sky_camera.
This will allow you to use multiple skyboxes in TF2 by creating multiple brushes with this texture that 
you can hide and show at any time.

Drag one of the files ending in ft,bk,rt,lf,up,dn onto the program. 

All 6 of these VTF files must be in the same directory. Sometimes the dn VTF is missing. Open the VMT 
file with the same name, find the VTF it points to, copy and paste it into the same folder as the VMT 
and give it the same name except with the extension VTF.

3 files will be created: theskybox_cubemap.vtf, theskybox_cubemap.vmt, theskybox_cubemap.hdr.vmt

Edit theskybox_cubemap.vmt and change the path if needed. Then copy the 3 files to that path.

You can now apply theskybox_cubemap to the faces around the sky_camera.

You can optionally delete the hdr.vmt if you are compiling LDR only. If you compile in HDR without this
file, then the material will be replaced with a blinding white one.
)";

const char* g_completed = R"(DONE
Edit the envmap path in %s_cubemap.vmt to your liking and copy the files ending in _cubemap there.
Default path is materials/cubemap_skyboxes/
)";

const char* g_vmt_template = \
R"("WindowImposter"
{
    "$envmap" "cubemap_skyboxes/%s_cubemap"
})";

struct CVTFFileUnprivate
{
    SVTFHeader* Header;
    vlUInt uiImageBufferSize;
    vlByte* lpImageData;
};

#pragma pack(1)
struct RGBA16
{
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
};

struct BGRA8
{
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};

// VTFLib does not properly convert these modes. we fully convert them here instead of the DLL so we don't have to deal with
// getting nvidia's library to compile 

void ConvertImageToFloat(VTFLib::CVTFFile* vtf)
{
    auto output_open = (CVTFFileUnprivate*)vtf;
    auto ptr = (RGBA16*)output_open->lpImageData;
    vlUInt pixels = vtf->GetHeight() * vtf->GetWidth() * vtf->GetFaceCount() * vtf->GetFrameCount();

    for (vlUInt i = 0; i < pixels; i++)
    {
        const float factor = (float)(1.0 / 65535.0);
        float fR = ptr[i].r * factor;
        float fG = ptr[i].g * factor;
        float fB = ptr[i].b * factor;
        float fA = ptr[i].a * factor;

        ptr[i].r = half::FLOAT16::ToFloat16Fast(fR).m_uiFormat;
        ptr[i].g = half::FLOAT16::ToFloat16Fast(fG).m_uiFormat;
        ptr[i].b = half::FLOAT16::ToFloat16Fast(fB).m_uiFormat;
        ptr[i].a = half::FLOAT16::ToFloat16Fast(fA).m_uiFormat;
    }
}

void ConvertImageToBGRA(VTFLib::CVTFFile* vtf)
{
    auto output_open = (CVTFFileUnprivate*)vtf;
    auto ptr = (BGRA8*)output_open->lpImageData;
    vlUInt pixels = vtf->GetHeight() * vtf->GetWidth() * vtf->GetFaceCount() * vtf->GetFrameCount();
    for (vlUInt i = 0; i < pixels; i++)
    {
        float divisor = 8.0f;
        ptr[i].a = 0; // does nothing at 0 or 255
        ptr[i].r = (unsigned char)((ptr[i].r + 0.5f) / divisor);
        ptr[i].g = (unsigned char)((ptr[i].g + 0.5f) / divisor);
        ptr[i].b = (unsigned char)((ptr[i].b + 0.5f) / divisor);
    }
}

int EndsWith(const char* str, const char* suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void PressKeyToContinue()
{
    int junk = getchar();
}

VTFLib::CVTFFile* LoadVTF(const char* filename)
{
    auto f = new VTFLib::CVTFFile();
    f->Load(filename, false);
    if (f->IsLoaded() == false)
    {
        delete f;
        f = NULL;
    }
    return f;
}

void Rotate90CW(vlByte* p, vlUInt width, vlUInt height)
{
    vlUInt* image = (vlUInt*)p;
    vlUInt* temp = (vlUInt*)malloc(4 * width * height);
    if (temp == NULL)
    {
        std::terminate();
    }
    memcpy(temp, image, 4 * width * height);


    for (vlUInt j = 0; j < height; j++)
    {
        for (vlUInt i = 0; i < width; i++)
        {
            image[i + j * width] = temp[i * width + width - 1 - j];
        }
    }

    free(temp);
}

const char* g_faceorder[] = {
    "ft",
    "bk",
    "rt",
    "lf",
    "up",
    "dn"
};

int main(int argc, char* argv[])
{
    printf(g_banner);
    if (argc == 1)
    {
        printf(g_usage, argv[0]);
        PressKeyToContinue();
        return 0;
    }

    char* base = argv[1];
    char base_nopath[MAX_PATH];

    auto errnum = _splitpath_s(base, NULL, 0, NULL, 0, base_nopath, sizeof(base_nopath), NULL, 0);
    if (errnum != 0)
    {
        char err[64];
        strerror_s(err, errnum);
        PressKeyToContinue();
        std::terminate();
    }


    if (EndsWith(base_nopath, "ft") || EndsWith(base_nopath, "bk") || EndsWith(base_nopath, "rt") || 
        EndsWith(base_nopath, "lf") || EndsWith(base_nopath, "up") || EndsWith(base_nopath, "dn"))
    {
        int len = strlen(base);
        base[len - 6] = 0;
        len = strlen(base_nopath);
        base_nopath[len - 2] = 0;
    }

    VTFLib::CVTFFile* faces[6]{};

    for (int i = 0; i < 6; i++)
    {
        char name[256];
        snprintf(name, sizeof(name), "%s%s.vtf", base, g_faceorder[i]);
        faces[i] = LoadVTF(name);
        if (faces[i] == NULL)
        {
            printf("failed to load file %s\n", name);
            PressKeyToContinue();
            std::terminate();
        }
    }

    auto cubemap = VTFLib::CVTFFile();
    auto width  = faces[0]->GetWidth();
    auto height = faces[0]->GetHeight();

    printf("assuming dimensions of %i x %i based on %sft.vtf \n", width, height, base_nopath);

    vlByte* main_buffer[6];
    for (int i = 0; i < 6; i++)
    {
        auto face_width = faces[i]->GetWidth();
        auto face_height = faces[i]->GetHeight();
        main_buffer[i] = (vlByte*)malloc(4 * face_height * face_width);
        auto success = cubemap.ConvertToRGBA8888(faces[i]->GetData(0, 0, 0, 0), main_buffer[i], face_width, face_height, faces[i]->GetFormat());
        if (!success)
        {
            printf("ConvertToRGBA8888 %s %s\n", g_faceorder[i], vlGetLastError());
            PressKeyToContinue();
            std::terminate();
        }

        if (face_width != width || face_height != height)
        {
            printf("%s%s.vtf has a different dimension %i x %i. attempting to resize.\n", base_nopath, g_faceorder[i], face_width, face_height);
            vlByte* resized = (vlByte*)malloc(4 * height * width);
            VTFMipmapFilter filter;
            if (face_width * face_height < width * height)
            {
                filter = VTFMipmapFilter::MIPMAP_FILTER_BLACKMAN;
            }
            else
            {
                filter = VTFMipmapFilter::MIPMAP_FILTER_MITCHELL;
            }
            success = cubemap.Resize(main_buffer[i], resized, face_width, face_height, width, height, filter, VTFSharpenFilter::SHARPEN_FILTER_SHARPENSOFT);
            if (!success)
            {
                printf("resize failed: %s\n", vlGetLastError());
                PressKeyToContinue();
                std::terminate();
            }
            free(main_buffer[i]);
            main_buffer[i] = resized;
        }


        switch (i)
        {
            // make rt load before lf for this
            case 0:
                cubemap.FlipImage(main_buffer[i], width, height);
                Rotate90CW(main_buffer[i], width, height);
                Rotate90CW(main_buffer[i], width, height);
                Rotate90CW(main_buffer[i], width, height);
                break;
            case 1:
                cubemap.FlipImage(main_buffer[i], width, height);
                Rotate90CW(main_buffer[i], width, height);
                break;
            case 2:
                cubemap.FlipImage(main_buffer[i], width, height);
                break;
            case 3:
                cubemap.MirrorImage(main_buffer[i], width, height);
                break;
            case 4:
                cubemap.FlipImage(main_buffer[i], width, height);
                break;
            case 5:
                cubemap.MirrorImage(main_buffer[i], width, height);
                break;

            /*
            // make lf before rt for this
            case 0:
                Rotate90CW(main_buffer[i], width, height);
                break;
            case 1:
                Rotate90CW(main_buffer[i], width, height);
                Rotate90CW(main_buffer[i], width, height);
                Rotate90CW(main_buffer[i], width, height);
                break;
            case 2:
                output.FlipImage(main_buffer[i], width, height);
                output.MirrorImage(main_buffer[i], width, height);
                break;
            case 3:
                break;
            case 4:
                break;
            case 5:
                break;
            */
        }
    }
    
    SVTFCreateOptions options;
    memset(&options, 0, sizeof(options));
    options.uiVersion[0] = 7;
    options.uiVersion[1] = 4;
    options.uiFlags = 0x0004 | 0x0008;
    options.ImageFormat = VTFImageFormat::IMAGE_FORMAT_RGBA8888;
    options.bSphereMap = true;
    options.bThumbnail = true;

    printf("Building cubemap\n");
    bool success;
    success = cubemap.Create(width, height, 1, 6, 1, (vlByte**)&main_buffer, options);
    if (!success)
    {
        printf("Create Error %s\n", vlGetLastError());
        PressKeyToContinue();
        std::terminate();
    }

    auto ldr = VTFLib::CVTFFile(cubemap, VTFImageFormat::IMAGE_FORMAT_DXT5);
    char output_name[FILENAME_MAX];
    snprintf(output_name, sizeof(output_name), "%s_cubemap.vtf", base);
    success = ldr.Save(output_name);
    if (!success)
    {
        printf("LDR Save Error %s\n", vlGetLastError());
        PressKeyToContinue();
        std::terminate();
    }
    ldr.Destroy();

    auto hdr = VTFLib::CVTFFile(cubemap, VTFImageFormat::IMAGE_FORMAT_RGBA16161616F);
    ConvertImageToFloat(&hdr);
    snprintf(output_name, sizeof(output_name), "%s_cubemap.hdr.vtf", base);
    success = hdr.Save(output_name);
    if (!success)
    {
        printf("HDR Save Error %s\n", vlGetLastError());
        PressKeyToContinue();
        std::terminate();
    }
    hdr.Destroy();

    /*
    auto hdr_lq = VTFLib::CVTFFile(cubemap, VTFImageFormat::IMAGE_FORMAT_BGRA8888);
    ConvertImageToBGRA(&hdr_lq);
    snprintf(output_name, sizeof(output_name), "%s_cubemap.hdrlq.vtf", base);
    success = hdr_lq.Save(output_name);
    if (!success)
    {
        printf("HDR LQ Save Error %s\n", vlGetLastError());
        PressKeyToContinue();
        std::terminate();
    }
    hdr_lq.Destroy();
    */

    snprintf(output_name, sizeof(output_name), "%s_cubemap.vmt", base);
    FILE* f;
    errnum = fopen_s(&f, output_name, "w");
    if (errnum || f == NULL)
    {
        char err[64];
        strerror_s(err, errnum);
        printf("Error opening %s to write: %s\n", output_name, err);
        PressKeyToContinue();
        std::terminate();
    }

    fprintf(f, g_vmt_template, base_nopath);
    fclose(f);

    printf(g_completed, base_nopath);
    PressKeyToContinue();

    return 0;
}