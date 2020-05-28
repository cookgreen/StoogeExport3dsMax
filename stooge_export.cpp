/*
	Stooge File Exporter for Maya 6.5+
	2005 Annex Labs
	Jeff Russell
	
	TODO:
	+ proper tangent/bitangent indexing is required
	+ add option/fallback to use old tangent scheme
	+ fix .mesh.mesh problem
	+ triangulation upon export is not undoable (can i push an undo state or something?)
	+ triangulation upon export might wreck normals a bit ("lock" them first sez joe)
	* possible indexing problems for separate vertices with the same maya UV index (fubar quad on hand bug)
	+ mesh hiding on export should work, doesn't :(
	+ undo triangulation automatically
*/

//needed to placate the deprecated iostream madness that maya uses
#ifndef REQUIRE_IOSTREAM
#define  REQUIRE_IOSTREAM
#endif

#include <maya/MSimple.h>
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
#include <maya/MFnPlugin.h>
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
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>

#include <maya/MFnAttribute.h>
#include <maya/MFnEnumAttribute.h>

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#include "stooge_io.h"
#include "stooge_plugin.h"

//annex math since maya's sucks apparently
#include "SurfaceMath.h"

//sigh
#include "ReIndex.h"

#define	CM_TO_FEET	0.03280839895013123

#define	clearPtr(p)	{if(p){delete p; p=0;}}
#define clearArray(a) {if(a){delete[] a; a=0;}}

using namespace m8;

MStatus initializePlugin( MObject _obj )
{
	MFnPlugin	plugin( _obj, "8Monkey Labs", "6.5" );
	plugin.registerCommand( "stoogeExport", stoogeExport::creator );
	plugin.registerCommand( "stoogeClean", stoogeClean::creator );
	plugin.registerCommand( "boneSaw", boneSaw::creator );
	plugin.registerCommand( "boneOpt", boneOptimize::creator );
	plugin.registerCommand( "stoogeFlatten", stoogeFlatten::creator );
	return MS::kSuccess;
}

MStatus uninitializePlugin( MObject _obj )
{
	MFnPlugin	plugin( _obj, "8Monkey Labs", "6.5" );
	plugin.deregisterCommand( "stoogeExport" );
	plugin.deregisterCommand( "stoogeClean" );
	plugin.deregisterCommand( "boneSaw" );
	plugin.deregisterCommand( "boneOpt" );
	plugin.deregisterCommand( "stoogeFlatten" );
	return MS::kSuccess;
}

bool	writeMesh( const char* basepath, bool dobones, bool usecolors, bool dotangents, bool split, bool userig, bool useoldtangentmethod );
bool	writeSkeleton( const char* basepath );
bool	writeRig( const char* basepath, bool allchunks=true );
void	error( const char* err );
void	warning( const char* warn );

void	getBoneTransform( MFnIkJoint& joint, float* pos, float* rot );
int		getBoneID( const MDagPath& dagpath, std::vector<MDagPath>& dagpaths );
void	getBoneList( std::vector<MDagPath>& bonelist );
void	sortBoneStuff(	const std::vector<int>& indices, const std::vector<float>& weights,
						std::vector<int>& sorted_indices, std::vector<float>& sorted_weights );
bool	isDead( MDagPath& bone );

void	getKeyframes( std::vector<double>& frames, float max_frame_interval );
bool	writeAnimation( const char* basepath );
bool    writeAnimationsFromText(const char *szBasePath, const char *szAnimFile, float max_frame_interval);


bool	isMeshVisible(const MFnDagNode &dagNode);	//Recursive Funtion to test all the parents of a dagnode

std::vector<MDagPath>	bones;
std::vector<double>		keyframes;
std::string				defaultmtl("placeholdermaterial");

enum
{
	BONE_DEAD = 0x0000001
};

struct	MeshChunkRef
{
	std::string	name;
	std::string mtlname;
};
std::vector<MeshChunkRef>	chunks;

int	main( void )
{
	return 0;
}

stoogeExport::stoogeExport()
{}

void*	stoogeExport::creator()
{
	return new stoogeExport;
}

