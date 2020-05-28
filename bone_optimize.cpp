/*
	Bone Optimize for Maya 6.5+
	2007 8Monkey Labs
	Jeff Russell
*/

//needed to placate the deprecated iostream madness that maya uses
#ifndef REQUIRE_IOSTREAM
#define  REQUIRE_IOSTREAM
#endif

//#include <maya/MSimple.h>
#include <maya/MStatus.h>

#include <maya/MPxCommand.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MObjectArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MFnDagNode.h>
#include <maya/MItDag.h>
#include <maya/MDistance.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItGeometry.h>
#include <maya/MMatrix.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFnIkJoint.h>
#include <maya/MQuaternion.h>
#include <maya/MItKeyframe.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#include "stooge_plugin.h"

#define	clearPtr(p)	{if(p){delete p; p=0;}}
#define clearArray(a) {if(a){delete[] a; a=0;}}

struct	bone_ref 
{
	MDagPath	bone;
	bool		used;
	
	bone_ref()
	{ used = false; }
};

bool	renameNode( MDagPath& p, const char* newname )
{
	if( newname && p.hasFn( MFn::kDagNode ) )
	{
		MFnDagNode node( p );
		MStatus status;
		node.setName( MString(newname), &status );
		return status == MStatus::kSuccess;
	}
	return false;
}

void	markBone( MDagPath& r, bool dontexport )
{
	std::string nm = r.partialPathName().asChar();
	size_t loc = nm.find( NOEXPORT_SUFFIX );
	if( dontexport )
	{
		if( loc == std::string::npos )
		{
			size_t e_loc = nm.find( EXPORT_SUFFIX );
			if( e_loc == std::string::npos )
			{
				nm += NOEXPORT_SUFFIX;
				renameNode( r, nm.c_str() );
			}
		}
	}
	else
	{
		if( loc != std::string::npos )
		{ nm.erase( loc, std::string::npos ); renameNode( r, nm.c_str() ); }
	}
}

int		getBoneID( const MDagPath& dagpath, const std::vector<bone_ref>& bonez )
{
	for( unsigned int i=0; i<bonez.size(); ++i )
	{
		if( bonez[i].bone == dagpath )
		{ return i; }
	}
	return -1;
}

bool	shouldExport( const bone_ref& r, const std::vector<bone_ref>& skinned )
{
	//if the bone is used directly, then it needs to export
	if( r.used ) { return true; }
	
	//if the bone has no children, then it can be ignored
	MFnDagNode node( r.bone );
	if( node.childCount() == 0 )
	{ return false; }
	
	//if the bone has only non-skinned children, then it can be ignored
	bool has_only_non_skinned_children = true;
	std::string mypath = r.bone.fullPathName().asChar();
	for( unsigned i=0; i<skinned.size(); ++i )
	{
		std::string otherpath = skinned[i].bone.fullPathName().asChar();
		if( otherpath.find(mypath) != std::string::npos )
		{ has_only_non_skinned_children = false; break; }
	}
	if( has_only_non_skinned_children )
	{ return false; }
	
	//otherwise we'll need it even though its not skinned
	return true;
}

boneOptimize::boneOptimize()
{
}

void*	boneOptimize::creator()
{
	return new boneOptimize;
}

MStatus boneOptimize::doIt( const MArgList& args )
{
	std::cout << "\n\n-------- Bone Opt -------------\n";
	std::cout << "-------------------------------\n\n";

	for( unsigned int i=0; i<args.length(); ++i )
	{
		std::string	s = args.asString(i).asChar();
	}

	bool result = markUnusedBones();	

	if( result )
	{
		setResult( "Bone optimization completed.\n" );
		return MS::kSuccess;
	}
	else
	{
		setResult( "Bone optimization failed!\n" );
		return MS::kFailure;
	}
}

