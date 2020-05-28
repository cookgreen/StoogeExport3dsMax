# Copyright (C) Alias Systems,  a division  of  Silicon Graphics Limited and/or
# its licensors ("Alias").  All rights reserved.  These coded instructions,
# statements, computer programs, and/or related material (collectively, the
# "Material") contain unpublished information proprietary to Alias, which is
# protected by Canadian and US federal copyright law and by international
# treaties.  This Material may not be disclosed to third parties, or be copied
# or duplicated, in whole or in part, without the prior written consent of
# Alias.  ALIAS HEREBY DISCLAIMS ALL WARRANTIES RELATING TO THE MATERIAL,
# INCLUDING, WITHOUT LIMITATION, ANY AND ALL EXPRESS OR IMPLIED WARRANTIES OF
# NON-INFRINGEMENT, MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
# IN NO EVENT SHALL ALIAS BE LIABLE FOR ANY DAMAGES WHATSOEVER, WHETHER DIRECT,
# INDIRECT, SPECIAL, OR PUNITIVE, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, OR IN EQUITY, ARISING OUT OF OR RELATED TO THE
# ACCESS TO, USE OF, OR RELIANCE UPON THE MATERIAL.

#
# Include platform specific build settings
#
include buildconfig

.SUFFIXES: .cpp .cc .o .so .lib .c

.c.o:
	$(CC) -c $(INCLUDES) $(CFLAGS) $<
	
.cc.o:
	$(C++) -c $(INCLUDES) $(C++FLAGS) $<

.cpp.o:
	$(C++) -c $(INCLUDES) $(C++FLAGS) $<

.cc.i:
	$(C++) -E $(INCLUDES) $(C++FLAGS) $*.cc > $*.i

.cc.so:
	-rm -f $@
	$(LD) -o $@ $(INCLUDES) $< $(LIBS)

.cpp.so:
	-rm -f $@
	$(LD) -o $@ $(INCLUDES) $< $(LIBS)

.o.so:
	-rm -f $@
	$(LD) -o $@ $< $(LIBS)
    
.o.lib:
	-rm -f $@
	$(LD) -o $@ $< $(LIBS)

all:
	-rm -f *.o
	-make stooge

stooge: stooge_export.o stooge_clean.o ../../engine/math/SurfaceMath.o
	$(LD) -o stoogeExportMaya.lib stooge_export.o stooge_clean.o SurfaceMath.o $(LIBS) -lOpenMayaAnim -lOpenMaya -lOpenMayaUI

clean:
	-rm -f *.o