MStatus stoogeExport::doIt( const MArgList& args )
//
//	Description:
//		implements the MEL stoogeExport command.
//
//	Arguments:
//		args - the argument list that was passes to the command from MEL
//
//	Return Value:
//		MS::kSuccess - command succeeded
//		MS::kFailure - command failed (returning this value will cause the 
//                     MEL script that is being run to terminate unless the
//                     error is caught using a "catch" statement.
//
{
	std::cout << "\n\n-------- Stooge Exporter 1.0 --------\n";
	std::cout << "-------------------------------------\n";

	bool		dobones = false;
	bool		usecolors = false;
	bool		usetangents = true;
	bool		useoldtangents = false;
	bool		split = false;
	bool		animation = false;
	bool		userig = true;
#ifdef __APPLE__
	std::string	filepath("/Users/jeffdr/Desktop/stoogefile");
#else
	std::string	filepath("C:/stoogefile");
#endif
	std::string animfile;
	
	float		anim_res = 0.15f;

	chunks.clear();
	keyframes.clear();
	bones.clear();

	for( unsigned int i=0; i<args.length(); ++i )
	{
		std::string	s = args.asString(i).asChar();

		if( s == "-o" )
		{
			if( i == args.length()-1 )
			{ error( "specify a filepath if you're going to use \"-o\"" ); }
			else
			{
				filepath = args.asString(i+1).asChar();
				++i;
			}
		}
		
		else if( s == "-mtl" )
		{
			if( i == args.length()-1 )
			{ error( "specify a filepath if you're going to use \"-mtl\"" ); }
			else
			{
				defaultmtl = args.asString(i+1).asChar();
				++i;
			}
		}

		else if( s == "-af" )
		{
			if( i == args.length()-1 )
			{ error("specify an animation definition to load from"); }
			else
			{
				i++;
				animfile = args.asString(i).asChar();
			}
		}
		
		else if( s == "-bones" || s == "-bone" )
		{ dobones = true; }

		else if( s == "-vertexcolors" )
		{ usecolors = true; }

		else if( s == "-notangents" )
		{ usetangents = false; }
		
		else if( s == "-split" )
		{ split = true; }
		
		else if( s == "-anim" || s == "-animation" )
		{ animation = true; }
	
		else if( s == "-norig" )
		{ userig = false; }
		
		else if( s == "-oldtangentmethod" )
		{ useoldtangents = true; }
		
		else if( s == "-highresanim" )
		{
			anim_res /= 4.f;
			std::cout << "Using high res animation export!" << std::endl;
		}
	}

	//fix '\' characters in filepath
	for( unsigned i=0; i<filepath.size(); ++i )
	{
		if( filepath[i] == '\\' )
		{ filepath[i] = '/'; }
	}

	std::cout << "Base path is \"" << filepath << "\"" << std::endl;
	
	if( dobones || animation )
	{
		std::cout << "getting bone list..." << std::endl;
		getBoneList(bones);
		std::cout << "done." << std::endl;
		
		std::cout << "getting keyframe list..." << std::endl;
		if( animation )
		{ getKeyframes(keyframes, anim_res); }
		std::cout << "done." << std::endl;
	}
	
	if( animation )
	{
		if( animfile.empty() )	//If there is no text here, go along with the original exporting ways.
		{ writeAnimation(filepath.c_str()); }
		else
		{ writeAnimationsFromText(filepath.c_str(), animfile.c_str(), anim_res); }
	}
	else
	{
		writeMesh( filepath.c_str(), dobones, usecolors, usetangents, split, userig, useoldtangents );
	}
	
	setResult( "Stooge export completed.\n" );
	return MS::kSuccess;
}


