#ifndef _STOOGE_PLUGIN_H_
#define _STOOGE_PLUGIN_H_

#define	STOOGE_BONE_WEIGHT_COUNT	4
#define STOOGE_SKELETON_SUFFIX		".skel"
#define STOOGE_MESH_SUFFIX			".mesh"
#define STOOGE_RIG_SUFFIX			".rig"
#define STOOGE_ANIM_SUFFIX			".anim"

#define	NOEXPORT_SUFFIX				"DONTEXPORT"
#define EXPORT_SUFFIX				"_EXPORT"

//cleanup command class
class	stoogeClean : public MPxCommand
{
public:
	stoogeClean();
	virtual MStatus	doIt( const MArgList& );
	static void*	creator();
};

//export command class
class	stoogeExport : public MPxCommand
{
public:
	stoogeExport();
	virtual MStatus	doIt( const MArgList& );
	static void*	creator();
};

//bone saw command class
class	boneSaw : public MPxCommand
{
public:
	boneSaw();
	virtual MStatus	doIt( const MArgList& );
	static void*	creator();
};

//bone optimize command (marks bones to ignore)
class	boneOptimize : public MPxCommand
{
public:
	boneOptimize();
	virtual MStatus	doIt( const MArgList& );
	static void*	creator();
	bool			markUnusedBones( void );
};

//stooge flatten command (for flattening scale)
class	stoogeFlatten : public MPxCommand
{
public:
	stoogeFlatten();
	virtual MStatus	doIt( const MArgList& );
	static void*	creator();
};


#endif
