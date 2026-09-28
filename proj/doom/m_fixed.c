/* Emacs style mode select   -*- C++ -*- */
/*----------------------------------------------------------------------------- */
/* */
/* $Id:$ */
/* */
/* Copyright (C) 1993-1996 by id Software, Inc. */
/* */
/* This program is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU General Public License */
/* as published by the Free Software Foundation; either version 2 */
/* of the License, or (at your option) any later version. */
/* */
/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */
/* */
/* $Log:$ */
/* */
/* DESCRIPTION: */
/*	Fixed point implementation. */
/* */
/*----------------------------------------------------------------------------- */

/*
static const char
        rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";
*/
#include "stdlib.h"

#include "doomtype.h"
#include "i_system.h"

#ifdef __GNUG__
#pragma implementation "m_fixed.h"
#endif
#include "m_fixed.h"


#if 0
#ifdef VALUES_H
#include <values.h>
#endif
#ifdef __WATCOMC__
#define long_long long long
#else
#define long_long long long
#endif

/* Fixme. __USE_C_FIXED__ or something. */

fixed_t
FixedMul
        ( fixed_t a,
        fixed_t b )
{
	return ((long_long) a * (long_long) b) >> FRACBITS;
}



/* */
/* FixedDiv, C version. */
/* */

fixed_t
FixedDiv
        ( fixed_t a,
        fixed_t b )
{
	if ( (abs(a)>>14) >= abs(b))
		return (a^b)<0 ? MININT : MAXINT;
	return FixedDiv2 (a,b);
}



fixed_t
FixedDiv2
        ( fixed_t a,
        fixed_t b )
{
#if 1
	long_long c;
	c = ((long_long)a<<16) / ((long_long)b);
	return (fixed_t) c;
#else
	double c;

	c = ((double)a) / ((double)b) * FRACUNIT;

	if (c >= 2147483648.0 || c < -2147483648.0)
		I_Error("FixedDiv: divide by zero");
	return (fixed_t) c;
#endif
}

#endif


/* Fixme. __USE_C_FIXED__ or something. */
fixed_t
FixedMul
        ( fixed_t a,
        fixed_t b )
{
#ifdef HAVE_LONG_LONG
    return ((long long) a * (long long) b) >> FRACBITS;
#else
    unsigned int ah,al,bh,bl,result;

    ah = (a >> FRACBITS);
    al = (a & (FRACUNIT-1));
    bh = (b >> FRACBITS);
    bl = (b & (FRACUNIT-1));

    /* Multiply the parts separately    */
    result = (ah * bh) << FRACBITS;     /* High*High    */
    result += ah * bl;                  /* High*Low     */
    result += al * bh;                  /* Low*High     */
    /* Low*Low part doesn't need to be calculated because it doesn't contribute to the result after shifting

    // Shift right by FRACBITS to get the fixed-point result                                            */
    result += (al * bl) >> FRACBITS;

    return (fixed_t)result;
#endif
}

/* */
/* FixedDiv, C version. */
/* */
fixed_t
FixedDiv2
        ( fixed_t a,
        fixed_t b )
{
#ifdef HAVE_LONG_LONG
        long long c;
        c = ((long long)a<<16) / ((long long)b);
        return (fixed_t) c;
#else
        double c;

        c = ((double)a) / ((double)b) * FRACUNIT;

        if (c >= 2147483648.0 || c < -2147483648.0)
                I_Error("FixedDiv: divide by zero");
        return (fixed_t) c;
#endif
}

fixed_t
FixedDiv
        ( fixed_t a,
        fixed_t b )
{
        if ( (abs(a)>>14) >= abs(b))
                return (a^b)<0 ? MININT : MAXINT;
        return FixedDiv2 (a,b);
}