bool	writeMesh( const char* basepath, bool dobones, bool usecolors, bool dotangents, bool split, bool userig, bool useoldtangentmethod )
{
	//how many chunks do we have?
	unsigned int chunkcount = 0;
	MItDag derIterator( MItDag::kBreadthFirst, MFn::kInvalid );
	for( ; !derIterator.isDone(); derIterator.next() )
	{
		MDagPath	path; derIterator.getPath(path);
		if( !MFnDagNode(path).isIntermediateObject() &&
			path.hasFn(MFn::kMesh) &&
			!path.hasFn(MFn::kTransform) &&
			isMeshVisible(MFnDagNode(path)) )
		{ chunkcount++; }
	}

	//open file
	FILE*	file;
	if( !split )
	{
		std::string outpath = basepath;
		if( outpath.find(STOOGE_MESH_SUFFIX) == std::string::npos )
		{ outpath += STOOGE_MESH_SUFFIX; }
		
		file = fopen( outpath.c_str(), "wb" );
		if( file == 0 )
		{ error("could not open file!"); return false; }

		//write file header
		writeUnsignedInt(file, 0);	//header flags
		writeUnsignedInt(file, chunkcount);	//chunk count
	}

	//write chunks:
	//iterate over the maya scenegraph
	MStatus	stat = MS::kSuccess;
	MObject	component;
	MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid, &stat );
	for( ; !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath	dagPath;
		stat = dagIterator.getPath(dagPath);
		
		MFnDagNode	dagNode( dagPath, &stat );
		if( dagNode.isIntermediateObject() )
		{ continue; }

		if( dagPath.hasFn(MFn::kMesh) && !dagPath.hasFn(MFn::kTransform) )
		{
			std::cout << "Checking if the mesh: \"" << dagNode.name() << "\" is visible:\n";
			std::cout.flush();
			if( !isMeshVisible(dagNode) )
			{
				std::cout << " *** INVISIBLE MESH DETECTED ***\n";
				continue; 
			}

			if( split )
			{
				file = fopen( ((std::string(basepath) + dagPath.partialPathName().asChar()) + STOOGE_MESH_SUFFIX).c_str(), "wb" );
				if( file == 0 )
				{ error("could not open file!"); return false; }

				//write file header
				writeUnsignedInt(file, 0); //header flags
				writeUnsignedInt(file, 1); //chunk count
			}
			
			std::cout << "----mesh chunk: " << dagPath.partialPathName() << std::endl;
			MeshChunkRef	r;
			r.name = std::string(dagPath.partialPathName().asChar());
			r.mtlname = defaultmtl;
			chunks.push_back(r);
			
			//before triangulation, joe sez we should lock the normals
			//MEL: "polyNormalPerVertex -fn true;" will do it to the current selection
			MGlobal::executeCommand( "polyNormalPerVertex -fn true " + dagPath.fullPathName(), true, true );
			
			//use MEL to triangulate this thing
			MGlobal::executeCommand( "polyTriangulate " + dagPath.fullPathName(), true, true );

			//ok, now we've got our mesh; lets write this mother
			MDagPath meshpath; dagIterator.getPath(meshpath);
			MFnMesh meshfunction( meshpath );

			//TODO: handle no parent nodes?
			MFnDagNode	parent = dagNode.parent(0);
			
			//get vertices
			std::cout << "getting vertices..." << std::endl;
			MPointArray	vertices;
			meshfunction.getPoints(vertices, MSpace::kWorld);
			for( unsigned ass=0; ass<vertices.length(); ++ass )
			{ vertices[ass] = vertices[ass] * CM_TO_FEET; }

			//get normals
			std::cout << "getting normals..." << std::endl;
			MFloatVectorArray	normals;
			meshfunction.getNormals(normals, MSpace::kWorld);
			
			//get vertex colors
			std::cout << "getting colors..." << std::endl;
			MColorArray	colors;
			meshfunction.getVertexColors(colors);

			//get uv coords (anywhere from 0 to n sets of em)
			std::cout << "getting texcoords..." << std::endl;
			MStringArray uvsets;
			int nuvsets;
			meshfunction.getUVSetNames(uvsets);
			if( !uvsets.length() || !meshfunction.numUVs(uvsets[0]) )
			{ warning("no uv coordinates found!"); nuvsets = 0; }
			else
			{ nuvsets = uvsets.length(); }
			MFloatArray* u_coords_sets = new MFloatArray[nuvsets];
			MFloatArray* v_coords_sets = new MFloatArray[nuvsets];
			for( int i=0; i<nuvsets; ++i )
			{
				std::cout << "  getting set " << i << std::endl;
				meshfunction.getUVs( u_coords_sets[i], v_coords_sets[i], &uvsets[i] );
			}
			
			const MString* uvset0_name = uvsets.length() ? &(uvsets[0]) : 0;
			
			//get tangents
			std::cout << "getting tangents..." << std::endl;
			MFloatVectorArray	tangents;
			meshfunction.getTangents( tangents, MSpace::kWorld, uvset0_name );
			
			//get bitangents
			std::cout << "getting bitangents..." << std::endl;
			MFloatVectorArray	bitangents;
			meshfunction.getBinormals( bitangents, MSpace::kWorld, uvset0_name );
			
			//now iterate through skinClusters to get bone weights and indices
			std::cout << "getting bone weights..." << std::endl;
			int*	boneindices=0;
			float*	boneweights=0;
			if( dobones )
			{
			MItDependencyNodes iter( MFn::kSkinClusterFilter );
			boneindices = new int[STOOGE_BONE_WEIGHT_COUNT * vertices.length()];
			boneweights = new float[STOOGE_BONE_WEIGHT_COUNT * vertices.length()];
			for( ; !iter.isDone(); iter.next() )
			{
				MFnSkinCluster	fn(iter.item());
				MDagPathArray	infs;
				MStatus			dummy;
				unsigned int nInfs = fn.influenceObjects(infs, &dummy);
				
				//loop through geometries affected by this cluster
				unsigned int nGeoms = fn.numOutputConnections();
				for( unsigned int i = 0; i<nGeoms; ++i )
				{
					//get dag path of i'th geometry
					unsigned int index = fn.indexForOutputConnection(i);
					MDagPath		skinPath;
					fn.getPathAtIndex(index, skinPath);

					//we only want the geometry of this mesh
					if( !(skinPath == dagPath) )
					{ continue; }

					MItGeometry gIter(skinPath);

					// print out the name of the skin cluster,
					// the vertexCount and the influenceCount
					cout<< "Skin: " << skinPath.partialPathName().asChar() <<endl;
					cout<< "pointcount " << gIter.count() <<endl;
					cout<< "numInfluences " << nInfs <<endl;

					//iterate through points
					for( ; !gIter.isDone(); gIter.next() )
					{
						MObject	comp = gIter.component();
						
						//get the weights for this vertex (one per influence obj)
						MFloatArray	wts;
						unsigned int infCount;
						fn.getWeights( skinPath, comp, wts, infCount );
						//if( infCount != 0 && gIter.index() == i && !gIter.isDone() )
						{
							unsigned int	numWeights = 0;
							float			outWts[40] = {0.0f};
							int				outInfs[40] = {-1,-1,-1,-1,-1,-1,-1};

							//output the weight data for this vertex
							for( unsigned int j=0; j != infCount; ++j )
							if( wts[j] > 0.001f )	//ignore weights below this tolerance
							{
								outWts[numWeights] = wts[j];
								outInfs[numWeights] = j;
								++numWeights;
							}
							
							//sort the indices greatest to least
							std::vector<int>	indices, sorted_indices;
							std::vector<float>	weights, sorted_weights;
							
							//build list of indices & weights for this vertex
							for( unsigned int n=0; n < STOOGE_BONE_WEIGHT_COUNT; ++n )
							{
								int index = (outInfs[n] >= 0 ? getBoneID( infs[outInfs[n]], bones ) : -1);
								float weight = outWts[n];
								indices.push_back(index); weights.push_back(weight);
							}
							sortBoneStuff(indices, weights, sorted_indices, sorted_weights);
							
							//store em
							for( unsigned int n=0; n < STOOGE_BONE_WEIGHT_COUNT; ++n )
							{
								boneindices[gIter.index() * STOOGE_BONE_WEIGHT_COUNT + n] = sorted_indices[n];
								boneweights[gIter.index() * STOOGE_BONE_WEIGHT_COUNT + n] = sorted_weights[n];
							}
						}
					}

					goto _FOUND_SKIN_CLUSTER;
				}
			}
			}
			_FOUND_SKIN_CLUSTER:

			//------ok finally sort out the indices ONCE AND FOR ALL!!!!1
			std::cout << "reorganizing vertex data..." << std::endl;
			MItMeshPolygon	itPoly(meshfunction.object());
			std::vector<stupid::indexref>	indices;
			int vertex_count = stupid::reIndex( normals, itPoly, indices, uvsets, !useoldtangentmethod );
			//std::cout << "DBG: point 0" << std::endl;
			std::cout << "total vertex count is " << vertex_count << std::endl;
			float*	final_positions = new float[ vertex_count * 3 ];
			float*	final_normals = new float[ vertex_count * 3 ];
			float*	final_tangents = new float[ vertex_count * 3 ];
			float*	final_bitangents = new float[ vertex_count * 3 ];
			float*	final_colors = (colors.length() && usecolors ? new float[ vertex_count * 4 ] : 0);
			float*	final_boneweights = (dobones ? new float[ vertex_count * STOOGE_BONE_WEIGHT_COUNT ] : 0 );
			int*	final_boneindices = (dobones ? new int[ vertex_count * STOOGE_BONE_WEIGHT_COUNT ] : 0 );
			float**	final_uvs = new float*[ nuvsets ];
			//std::cout << "DBG: point 1" << std::endl;
			for( int i=0; i<nuvsets; ++i )
			{ final_uvs[i] = new float[ vertex_count * 2 ]; }
			//std::cout << "DBG: point 2" << std::endl;
			
			if( final_boneindices )
			for( int i=0; i<vertex_count; i++ )
			{ //fill the bone indices with -666 so that the char animator can skip these empty spots in the array
				final_boneindices[ i*STOOGE_BONE_WEIGHT_COUNT+0 ] =
				final_boneindices[ i*STOOGE_BONE_WEIGHT_COUNT+1 ] =
				final_boneindices[ i*STOOGE_BONE_WEIGHT_COUNT+2 ] =
				final_boneindices[ i*STOOGE_BONE_WEIGHT_COUNT+3 ] = -666;
			}
			//std::cout << "DBG: point 3" << std::endl;
			
			for( int i=0; i<vertex_count; i++ )
			{ //ditto for positions
				final_positions[ i*3+0 ] =
				final_positions[ i*3+1 ] =
				final_positions[ i*3+2 ] = 0.0f;
			}
			//std::cout << "DBG: point 4" << std::endl;
			
			for( unsigned i=0; i<indices.size(); ++i )
			{
				unsigned int index = indices[i].newindex;
				final_positions[ index*3+0 ] = (float)vertices[indices[i].vert_index].x;
				final_positions[ index*3+1 ] = (float)vertices[indices[i].vert_index].y;
				final_positions[ index*3+2 ] = (float)vertices[indices[i].vert_index].z;
				
				final_normals[ index*3+0 ] = normals[indices[i].norm_index].x;
				final_normals[ index*3+1 ] = normals[indices[i].norm_index].y;
				final_normals[ index*3+2 ] = normals[indices[i].norm_index].z;
				
				//maya tangents (using normal indices for now ?)
				final_tangents[ index*3+0 ] = tangents[indices[i].tangent_index].x;
				final_tangents[ index*3+1 ] = tangents[indices[i].tangent_index].y;
				final_tangents[ index*3+2 ] = tangents[indices[i].tangent_index].z;
				
				//maya bitangents (using normal indices for now ?)
				final_bitangents[ index*3+0 ] = bitangents[indices[i].tangent_index].x;
				final_bitangents[ index*3+1 ] = bitangents[indices[i].tangent_index].y;
				final_bitangents[ index*3+2 ] = bitangents[indices[i].tangent_index].z;
				
				for( int u=0; u<nuvsets; ++u )
				{
					final_uvs[u][ index*2+0 ] = u_coords_sets[u][ indices[i].uv_index[u] ];
					final_uvs[u][ index*2+1 ] = v_coords_sets[u][ indices[i].uv_index[u] ];
				}
				
				if( final_colors )
				{
					final_colors[ index*4+0 ] = fabsf( colors[ indices[i].vert_index ].r );
					final_colors[ index*4+1 ] = fabsf( colors[ indices[i].vert_index ].g );
					final_colors[ index*4+2 ] = fabsf( colors[ indices[i].vert_index ].b );
					final_colors[ index*4+3 ] = fabsf( colors[ indices[i].vert_index ].a );
				}
				
				if( dobones )
				{
					final_boneweights[ index*STOOGE_BONE_WEIGHT_COUNT + 0 ] = boneweights[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 0 ];
					final_boneweights[ index*STOOGE_BONE_WEIGHT_COUNT + 1 ] = boneweights[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 1 ];
					final_boneweights[ index*STOOGE_BONE_WEIGHT_COUNT + 2 ] = boneweights[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 2 ];
					final_boneweights[ index*STOOGE_BONE_WEIGHT_COUNT + 3 ] = boneweights[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 3 ];
					
					final_boneindices[ index*STOOGE_BONE_WEIGHT_COUNT + 0 ] = boneindices[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 0 ];
					final_boneindices[ index*STOOGE_BONE_WEIGHT_COUNT + 1 ] = boneindices[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 1 ];
					final_boneindices[ index*STOOGE_BONE_WEIGHT_COUNT + 2 ] = boneindices[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 2 ];
					final_boneindices[ index*STOOGE_BONE_WEIGHT_COUNT + 3 ] = boneindices[ STOOGE_BONE_WEIGHT_COUNT*indices[i].vert_index + 3 ];
				}
			}
			
			stupid::finalCollapse(	indices,
									final_positions,
									final_normals,
									final_tangents,
									final_bitangents,
									final_colors,
									final_boneweights,
									final_boneindices,
									final_uvs,
									vertex_count,
									nuvsets,
									STOOGE_BONE_WEIGHT_COUNT	);
			//std::cout << "DBG: point 5" << std::endl;
			//---------------
			
			if( useoldtangentmethod )
			{
				//compute tangent space ourselves (old method)
				float* tangents=0;
				float* bitangents=0;
				unsigned int* indices_temp;
				std::cout << "computing tangent basis..." << std::endl;
				if( dotangents && nuvsets )
				{
					indices_temp = new unsigned int[indices.size()];
					for( unsigned i=0; i<indices.size(); ++i )
					{ indices_temp[i] = indices[i].newindex; }
					tangents = new float[vertex_count*3];
					bitangents = new float[vertex_count*3];
					computeTangentSpaces(	indices_temp, (int)indices.size(), final_positions,
											final_uvs[0], final_normals, tangents, bitangents	);
				}
				delete[] indices_temp;
				
				delete[] final_tangents;
				final_tangents = tangents;
				
				delete[] final_bitangents;
				final_bitangents = bitangents;
			}
			
			std::cout << "writing...";
			
			//write chunk header
			writeString(file, dagPath.partialPathName().asChar()); //write mesh chunk name
			writeUnsignedInt(file, vertex_count);
			
			//write vertex positions
			writeUnsignedShort(file, 1);
			writeArray<float>(file, final_positions, vertex_count*3 );
			delete[] final_positions;

			//write normals
			writeUnsignedShort(file, 1);
			writeArray<float>( file, final_normals, vertex_count*3 );
			delete[] final_normals;

			//write vertex colors
			if( final_colors == 0 )
			{ writeUnsignedShort(file, 0); }
			else
			{
				writeUnsignedShort(file, 4);
				writeArray<float>(file, final_colors, vertex_count*4);
				delete[] final_colors;
			}

			//write texture coordinate sets
			writeUnsignedShort( file, nuvsets ); //texcoord set count
			for( int i=0; i<nuvsets; ++i )
			{
				writeUnsignedShort(file, 2);	//2 components per texture coordinate
				writeArray<float>( file, final_uvs[i], vertex_count*2 );
				delete[] final_uvs[i];
			}
			delete[] final_uvs;

			//write tangents and bitangents
			if( dotangents && nuvsets )
			{
				writeUnsignedShort(file, 3); //tangents
				writeArray<float>(file, final_tangents, vertex_count*3);
				
				writeUnsignedShort(file, 3); //bitangents
				writeArray<float>(file, final_bitangents, vertex_count*3);
				
				delete[] final_tangents;
				delete[] final_bitangents;
			}
			else
			{
				//no tangents or bitangnets
				writeUnsignedShort(file, 0);
				writeUnsignedShort(file, 0);
			}

			//write bone weights & indices
			if( !dobones )
			{ writeUnsignedShort(file,0); }	//no bones
			else
			{
				writeUnsignedShort(file, STOOGE_BONE_WEIGHT_COUNT);	//this many bone weights
				writeArray<float>(file, final_boneweights, STOOGE_BONE_WEIGHT_COUNT * vertex_count);
				writeArray<int>(file, final_boneindices, STOOGE_BONE_WEIGHT_COUNT * vertex_count);
				
				delete[] final_boneweights;
				delete[] final_boneindices;
			}

			//generic attribs (placeholder for now)
			writeUnsignedShort(file, 0);

			//indices
			writeUnsignedInt(file, (unsigned)indices.size());
			for( unsigned int i=0; i<indices.size(); ++i )
			{ writeInt( file, indices[i].newindex ); }
			
			std::cout << "done." << std::endl;
			if( split )
			{
				fclose(file);
				writeRig( (std::string(basepath) + dagNode.partialPathName().asChar()).c_str(), false );
			}
			
			//MEL undo the triangulation
			MGlobal::executeCommand( "undo", true, true );
		}
	}

	//omg finished
	if( !split )
	{ fclose(file); }

	if( dobones && !split )
	{ writeSkeleton(basepath); }

	if( !split && userig )
	{ writeRig(basepath); }

	std::cout << "all done!" << std::endl;

	return true;
}

