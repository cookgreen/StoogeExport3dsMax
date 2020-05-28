/*
	File IO Helper
	2005, Jeff Russell

	Cuz fwrite() and fread() aren't good enough.
	Maintains little endian (i.e. x86) order for all primitives on disk.
	
	FOR USE WITH THE STOOGE EXPORTER ONLY!!!!
	**NOT** COMPATIBLE WITH THE REST OF THE M8 NAMESPACE
*/

#ifndef _FILE_IO_HELPER_H_
#define _FILE_IO_HELPER_H_

#include <stdio.h>
#include <string>

#if defined(__APPLE__)
	#define	STOOGE_BIG_ENDIAN
#else
	#define STOOGE_LITTLE_ENDIAN
#endif

#define	_leet_swapbytes(a,b)	{(a)^=(b); (b)^=(a); (a)^=(b);}

namespace m8
{

typedef unsigned int	uint;
#ifndef __APPLE__
typedef unsigned short	ushort;
#endif
typedef unsigned char	uchar;
typedef unsigned char	Byte;

//double word reversal
inline void fix64( Byte* b )
{
	#ifdef STOOGE_BIG_ENDIAN
	_leet_swapbytes(b[0], b[7]);
	_leet_swapbytes(b[1], b[6]);
	_leet_swapbytes(b[2], b[5]);
	_leet_swapbytes(b[3], b[4]);
	#endif
}

//word reversal
inline void	fix32( Byte* b )
{
	#ifdef STOOGE_BIG_ENDIAN
	_leet_swapbytes(b[0], b[3]);
	_leet_swapbytes(b[1], b[2]);
	#endif
}

//half word reversal
inline void	fix16( Byte* b )
{
	#ifdef STOOGE_BIG_ENDIAN
	_leet_swapbytes(b[0], b[1]);
	#endif
}

//----- read functions
inline long long int readLong( FILE* file )
{
	long long int	i;
	fread( &i, 8, 1, file );
	fix64( (Byte*)(&i) );
	return i;
}

inline unsigned long long int readUnsignedLong( FILE* file )
{
	unsigned long long int	i;
	fread( &i, 8, 1, file );
	fix64( (Byte*)(&i) );
	return i;
}

inline int	readInt( FILE* file )
{
	int	i;
	fread( &i, 4, 1, file );
	fix32( (Byte*)(&i) );
	return i;
}

inline uint readUnsignedInt( FILE* file )
{
	uint	i;
	fread( &i, 4, 1, file );
	fix32( (Byte*)(&i) );
	return i;
}

inline short readShort( FILE* file )
{
	short	s;
	fread( &s, 2, 1, file );
	fix16( (Byte*)(&s) );
	return s;
}

inline ushort readUnsignedShort( FILE* file )
{
	ushort	s;
	fread( &s, 2, 1, file );
	fix16( (Byte*)(&s) );
	return s;
}

inline float	readFloat( FILE* file )
{
	float	f;
	fread( &f, 4, 1, file );
	fix32( (Byte*)(&f) );
	return f;
}

inline double	readDouble( FILE* file )
{
	double d;
	fread( &d, 8, 1, file );
	fix64( (Byte*)(&d) );
	return d;
}

inline std::string	readString( FILE* file )
{
	std::string s;
	char c;
	do
	{
		if( fread( &c, 1, 1, file ) && c != '\0')
		{ s += c; }
		else break;
	} while(1);
	
	return s;
}

//should really only be applied to arrays of primitives, not structs or anything
template <typename typeage>
inline void	readArray( FILE* file, typeage* dst, int size )
{
	fread( dst, sizeof(typeage), size, file );
#ifdef STOOGE_BIG_ENDIAN
	if( sizeof(typeage) == 8 )
	{
		for( uint i=0; i<size; ++i )
		{ fix64( (Byte*)(&(dst[i])) ); }
	}
	else if( sizeof(typeage) == 4 )
	{
		for( uint i=0; i<size; ++i )
		{ fix32( (Byte*)(&(dst[i])) ); }
	}
	else if( sizeof(typeage) == 2 )
	{
		for( uint i=0; i<size; ++i )
		{ fix16( (Byte*)(&(dst[i])) ); }
	}
#endif
}


//----- write functions
inline void write64( FILE* file, void* data )
{
	fix64((Byte*)data);
	fwrite( data, 8, 1, file );
}

inline void	write32( FILE* file, void* data )
{
	fix32((Byte*)data);
	fwrite( data, 4, 1, file );
}

inline void	write16( FILE* file, void* data )
{
	fix16((Byte*)data);
	fwrite( data, 2, 1, file );
}

inline void writeFloat( FILE* f, float flt )
{ write32( f, &flt ); }

inline void writeDouble( FILE* f, double dbl )
{ write64( f, &dbl ); }

inline void writeInt( FILE* f, int i )
{ write32( f, &i ); }

inline void writeUnsignedInt( FILE* f, uint i )
{ write32( f, &i ); }

inline void writeLong( FILE* f, long long int i )
{ write64( f, &i ); }

inline void writeUnsignedLong( FILE* f, unsigned long long int i )
{ write64( f, &i ); }

inline void writeShort( FILE* f, short s )
{ write16( f, &s ); }

inline void writeUnsignedShort( FILE* f, ushort s )
{ write16( f, &s ); }

inline void writeString( FILE* f, const char* str )
{
	int i = -1;
	do
	{
		++i;
		fwrite( &(str[i]), 1, 1, f );
	} while( str[i] != '\0' );
}

template <typename typeage>
inline void	writeArray( FILE* file, const typeage* src, int size )
{
#ifdef STOOGE_BIG_ENDIAN
	typeage*	new_src = new typeage[size];
	memcpy( new_src, src, size * sizeof(typeage) );
	if( sizeof(typeage) == 8 )
	{
		for( int i=0; i<size; ++i )
		{ fix64( (Byte*)(&(new_src[i])) ); }
	}
	else if( sizeof(typeage) == 4 )
	{
		for( int i=0; i<size; ++i )
		{ fix32( (Byte*)(&(new_src[i])) ); }
	}
	else if( sizeof(typeage) == 2 )
	{
		for( int i=0; i<size; ++i )
		{ fix16( (Byte*)(&(new_src[i])) ); }
	}
	fwrite( new_src, sizeof(typeage), size, file);
	delete [] new_src;
#else
	fwrite( src, sizeof(typeage), size, file );
#endif
}

}


#endif