/* A Bison parser, made by GNU Bison 3.5.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_PPY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_HOST_TOOLS_BIN_SDK_TOOLS_WPP_PPY_TAB_H_INCLUDED
# define YY_PPY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_HOST_TOOLS_BIN_SDK_TOOLS_WPP_PPY_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef PPY_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define PPY_DEBUG 1
#  else
#   define PPY_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define PPY_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined PPY_DEBUG */
#if PPY_DEBUG
extern int ppy_debug;
#endif

/* Token type.  */
#ifndef PPY_TOKENTYPE
# define PPY_TOKENTYPE
  enum ppy_tokentype
  {
    tRCINCLUDE = 258,
    tIF = 259,
    tIFDEF = 260,
    tIFNDEF = 261,
    tELSE = 262,
    tELIF = 263,
    tENDIF = 264,
    tDEFINED = 265,
    tNL = 266,
    tINCLUDE = 267,
    tLINE = 268,
    tGCCLINE = 269,
    tERROR = 270,
    tWARNING = 271,
    tPRAGMA = 272,
    tPPIDENT = 273,
    tUNDEF = 274,
    tMACROEND = 275,
    tCONCAT = 276,
    tELLIPSIS = 277,
    tSTRINGIZE = 278,
    tIDENT = 279,
    tLITERAL = 280,
    tMACRO = 281,
    tDEFINE = 282,
    tDQSTRING = 283,
    tSQSTRING = 284,
    tIQSTRING = 285,
    tUINT = 286,
    tSINT = 287,
    tULONG = 288,
    tSLONG = 289,
    tULONGLONG = 290,
    tSLONGLONG = 291,
    tRCINCLUDEPATH = 292,
    tLOGOR = 293,
    tLOGAND = 294,
    tEQ = 295,
    tNE = 296,
    tLTE = 297,
    tGTE = 298,
    tLSHIFT = 299,
    tRSHIFT = 300
  };
#endif

/* Value type.  */
#if ! defined PPY_STYPE && ! defined PPY_STYPE_IS_DECLARED
union PPY_STYPE
{
#line 121 "C:/proj/reactos/sdk/tools/wpp/ppy.y"

	int		sint;
	unsigned int	uint;
	long		slong;
	unsigned long	ulong;
	__int64		sll;
	unsigned __int64 ull;
	int		*iptr;
	char		*cptr;
	cval_t		cval;
	char		*marg;
	mtext_t		*mtext;

#line 125 "C:/proj/reactos/output-MinGW-i386/host-tools/bin/sdk/tools/wpp/ppy.tab.h"

};
typedef union PPY_STYPE PPY_STYPE;
# define PPY_STYPE_IS_TRIVIAL 1
# define PPY_STYPE_IS_DECLARED 1
#endif


extern PPY_STYPE ppy_lval;

int ppy_parse (void);

#endif /* !YY_PPY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_HOST_TOOLS_BIN_SDK_TOOLS_WPP_PPY_TAB_H_INCLUDED  */