void	error( const char* err )
{ std::cout << "STOOGE ERROR: " << err << std::endl; }

void	warning( const char* warn )
{ std::cout << "STOOGE WARNING: " << warn << std::endl; }

int	getBoneID( const MDagPath& dagpath, std::vector<MDagPath>& dagpaths )
{
	for( unsigned int i=0; i<dagpaths.size(); ++i )
	{
		if( dagpaths[i] == dagpath )
		{ return i; }
	}
	std::cout << "BAD BONE INDEX? dagpath:" << dagpath.fullPathName().asChar() << std::endl;
	return -1;
}

inline bool	operator<( const MDagPath& a, const MDagPath& b )
{
	MString A = a.partialPathName();
	MString B = b.partialPathName();
	
	if( A.length() >= 2 )
	if( A.substring(0,1) == MString("dd") || A.substring(0,1) == MString("DD") )
	{ A = A.substring( 2, A.length()-1 ); }
	
	if( B.length() >= 2 )
	if( B.substring(0,1) == MString("dd") || B.substring(0,1) == MString("DD") )
	{ B = B.substring( 2, B.length()-1 ); }
	
	return (strcmp(A.asChar(), B.asChar()) < 0);
}

void	getBoneList( std::vector<MDagPath>& bonelist )
{
	bonelist.clear();
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
			std::string str( dagPath.partialPathName().asChar() );
			if( str.find(NOEXPORT_SUFFIX) == std::string::npos )
			{
				bonelist.push_back( dagPath );
			}
		}
	}
	
	//ensure that the ordering is the same across files
	std::sort( bonelist.begin(), bonelist.end() );
}

