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

#ifndef YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_VBSCRIPT_PARSER_TAB_H_INCLUDED
# define YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_VBSCRIPT_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef PARSER_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define PARSER_DEBUG 1
#  else
#   define PARSER_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define PARSER_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined PARSER_DEBUG */
#if PARSER_DEBUG
extern int parser_debug;
#endif

/* Token type.  */
#ifndef PARSER_TOKENTYPE
# define PARSER_TOKENTYPE
  enum parser_tokentype
  {
    tEXPRESSION = 258,
    tNL = 259,
    tEMPTYBRACKETS = 260,
    tEXPRLBRACKET = 261,
    tLTEQ = 262,
    tGTEQ = 263,
    tNEQ = 264,
    tSTOP = 265,
    tME = 266,
    tREM = 267,
    tDOT = 268,
    tTRUE = 269,
    tFALSE = 270,
    tNOT = 271,
    tAND = 272,
    tOR = 273,
    tXOR = 274,
    tEQV = 275,
    tIMP = 276,
    tIS = 277,
    tMOD = 278,
    tCALL = 279,
    tSUB = 280,
    tFUNCTION = 281,
    tGET = 282,
    tLET = 283,
    tCONST = 284,
    tDIM = 285,
    tREDIM = 286,
    tPRESERVE = 287,
    tIF = 288,
    tELSE = 289,
    tELSEIF = 290,
    tEND = 291,
    tTHEN = 292,
    tEXIT = 293,
    tWHILE = 294,
    tWEND = 295,
    tDO = 296,
    tLOOP = 297,
    tUNTIL = 298,
    tFOR = 299,
    tTO = 300,
    tEACH = 301,
    tIN = 302,
    tSELECT = 303,
    tCASE = 304,
    tWITH = 305,
    tBYREF = 306,
    tBYVAL = 307,
    tOPTION = 308,
    tNOTHING = 309,
    tEMPTY = 310,
    tNULL = 311,
    tCLASS = 312,
    tSET = 313,
    tNEW = 314,
    tPUBLIC = 315,
    tPRIVATE = 316,
    tNEXT = 317,
    tON = 318,
    tRESUME = 319,
    tGOTO = 320,
    tIdentifier = 321,
    tString = 322,
    tDEFAULT = 323,
    tERROR = 324,
    tEXPLICIT = 325,
    tPROPERTY = 326,
    tSTEP = 327,
    tInt = 328,
    tDouble = 329,
    tDate = 330
  };
#endif

/* Value type.  */
#if ! defined PARSER_STYPE && ! defined PARSER_STYPE_IS_DECLARED
union PARSER_STYPE
{
#line 96 "C:/proj/reactos/dll/win32/vbscript/parser.y"

    const WCHAR *string;
    statement_t *statement;
    expression_t *expression;
    member_expression_t *member;
    elseif_decl_t *elseif;
    dim_decl_t *dim_decl;
    dim_list_t *dim_list;
    redim_decl_t *redim_decl;
    function_decl_t *func_decl;
    arg_decl_t *arg_decl;
    class_decl_t *class_decl;
    const_decl_t *const_decl;
    case_clausule_t *case_clausule;
    unsigned uint;
    LONG integer;
    BOOL boolean;
    double dbl;
    DATE date;

#line 162 "C:/proj/reactos/output-MinGW-i386/dll/win32/vbscript/parser.tab.h"

};
typedef union PARSER_STYPE PARSER_STYPE;
# define PARSER_STYPE_IS_TRIVIAL 1
# define PARSER_STYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined PARSER_LTYPE && ! defined PARSER_LTYPE_IS_DECLARED
typedef struct PARSER_LTYPE PARSER_LTYPE;
struct PARSER_LTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define PARSER_LTYPE_IS_DECLARED 1
# define PARSER_LTYPE_IS_TRIVIAL 1
#endif



int parser_parse (parser_ctx_t *ctx);

#endif /* !YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_VBSCRIPT_PARSER_TAB_H_INCLUDED  */
