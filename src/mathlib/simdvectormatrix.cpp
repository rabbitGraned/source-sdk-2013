//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide a class (SSE/SIMD only) holding a 2d matrix of class FourVectors,
// for high speed processing in tools.
//
// $NoKeywords: $
//
//=============================================================================//

#include "basetypes.h"
#include "mathlib/mathlib.h"
#include "mathlib/simdvectormatrix.h"
#include "mathlib/ssemath.h"
#include "tier0/dbg.h"

#ifndef _MM_TRANSPOSE4_PS
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3) do { \
    __m128 tmp0, tmp1, tmp2, tmp3; \
    tmp0 = _mm_unpacklo_ps((row0), (row1)); \
    tmp1 = _mm_unpackhi_ps((row0), (row1)); \
    tmp2 = _mm_unpacklo_ps((row2), (row3)); \
    tmp3 = _mm_unpackhi_ps((row2), (row3)); \
    (row0) = _mm_movelh_ps(tmp0, tmp2); \
    (row1) = _mm_movehl_ps(tmp2, tmp0); \
    (row2) = _mm_movelh_ps(tmp1, tmp3); \
    (row3) = _mm_movehl_ps(tmp3, tmp1); \
} while (0)
#endif	// _MM_TRANSPOSE4_PS

void CSIMDVectorMatrix::CreateFromRGBA_FloatImageData(int srcwidth, int srcheight,
													  float const *srcdata )
{
	Assert( srcwidth && srcheight && srcdata );
	SetSize( srcwidth, srcheight );

	int n_vectors_per_source_line = (srcwidth >> 2);
	int ntrailing_pixels_per_source_line = (srcwidth & 3);

	for (int y = 0; y < srcheight; y++)
	{
		float const *data_in = srcdata;
		FourVectors *p_out = m_pData + y * m_nPaddedWidth;  // ADD: Use row-based addressing to avoid pointer drift

		// copy full input blocks
		// ADD: Replaced scalar copying with SIMD transpose for performance
		for (int x = 0; x < n_vectors_per_source_line; x++)
		{
			__m128 r0 = _mm_loadu_ps(data_in + 0);
			__m128 r1 = _mm_loadu_ps(data_in + 4);
			__m128 r2 = _mm_loadu_ps(data_in + 8);
			__m128 r3 = _mm_loadu_ps(data_in + 12);

			_MM_TRANSPOSE4_PS(r0, r1, r2, r3);
			// Now: r0 = [R0,R1,R2,R3], r1 = [G0,G1,G2,G3], r2 = [B0,B1,B2,B3]

			p_out->x = r0;
			p_out->y = r1;
			p_out->z = r2;
			p_out++;
			data_in += 16;
		}

		// now, copy trailing data and pad with copies
		if (ntrailing_pixels_per_source_line)
		{
			// ADD: Use temporary arrays to avoid strict aliasing violations
			float temp_x[4], temp_y[4], temp_z[4];
			for (int cp = 0; cp < 4; cp++)
			{
				int real_cp = min(cp, ntrailing_pixels_per_source_line - 1);
				temp_x[cp] = data_in[4 * real_cp + 0];
				temp_y[cp] = data_in[4 * real_cp + 1];
				temp_z[cp] = data_in[4 * real_cp + 2];
			}

			p_out->x = _mm_loadu_ps(temp_x);
			p_out->y = _mm_loadu_ps(temp_y);
			p_out->z = _mm_loadu_ps(temp_z);
			// p_out is not incremented further — it's the end of the row
		}

		// advance source ptr to next line
		srcdata += 4 * srcwidth;
	}
}

void CSIMDVectorMatrix::RaiseToPower( float power )
{
	int nv = NVectors();
	if ( nv )
	{
		// ADD: Round instead of truncate for better fixed-point accuracy
		int fixed_point_exp = (int)floorf(4.0f * power + 0.5f);
		FourVectors *src = m_pData;
		do
		{
			src->x = Pow_FixedPoint_Exponent_SIMD( src->x, fixed_point_exp );
			src->y = Pow_FixedPoint_Exponent_SIMD( src->y, fixed_point_exp );
			src->z = Pow_FixedPoint_Exponent_SIMD( src->z, fixed_point_exp );
			src++;
		} while (--nv);
	}
}

CSIMDVectorMatrix & CSIMDVectorMatrix::operator+=( CSIMDVectorMatrix const &src )
{
	Assert( m_nWidth == src.m_nWidth );
	Assert( m_nHeight == src.m_nHeight );
	int nv = NVectors();
	if ( nv )
	{
		FourVectors *srcv = src.m_pData;
		FourVectors *destv = m_pData;
		do													// !! speed !! inline more iters
		{
			*( destv++ ) += *( srcv++ );
		} while ( --nv );
	}
	return *this;
}

CSIMDVectorMatrix & CSIMDVectorMatrix::operator*=( Vector const &src )
{
	int nv = NVectors();
	if ( nv )
	{
		FourVectors scalevalue;
		scalevalue.DuplicateVector( src );
		FourVectors *destv = m_pData;
		do													// !! speed !! inline more iters
		{
			destv->VProduct( scalevalue );
			destv++;
		} while ( --nv );
	}
	return *this;
}