void	addKeyframe( double time, std::vector<double>& f )
{
	if(	time < MAnimControl::minTime().as(MTime::kSeconds) ||
		time > MAnimControl::maxTime().as(MTime::kSeconds) )
	{ return; }
	
	for( unsigned i=0; i < f.size(); ++i )
	{
		if( fabs(time-f[i]) < 0.001 )
		{ return; }
	}
	f.push_back(time);
}

void	getKeyframes( std::vector<double>& frames, float max_frame_interval )
{
	frames.clear();
	MItDependencyNodes it(MFn::kAnimCurve);
	while( !it.isDone() )
	{
		MObject obj = it.item();
		MFnAnimCurve fn(obj);
		
		unsigned keycount = fn.numKeys();
		if( keycount == 0 ) { it.next(); continue; }
		
		//std::cout << "animcurve " << fn.name().asChar() << ", numkeys: " << keycount << std::endl;
		
		MItKeyframe kfi(obj);
		while( !kfi.isDone() )
		{
			MTime t = kfi.time();
			double time = t.as(MTime::kSeconds);
			addKeyframe(time, frames);
			kfi.next();
		}
		it.next();
	}
	std::sort(frames.begin(), frames.end());
	std::cout << "actual keyframes: " << keyframes.size() << std::endl;
	
	//extra interpolation
	for( unsigned i=0; i<frames.size()-1; ++i )
	{
		if( frames[i]+max_frame_interval < frames[i+1] )
		{ frames.insert( frames.begin()+i+1, frames[i]+max_frame_interval ); }
	}
}

