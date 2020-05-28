----------------------------
Stooge Exporter for Maya
by Jeff Russell
Copyright 2005-2010 8monkey Labs Inc.
----------------------------

PURPOSE:
------------
As of the time of this writing, stoogeExport does the following in Maya:

	- Export static meshes (including rigging info and many other mesh attributes)
	- Export .rig files (for material bindings and possible future uses)
	- Export skeletons
	- Export animation sequences

stoogeExport does NOT perform any of the following functions:

	- Solve world hunger
	- Make you more attractive
	- Anything else, really


INSTALLATION:
------------
Close Maya, then:

- Move the appropriate .mll file for your version of Maya into your maya directory, into bin/plug-ins/.

- Move the .mel script file into any suitable script folder, such as /scripts.

Open Maya. Then go to the plugin window (its under Window->Setting/Preferences->Plug-in Manager).

Search for the "stoogeExport" plugin, and check both the "Loaded" and "Auto-Load" boxes.


USE:
------------
Once the appropriate .mll and .mel script are installed, to envoke the exporter, simply type 'stooge' (without quotes) into the MEL command line.
You will then be presented with an options dialog. Hit the 'export' button when you are ready to go.


FILE TYPES:
------------
.mesh:
A mesh file, as you might have guessed.  Can contain vertex positions, normals, tangents, bitangents, colors, and a full complement of texture coordinates.  This file is almost always paired with a ".rig" file (see below).

.rig:
A user-editable text file that contains material bindings.  Edit this file in order to bind our material system to a mesh. It contains a listing of mesh chunks from the exported maya scene and what material file each should use (defaults to "placeholdermaterial").

.skel:
A skeleton file, contains the bone structure for an animated mesh. This file gets exported simultaneously with a ".mesh" file, and the two should generally be considered to be joined at the hip (pun intended).

.anim:
An animation sequence. This is exported independently from the previous 3, so that a single animated mesh can have several animations.