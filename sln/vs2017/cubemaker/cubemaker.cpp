#include <iostream>

#include <VTFFile.h>
#include <VTFLib.h>
#include <fp16.h>

const char* g_banner = "Skybox to Cubemap Maker by Bottiger skial.com\n\n";

const char* g_usage = \
R"(Converts a group of skybox VTFs to a cubemap VTF that can be applied to a box around a sky_camera.
This will allow you to use multiple skyboxes in TF2 by creating multiple brushes with this texture that 
you can hide and show at any time.

Drag one of the files ending in ft,bk,rt,lf,up,dn onto the program. 

All 6 of these VTF files must be in the same directory. Sometimes the dn VTF is missing. Open the VMT 
file with the same name, find the VTF it points to, copy and paste it into the same folder as the VMT 
and give it the same name except with the extension VTF.

4 files will be created: theskybox_cubemap.vtf, theskybox_cubemap.vtf.hq, theskybox_cubemap.vmt, theskybox_cubemap.hdr.vmt

If you want a smaller file size, delete theskybox_cubemap.vtf.hq. If you want higher quality skybox, 
delete theskybox_cubemap.vtf and rename the .hq version to theskybox_cubemap.vtf

Edit theskybox_cubemap.vmt and change the path if needed. Then copy the files to that path.

You can now apply theskybox_cubemap to the faces around the sky_camera.

You can optionally delete the hdr.vmt if you are compiling LDR only. If you compile in HDR without this
file, the skybox will be pure white for people with HDR enabled.
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

struct RGBA16F
{
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
};

struct RGBA32F
{
    float r;
    float g;
    float b;
    float a;
};

struct BGRA8
{
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};

struct RGBA8
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

// needed to convert to HDR
// TODO: make a table for 0 to 255?
float SRGBToLinear(float u)
{
    if (u <= 0.04045)
    {
        return (float)(u / 12.92);
    }
    else
    {
        return (float)pow((u + 0.055) / 1.055, 2.4);
    }
}

vlByte LinearToSRGB(float u)
{
    if (u <= 0.0031308)
    {
        auto scale = u * 12.92 * 255.0;
        if (scale > 255.0)
        {
            scale = 255.0;
        }
        return (vlByte)round(scale);
    }
    else
    {
        auto scale = (pow(u, 1.0 / 2.4) * 1.055 - 0.055) * 255.0;
        if (scale > 255.0)
        {
            scale = 255.0;
        }
        return (vlByte)round(scale);
    }
}

vlByte ConvertFloat16ToInt(uint16_t u)
{
    float f = fp16_ieee_to_fp32_value(u);
    vlByte output = LinearToSRGB(f);
    return output;
}

// VTFLib does not properly convert these modes. we fully convert them here instead of the DLL so we don't have to deal with
// getting nvidia's library to compile 