bool	writeAnimation( const char* basepath )
{
	std::cout << "writing animation...";
	if( keyframes.size() == 0 || bones.size() == 0 )
	{ error("no animation to write!"); }
	
	FILE*	file = fopen( (std::string(basepath) + STOOGE_ANIM_SUFFIX).c_str(), "wb" );
	if( file == 0 )
	{
		error("cant write animation file there!");
		return false;
	}
	
	//header
	writeUnsignedInt(file,0);
	
	//bonecount
	writeUnsignedShort(file, (unsigned short)bones.size());
	
	//framecount
	writeUnsignedShort(file, (unsigned short)keyframes.size());
	
	//animation length
	float l = keyframes.size() ? float(keyframes.back() - keyframes[0]) : 0.0f;
	std::cout << "Animation length: " << l << std::endl;
	writeFloat(file, l);
	
	//frames
	for( unsigned f=0; f<keyframes.size(); ++f )
	{
		//frame timestamp
		writeFloat(file, float(keyframes[f] - keyframes[0]));
		
		//set the animation time here
		MTime thetime;
		thetime.setUnit( MTime::kSeconds );
		thetime.setValue( keyframes[f] );
		MAnimControl::setCurrentTime(thetime);
		
		//bone positions in this frame
		for( unsigned b=0; b<bones.size(); ++b )
		{
			MFnIkJoint joint(bones[b]);
			float position[3];
			float rotation[4];
			getBoneTransform( joint, position, rotation );
			writeFloat(file, position[0]);
			writeFloat(file, position[1]);
			writeFloat(file, position[2]);
			
			writeFloat(file, rotation[0]);
			writeFloat(file, rotation[1]);
			writeFloat(file, rotation[2]);
			writeFloat(file, rotation[3]);
			
			unsigned flags = 0;
			if( isDead(bones[b]) ) { flags |= BONE_DEAD; }
			writeUnsignedInt(file, flags);
		}
	}
	fclose(file);
	std::cout << "done." << std::endl;
	return true;
}

bool    writeAnimationsFromText( const char *szBasePath, const char *szAnimFile, float max_frame_interval )
{
	if (NULL == szBasePath || NULL == szAnimFile)
		return false;

	std::fstream fileStream(szAnimFile);
	if (false == fileStream.is_open())
	{
		std::cout << "Animation Definition File not found: \"" << szAnimFile << "\"\n";
		return false;
	}

	char szAnimName[1024] = { '\0', };
	int iAnimStartFrame = 0;
	int iAnimEndFrame = 0;


	char szAnimPath[1024] = { '\0', };
	
	int iPathLen = (int)strlen(szBasePath);
	strcpy(szAnimPath, szBasePath);
	for ( ; iPathLen > 0; iPathLen--)
	{
		if (szBasePath[iPathLen] == '\\' || szBasePath[iPathLen] == '/')
			break;
	}

	iPathLen++;
	szAnimPath[iPathLen] = '\0';	//and the str

	while (true)
	{
		if (true == fileStream.eof())
			break;

		fileStream >> szAnimName;
		if (0 == _stricmp(szAnimName, "EndAnimationExport"))
			break;

		fileStream >> iAnimStartFrame >> iAnimEndFrame;

		MString m;		m = "-min ";		m += iAnimStartFrame;		m += " -max ";		m += iAnimEndFrame;
		std::cout << "m = " << m.asChar() << "\n";
		MGlobal::executeCommand("playbackOptions " + m);

		strcpy(&szAnimPath[iPathLen], szAnimName);

		getKeyframes(keyframes, max_frame_interval);
		if (false == writeAnimation(szAnimPath))
			std::cout << "Error: Animation File Not Saved correctly!  " << szAnimName << "\n";
	}

	//TODO:  Psuedo Code (Maya Never Works as Expected)
	//	while (!eof())
	//		read animName from file
	//		read animStartFrame
	//		read animEndFrame
	//		set playbackOptions using mel command:  playbackOptions -min animStartFrame -max animEndFrame
	//		call writeAnimation()

	fileStream.close();
	return true;
}

