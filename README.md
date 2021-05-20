# Cubemaker

Cubemaker converts Valve skybox textures to a single cubemap texture that can be applied to a func_brush to act exactly like a skybox.

If you apply skybox textures to a brush without using this, you will see the faint outlines of a box and they will move when you move in the world.

This is used to allow the changing of skyboxes whenever you want.

## How to use

Download the release and put the exe and dll file in a folder somewhere.

Locate the VTF files of the skybox. For example if the name of the skybox is **theskybox** there will be a folder with these files

* **theskybox_ft.vtf** 
* **theskybox_bk.vtf** 
* **theskybox_lt.vtf** 
* **theskybox_rt.vtf** 
* **theskybox_dn.vtf** 
* **theskybox_up.vtf**

Drag and drop one of these files onto the exe. 

Sometimes the dn VTF is missing. Open the VMT file with the same name (example theskybox_dn.vmt). It will point to the actual 
VTF file used (example $basetexture cs_italy/black). Find the actual VTF file, and copy and paste it into the same place
with the rest of the VTFs and give it the proper name (example theskybox_dn.vtf).

Two files will be produced

* theskybox_cubemap.vtf
* theskybox_cubemap.vmt

Move these files to **materials/cubemaps_skyboxes** or edit **theskybox_cubemap.vmt** to use a different path.

Now you can create a **func_brush** with this texture around your **sky_camera** and toggle it on and off whenever you want.

## How to compile

The program was added as a Visual Studio 2019 solution to VTFLib. Open sln/vs2017/VTFLib.sln

Compile in x86 and drop in the DLL provided by the release in the same directory to allow it to run.

It is not recommended that you use the DLL that was compiled from this project because it requires an ancient library
from Nvidia to be able to load and save compressed DXT5 textures. This compression is necessary because
it is used frequently and shrinks the output on this program by 3-4x. I was not able to get the library to compile on VS2019.

https://developer.nvidia.com/legacy-texture-tools

## Credits

The program was written by Bottiger @ skial.com. Thanks to Berke for discovering the method. Thanks to VTFLib for creating the library.