void ConvertImageToFloat(VTFLib::CVTFFile* vtf, VTFLib::CVTFFile* vtfOld)
{
    auto ptr = (RGBA16F*)((CVTFFileUnprivate*)vtf)->lpImageData;
    auto ptrOld = (RGBA8*)((CVTFFileUnprivate*)vtfOld)->lpImageData;
    
    vlUInt pixels = vtf->GetHeight() * vtf->GetWidth() * vtf->GetFaceCount() * vtf->GetFrameCount();

    for (vlUInt i = 0; i < pixels; i++)
    {
        float fR = ((float)ptrOld[i].r) / 255.0f;
        float fG = ((float)ptrOld[i].g) / 255.0f;
        float fB = ((float)ptrOld[i].b) / 255.0f;
        float fA = ((float)ptrOld[i].a) / 255.0f;

        fR = SRGBToLinear(fR);
        fG = SRGBToLinear(fG);
        fB = SRGBToLinear(fB);

        ptr[i].r = fp16_ieee_from_fp32_value(fR);
        ptr[i].g = fp16_ieee_from_fp32_value(fG);
        ptr[i].b = fp16_ieee_from_fp32_value(fB);
        ptr[i].a = fp16_ieee_from_fp32_value(fA);
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

    vlUInt width = 0;
    vlUInt height = 0;

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
        auto fWidth = faces[i]->GetWidth();
        auto fHeight = faces[i]->GetHeight();
        if (fWidth == fHeight && fWidth > width)
        {
            width = fWidth;
            height = fHeight;
        }
    }

    if (width == 0)
    {
        printf("Failed to find a VTF with same width and height\n");
        PressKeyToContinue();
        std::terminate();
    }

    printf("assuming dimensions of %i x %i based on largest square VTF \n", width, height);
    auto cubemap = VTFLib::CVTFFile();

    vlByte* main_buffer[6];
    RGBA8 lastPixel[6]; // get the last pixel for face sides for stretching method
    RGBA8 lastPixelAverage{};

    // convert to RGBA8888 internally because sphere face making process in VTFLib and Valve both do that already
    for (int i = 0; i < 6; i++)
    {
        auto face_width = faces[i]->GetWidth();
        auto face_height = faces[i]->GetHeight();
        main_buffer[i] = (vlByte*)malloc(4 * face_height * face_width);
        auto oldFormat = faces[i]->GetFormat();

        // gamma correction for HDR formats
        if (oldFormat == VTFImageFormat::IMAGE_FORMAT_RGBA16161616F)
        {
            auto old_ptr = (RGBA16F*)faces[i]->GetData(0, 0, 0, 0);
            auto new_ptr = (RGBA8*)main_buffer[i];
            for (vlUInt j = 0; j < face_width * face_height; j++)
            {
                new_ptr[j].r = ConvertFloat16ToInt(old_ptr[j].r);
                new_ptr[j].g = ConvertFloat16ToInt(old_ptr[j].g);
                new_ptr[j].b = ConvertFloat16ToInt(old_ptr[j].b);
                new_ptr[j].a = (unsigned char)(fp16_ieee_to_fp32_value(old_ptr[j].a) * 255.0f);
            }
        }
        else if (oldFormat == VTFImageFormat::IMAGE_FORMAT_RGBA32323232F)
        {
            auto old_ptr = (RGBA32F*)faces[i]->GetData(0, 0, 0, 0);
            auto new_ptr = (RGBA8*)main_buffer[i];
            for (vlUInt j = 0; j < face_width * face_height; j++)
            {
                new_ptr[j].r = LinearToSRGB(old_ptr[j].r);
                new_ptr[j].g = LinearToSRGB(old_ptr[j].g);
                new_ptr[j].b = LinearToSRGB(old_ptr[j].b);
                new_ptr[j].a = (unsigned char)(old_ptr[j].a * 255.0f);
            }
        }
        else
        {
            auto success = cubemap.ConvertToRGBA8888(faces[i]->GetData(0, 0, 0, 0), main_buffer[i], face_width, face_height, oldFormat);
            if (!success)
            {
                printf("ConvertToRGBA8888 %s %s\n", g_faceorder[i], vlGetLastError());
                PressKeyToContinue();
                std::terminate();
            }
        }

        RGBA8* pixelPtr = (RGBA8*)main_buffer[i];
        lastPixel[i] = pixelPtr[face_height * face_width - 1];
    }
    
    // calculate average last pixel color for stretch method
    {
        float r = 0.0, g = 0.0, b = 0.0, a = 0.0;
        for (int i = 0; i <= 3; i++)
        {
            r += lastPixel[i].r;
            g += lastPixel[i].g;
            b += lastPixel[i].b;
            a += lastPixel[i].a;
        }

        lastPixelAverage.r = (vlByte)round(r / 4.0);
        lastPixelAverage.g = (vlByte)round(g / 4.0);
        lastPixelAverage.b = (vlByte)round(b / 4.0);
        lastPixelAverage.a = (vlByte)round(a / 4.0);
    }

    for(int i = 0; i < 6; i++)
    {
        auto face_width = faces[i]->GetWidth();
        auto face_height = faces[i]->GetHeight();
        if (face_width != width || face_height != height)
        {
            printf("%s%s.vtf has a different dimension %i x %i. attempting to resize.\n", base_nopath, g_faceorder[i], face_width, face_height);
            // try to enlarge side faces without stretching them. 
            // This seems to be the correct method for rectangular sideways skyboxes
            // take the last pixel and fill out the buffer with it
            bool skip_resize = false;
            if (i >= 0 && i <= 3 && face_width > face_height)
            {
                printf("padding rectangular side face\n");
                vlByte* resized = (vlByte*)malloc(4 * face_width * face_width);
                memcpy(resized, main_buffer[i], 4 * face_width * face_height);
                RGBA8* resizedPixelPtr = (RGBA8*)resized;
                for (vlUInt i = face_width * face_height; i < face_width * face_width; i++)
                {
                    resizedPixelPtr[i] = lastPixelAverage;
                }
                free(main_buffer[i]);
                main_buffer[i] = resized;
                face_height = face_width;
                // check if we still need to stretch it out
                skip_resize = face_height == height && face_width == width;
            }

            if (!skip_resize)
            {
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
                bool success = cubemap.Resize(main_buffer[i], resized, face_width, face_height, width, height, filter, VTFSharpenFilter::SHARPEN_FILTER_SHARPENSOFT);
                if (!success)
                {
                    printf("resize failed: %s\n", vlGetLastError());
                    PressKeyToContinue();
                    std::terminate();
                }
                free(main_buffer[i]);
                main_buffer[i] = resized;
            }
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

    auto ldr_hq = VTFLib::CVTFFile(cubemap, VTFImageFormat::IMAGE_FORMAT_RGB888);
    snprintf(output_name, sizeof(output_name), "%s_cubemap.vtf.hq", base);
    success = ldr_hq.Save(output_name);
    if (!success)
    {
        printf("LDR high quality Save Error %s\n", vlGetLastError());
        PressKeyToContinue();
        std::terminate();
    }
    ldr_hq.Destroy();

    auto hdr = VTFLib::CVTFFile(cubemap, VTFImageFormat::IMAGE_FORMAT_RGBA16161616F);
    ConvertImageToFloat(&hdr, &cubemap);
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