bool	writeSkeleton( const char* filepath )
{
	FILE*	file = fopen( (std::string(filepath) + STOOGE_SKELETON_SUFFIX).c_str(), "wb" );
	if( file == 0 )
	{ return false; }

	std::cout << "writing skeleton...\n";

	//header
	writeUnsignedInt(file,0);

	//bones
	int dbg_active = 0, dbg_inactive = 0;
	writeUnsignedShort(file, ushort(bones.size()));
	for( unsigned int i=0; i<bones.size(); ++i )
	{
		MFnIkJoint	joint(bones[i]);
		writeString(file, joint.partialPathName().asChar());
		
		float position[3];
		float rotation[4];
		
		getBoneTransform( joint, position, rotation );
		
		//translation vector
		writeFloat(file, position[0]);
		writeFloat(file, position[1]);
		writeFloat(file, position[2]);
		
		//rotation quaternion
		writeFloat(file, rotation[0]);
		writeFloat(file, rotation[1]);
		writeFloat(file, rotation[2]);
		writeFloat(file, rotation[3]);

		//find parent index
		int	parentindex = -1;
		if( joint.parentCount() > 1 )
		{ warning("joint has more than 1 parent! BAD PIE!"); }
		else if( joint.parentCount() != 0 )
		{
			MDagPath thepath = joint.dagPath();
			MObject parent;
			do
			{
				thepath.pop();
				parent = thepath.node();
			} while( !parent.hasFn( MFn::kJoint ) && thepath.length() > 0 );
			
			if( parent.hasFn( MFn::kJoint ) )
			{
				parentindex = getBoneID( thepath, bones );
				//std::cout << joint.partialPathName() << " has parent " << thepath.partialPathName() << std::endl;
			}
			else
			{
				warning( "joint has parent that isn't another joint!" );
				std::cout << bones[i].fullPathName() << std::endl;
			}
		}
		writeInt(file, parentindex);
	}

	fclose(file);
	std::cout << "done." << std::endl;
	return	true;
}

bool	writeRig( const char* basepath, bool allchunks )
{
	std::ofstream	f;
	std::string		bp(basepath);
	f.open((bp+STOOGE_RIG_SUFFIX).c_str());
	if( !f.is_open() )
	{ return false; }

	std::cout << "writing rig...";

	if( allchunks )
	{
		for( unsigned int i=0; i<chunks.size(); ++i )
		{ f << "MChunk\t\t" << chunks[i].name << "\t" << chunks[i].mtlname << "\n"; }
	}
	else
	{ f << "MChunk\t\t" << chunks[chunks.size()-1].name << "\t" << chunks[chunks.size()-1].mtlname << "\n"; }

	f.close();
	std::cout << "done." << std::endl;
	return true;
}

MMatrix	getNodeTransform( MFnTransform& node, bool nuke_scale=true )
{
	MTransformationMatrix x = node.transformation();
	if( nuke_scale )
	{
		double identity_scale[] = { 1.0, 1.0, 1.0 };
		x.setScale( identity_scale, MSpace::kTransform );
	}
	MMatrix mat = x.asMatrix();
	return mat;
}

void	getBoneTransform( MFnIkJoint& joint, float* pos, float* rot )
{
	MMatrix thismat = getNodeTransform( joint );
	
	if( joint.parentCount() > 0 )
	{
		MObject parentobj = joint.parent(0);
		while( parentobj.hasFn( MFn::kTransform ) && !parentobj.hasFn( MFn::kJoint ) )
		{
			MFnTransform parent(parentobj);
			thismat = thismat * getNodeTransform( parent );
			if( parent.parentCount() > 0 )
			{ parentobj = parent.parent(0); }
			else { break; }
		}
	}
	
	//scale stuff
	#if 1
		double scale[] = { 1.0, 1.0, 1.0 };
		MMatrix worldxform; //( thismat );
		if( joint.parentCount() > 0 )
		{
			MObject parentobj = joint.parent(0);
			while( parentobj.hasFn( MFn::kTransform ) )
			{
				MFnTransform parent(parentobj);
				double s[3];
				parent.transformation().getScale( s, MSpace::kTransform );
				scale[0] *= s[0];
				scale[1] *= s[1];
				scale[2] *= s[2];
				
				MTransformationMatrix pxform = parent.transformation();
				double unity[] = { 1.0, 1.0, 1.0 };
				pxform.setScale( unity, MSpace::kTransform );
				//worldxform = pxform.asMatrix() * worldxform;
				worldxform = worldxform * pxform.asMatrix();
				//worldxform = worldxform * parent.transformation().asMatrix();
				
				if( parent.parentCount() > 0 )
				{ parentobj = parent.parent(0); }
				else
				{ break; }
			}
		}
	#endif
	
	MQuaternion q; MPoint p;
	q.operator=(thismat);
	p.x = thismat(3,0);
	p.y = thismat(3,1);
	p.z = thismat(3,2);
	
	//more scale stuff
	#if 1
		p = worldxform * p;
		
		//adjust position based on scale (uh... i guess??)
		//std::cout << scale[0] << ',' << scale[1] << ',' << scale[2] << std::endl;
		
		p.x *= scale[0];
		p.y *= scale[1];
		p.z *= scale[2];
		
		worldxform[0][3] *= scale[0];
		worldxform[1][3] *= scale[1];
		worldxform[2][3] *= scale[2];
		
		p = worldxform.inverse() * p;
	#endif
	
	rot[0] = float(q.w); rot[1] = float(q.x);
	rot[2] = float(q.y); rot[3] = float(q.z);
	
	pos[0] = float(p.x*CM_TO_FEET);
	pos[1] = float(p.y*CM_TO_FEET);
	pos[2] = float(p.z*CM_TO_FEET);
}