bool	boneOptimize::markUnusedBones( void )
{
	//build a list of bones in the scene
	std::vector<bone_ref>	bones;
	{
		MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid );
		for( ; !dagIterator.isDone(); dagIterator.next() )
		{
			MDagPath	dagPath;
			dagIterator.getPath(dagPath);
			MFnDagNode	dagNode(dagPath);
			if( dagNode.isIntermediateObject() )
			{ continue; }
			if( dagPath.hasFn(MFn::kJoint) )
			{
				bone_ref r; r.bone = dagPath;
				bones.push_back( r );
			}
		}
	}
	
	//determine which bones are skinned to anything
	MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid );
	for( ; !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath	dagPath;
		dagIterator.getPath(dagPath);
		
		MFnDagNode	dagNode( dagPath );
		if( dagNode.isIntermediateObject() )
		{ continue; }

		if( dagPath.hasFn(MFn::kMesh) && !dagPath.hasFn(MFn::kTransform) )
		{
			//ok, now we've got our mesh; lets write this mother
			MFnMesh meshfunction( dagPath );
			
			//now iterate through skinClusters to get bone weights and indices
			MItDependencyNodes iter( MFn::kSkinClusterFilter );
			for( ; !iter.isDone(); iter.next() )
			{
				MFnSkinCluster	fn(iter.item());
				MDagPathArray	infs;
				MStatus stat;
				unsigned int nInfs = fn.influenceObjects(infs, &stat);
				
				//loop through geometries affected by this cluster
				unsigned int nGeoms = fn.numOutputConnections();
				for( unsigned int i = 0; i<nGeoms; ++i )
				{
					//get dag path of i'th geometry
					unsigned index = fn.indexForOutputConnection(i);
					MDagPath skinPath;
					fn.getPathAtIndex(index, skinPath);

					//we only want the geometry of this mesh
					if( !(skinPath == dagPath) )
					{ continue; }

					MItGeometry gIter(skinPath);

					//iterate through points
					for( ; !gIter.isDone(); gIter.next() )
					{
						MObject	comp = gIter.component();
						
						//get the weights for this vertex (one per influence obj)
						MFloatArray	wts;
						unsigned int infCount;
						fn.getWeights( skinPath, comp, wts, infCount );
						
						unsigned int	numWeights = 0;
						float			outWts[40] = { 0.0f };
						int				outInfs[40] = {-1,-1,-1,-1,-1,-1,-1};

						//output the weight data for this vertex
						for( unsigned int j=0; j != infCount; ++j )
						if( wts[j] > 0.001f ) //ignore weights below this tolerance
						{
							outWts[numWeights] = wts[j];
							outInfs[numWeights] = j;
							++numWeights;
						}
						
						//build list of indices & weights for this vertex
						for( unsigned int n=0; n < STOOGE_BONE_WEIGHT_COUNT; ++n )
						{
							int index = (outInfs[n] >= 0 ? getBoneID( infs[outInfs[n]], bones ) : -1);
							float weight = outWts[n];
							if( index >= 0 && weight > 0.0f )
							{ bones[index].used = true; }
						}
					}
				}
			}
		}
	} // end for
	
	//bones marked with '_EXPORT' count as skinned
	for( unsigned i=0; i<bones.size(); ++i )
	{
		std::string name( bones[i].bone.partialPathName().asChar() );
		if( name.find(EXPORT_SUFFIX) != std::string::npos )
		{ bones[i].used = true; }
	}
	
	//print out the unskinned bones
	std::vector<bone_ref>	skinned_bones, unskinned_bones;
	for( unsigned i=0; i<bones.size(); ++i )
	{
		if( bones[i].used ) { skinned_bones.push_back(bones[i]); }
		else { unskinned_bones.push_back(bones[i]); }
	}
	std::cout << "Total Joints: " << bones.size() << '\n';
	std::cout << "Skinned: " << skinned_bones.size() << '\n';
	std::cout << "Unskinned: " << unskinned_bones.size() << '\n';
	
	std::cout << "The following joints are not skinned:\n";
	for( unsigned i=0; i<unskinned_bones.size(); ++i )
	{
		std::cout << unskinned_bones[i].bone.fullPathName() << '\n';
	}
	
	std::cout << "renaming bones...\n";
	unsigned disabled = 0;
	for( unsigned i=0; i<bones.size(); ++i )
	{
		bool shouldexport = shouldExport( bones[i], skinned_bones );
		markBone( bones[i].bone, !shouldexport );
		if( !shouldexport )
		{
			disabled++;
			std::cout << bones[i].bone.fullPathName() << '\n';
		}
	}
	std::cout << "done. (" << disabled << " total)\n";
	
	return true;
}

