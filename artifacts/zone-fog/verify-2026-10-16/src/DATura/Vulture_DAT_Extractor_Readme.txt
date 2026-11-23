Author: vulture
License: public domain

Usage: dat [filename.dat]

#define WRITE_CHUNKS_TO_DISK
#define WRITE_COLLISION_MESH_TO_DISK
// other things comment/uncomment as desired whatever

-------------------------------------------------------------

This program is used to extract hex dumps and collision mesh data for zones in FINAL FANTASY XI. For your convenience, the complete source code has been included along with the already compiled executable. If you wish to change any aspect of the program, simply edit the source code file and recompile.



Instructions:

   1. Place the dat.exe executable into its own folder. The one it comes in is fine.
   2. Type "cmd" into the folder filepath field and then press Enter to launch the Windows Command Prompt.
   3. In the Command Prompt window, type "dat.exe" followed by the filepath of the DAT file you want to extract, and press Enter.
      Note that the filepath must be contained within quotation marks.

   EXAMPLE:
      dat.exe "C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI/ROM/0/124.DAT"


   The program will extract a hex dump of the entire DAT file, which create literally hundreds of .dec files in the same folder where the executable is located. This is the reason why you want to ensure that the executable is in its own folder. In order to read the .dec files, you will need some kind of hex editor program. There is a good hex editor called HxD, which you can download from the following website:

   https://mh-nexus.de/en/hxd/

   In addition to the hundreds of .dec files, this program will also create one 3D object called "out," which is saved in the .obj file format. This is the collision mesh for the zone specified.