class IndexSortItem
{
public:
	int		index;
	float	weight;
	
	bool	operator<( const IndexSortItem& i2 ) const
	{ return index > i2.index; }
};

void	sortBoneStuff(	const std::vector<int>& indices, const std::vector<float>& weights,
						std::vector<int>& sorted_indices, std::vector<float>& sorted_weights )
{
	float total_wt = 0.0f;
	std::vector<IndexSortItem> sorted;
	for( unsigned i=0; i<indices.size(); ++i )
	{
		IndexSortItem item;
		item.index = indices[i];
		item.weight = weights[i];
		sorted.push_back(item);
		total_wt += (indices[i] >= 0 ? weights[i] : 0.0f);
	}
	std::sort(sorted.begin(), sorted.end());

	for( unsigned i=0; i<sorted.size(); ++i )
	{
		sorted_indices.push_back( sorted[i].index );
		sorted_weights.push_back( sorted[i].weight/total_wt );
	}
}

bool	isDead( MDagPath& bone )
{
	MFnIkJoint	joint(bone);
	std::string path(joint.partialPathName().asChar());
	if( path.size() >= 2 )
	if( (path[0] == 'd' &&  path[1] == 'd') ||
		(path[0] == 'D' &&  path[1] == 'D')	)
	{ return true; }
	return false;
}

bool	isMeshVisible(const MFnDagNode &dagNode)
{
	MStatus statusCheck;

	MPlug visPlug = dagNode.findPlug("visibility", &statusCheck);
	if (MStatus::kSuccess == statusCheck.statusCode())
	{
		int iVisible;
		if (MStatus::kSuccess == visPlug.getValue(iVisible).statusCode() && 0 == iVisible)
			return false;
	}
	//else
	//{
	//	std::cout << "  DagNode: \"" << dagNode.name() << "\" did not have visibility attr.\n";
	//}	

	for (unsigned int uiParent = 0; uiParent < 1; uiParent++)
	{
		MObject objParent = dagNode.parent(uiParent);
		if (true == objParent.hasFn(MFn::kDagNode))
		{
			MFnDagNode dagParent(objParent);
			if (false == isMeshVisible(objParent))
				return false;	
		}
		//else
		//{
		//	std::cout << "  Not A Node\n";
		//}
	}

	return true;	//Visible unless we see that it is invisible.
}


/*
In case it bears repeating:
sizeof(int) = 4
sizeof(float) = 4
sizeof(half) = 2
sizeof(char) = 1
and strings are cstrings

everything is stored in LITTLE ENDIAN byte order - NO EXCEPTIONS!
this means that big endian builds (ppc architecture specifically)
will need to swap byte order on floats, ints, and halfs at both read and write.

Stooge Mesh File Format:
<header>
	uint	flags = 0
	uint	numchunks
</header>

<chunks>
	<chunk>
		string	chunkname
		uint	numvertices
		
		ushort	haspositions (nonzero means there are positions)
		float	positions[numvertices * 3]
		
		ushort	hasnormals
		float	normals[numvertices * 3]

		ushort	colorcomponents (typically 3 or 4, 0 means no colors)
		float	colors[colorcomponents * numvertices] (ordering will be r,g,b,a)

		ushort	texcoordsets
		<texcoords>
			<texcoord>
				ushort	texcomponents
				float	texcoords[texcomponents * numvertices]
			</texcoord>
			...
		</texcoords>

		ushort	tangentcomponents (typically 0,3, or 4. 4th component would be handedness multiplier for generating bitangents)
		float	tangents[tangentcomponents * numvertices]

		ushort	bitangentcomponents (typically 0,3, or 4. 4th component would be handedness multiplier for generating tangents)
		float	bitangents[bitangentcomponents * numvertices]

		ushort	boneweightcount
		float	boneweights[boneweightcount * numvertices]
		int		boneindices[boneweightcount * numvertices]

		ushort	genericattribcount
		<generics>
			<generic>
				string	name
				ushort	componentcount
				float	genericattrib[componentcount * numvertices]
			</generic>
			...
		</generics>


		uint	indexcount
		uint	indices[indexcount]
		(if there are no indices, then the previous arrays should be taken to be 'straight' vertex arrays)
	</chunk>
	...
</chunks>


Stooge Rig File Format (text):
Rig		<rig name>\n
Skel	<skeleton file name>\n

M		<mesh file name>\n
MChunk	<mesh chunk name>	<material string = "placeholdermaterial">\n ...


Stooge Bone Flags:
define	BONE_DEAD	0x00000001	//dont animate this bone

Stooge Skeleton File Format:
<header>
uint	flags = 0
</header>

ushort	bonecount
<joints>
	<joint>
		string	name
		float	position[3]	(world coordinates)
		float	rotation[4] (world coordinates, quaternion: [w,x,y,z])
		int		parent		(-1 means no parent)
	</joint>
	...
</joints>


Stooge Animation File Format:
<header>
	uint flags = 0
</header>

ushort bonecount
ushort framecount
float  length
<frames>
	<frame>
		float	timemark
		<boneframes>
			<boneframe>
				float	position[3] (world coordinates)
				float	rotation[4]	(world coordinates)
				uint	flags (see above)
			</boneframe>
			...
		</boneframes>
	</frame>
	...
</frames>
*/

