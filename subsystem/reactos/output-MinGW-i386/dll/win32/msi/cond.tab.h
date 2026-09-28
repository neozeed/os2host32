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

#ifndef YY_COND_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_MSI_COND_TAB_H_INCLUDED
# define YY_COND_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_MSI_COND_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef COND_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define COND_DEBUG 1
#  else
#   define COND_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define COND_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined COND_DEBUG */
#if COND_DEBUG
extern int cond_debug;
#endif

/* Token type.  */
#ifndef COND_TOKENTYPE
# define COND_TOKENTYPE
  enum cond_tokentype
  {
    COND_SPACE = 258,
    COND_OR = 259,
    COND_AND = 260,
    COND_NOT = 261,
    COND_XOR = 262,
    COND_IMP = 263,
    COND_EQV = 264,
    COND_LT = 265,
    COND_GT = 266,
    COND_EQ = 267,
    COND_NE = 268,
    COND_GE = 269,
    COND_LE = 270,
    COND_ILT = 271,
    COND_IGT = 272,
    COND_IEQ = 273,
    COND_INE = 274,
    COND_IGE = 275,
    COND_ILE = 276,
    COND_LPAR = 277,
    COND_RPAR = 278,
    COND_TILDA = 279,
    COND_SS = 280,
    COND_ISS = 281,
    COND_ILHS = 282,
    COND_IRHS = 283,
    COND_LHS = 284,
    COND_RHS = 285,
    COND_PERCENT = 286,
    COND_DOLLARS = 287,
    COND_QUESTION = 288,
    COND_AMPER = 289,
    COND_EXCLAM = 290,
    COND_IDENT = 291,
    COND_NUMBER = 292,
    COND_LITER = 293,
    COND_ERROR = 294
  };
#endif

/* Value type.  */
#if ! defined COND_STYPE && ! defined COND_STYPE_IS_DECLARED
union COND_STYPE
{
#line 121 "C:/proj/reactos/dll/win32/msi/cond.y"

    struct cond_str str;
    struct value value;
    LPWSTR identifier;
    INT operator;
    BOOL boolean;

#line 113 "C:/proj/reactos/output-MinGW-i386/dll/win32/msi/cond.tab.h"

};
typedef union COND_STYPE COND_STYPE;
# define COND_STYPE_IS_TRIVIAL 1
# define COND_STYPE_IS_DECLARED 1
#endif



int cond_parse (COND_input *info);

#endif /* !YY_COND_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_MSI_COND_TAB_H_INCLUDED  */
