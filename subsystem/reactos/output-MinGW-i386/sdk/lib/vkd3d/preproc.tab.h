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

#ifndef YY_PREPROC_YY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_SDK_LIB_VKD3D_PREPROC_TAB_H_INCLUDED
# define YY_PREPROC_YY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_SDK_LIB_VKD3D_PREPROC_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef PREPROC_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define PREPROC_YYDEBUG 1
#  else
#   define PREPROC_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define PREPROC_YYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined PREPROC_YYDEBUG */
#if PREPROC_YYDEBUG
extern int preproc_yydebug;
#endif
/* "%code requires" blocks.  */
#line 22 "C:/proj/reactos/sdk/lib/vkd3d/libs/vkd3d-shader/preproc.y"


#include "vkd3d_shader_private.h"
#include "preproc.h"
#include <stdio.h>
#include <sys/stat.h>

#define PREPROC_YYLTYPE struct vkd3d_shader_location

struct parse_arg_names
{
    char **args;
    size_t count;
};


#line 73 "C:/proj/reactos/output-MinGW-i386/sdk/lib/vkd3d/preproc.tab.h"

/* Token type.  */
#ifndef PREPROC_YYTOKENTYPE
# define PREPROC_YYTOKENTYPE
  enum preproc_yytokentype
  {
    T_HASHSTRING = 258,
    T_IDENTIFIER = 259,
    T_IDENTIFIER_PAREN = 260,
    T_INTEGER = 261,
    T_STRING = 262,
    T_TEXT = 263,
    T_NEWLINE = 264,
    T_DEFINE = 265,
    T_ERROR = 266,
    T_ELIF = 267,
    T_ELSE = 268,
    T_ENDIF = 269,
    T_IF = 270,
    T_IFDEF = 271,
    T_IFNDEF = 272,
    T_INCLUDE = 273,
    T_LINE = 274,
    T_PRAGMA = 275,
    T_UNDEF = 276,
    T_CONCAT = 277,
    T_LE = 278,
    T_GE = 279,
    T_EQ = 280,
    T_NE = 281,
    T_AND = 282,
    T_OR = 283,
    T_DEFINED = 284
  };
#endif

/* Value type.  */
#if ! defined PREPROC_YYSTYPE && ! defined PREPROC_YYSTYPE_IS_DECLARED
union PREPROC_YYSTYPE
{
#line 263 "C:/proj/reactos/sdk/lib/vkd3d/libs/vkd3d-shader/preproc.y"

    char *string;
    const char *const_string;
    uint32_t integer;
    struct vkd3d_string_buffer string_buffer;
    struct parse_arg_names arg_names;

#line 122 "C:/proj/reactos/output-MinGW-i386/sdk/lib/vkd3d/preproc.tab.h"

};
typedef union PREPROC_YYSTYPE PREPROC_YYSTYPE;
# define PREPROC_YYSTYPE_IS_TRIVIAL 1
# define PREPROC_YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined PREPROC_YYLTYPE && ! defined PREPROC_YYLTYPE_IS_DECLARED
typedef struct PREPROC_YYLTYPE PREPROC_YYLTYPE;
struct PREPROC_YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define PREPROC_YYLTYPE_IS_DECLARED 1
# define PREPROC_YYLTYPE_IS_TRIVIAL 1
#endif



int preproc_yyparse (void *scanner, struct preproc_ctx *ctx);
/* "%code provides" blocks.  */
#line 40 "C:/proj/reactos/sdk/lib/vkd3d/libs/vkd3d-shader/preproc.y"


int preproc_yylex(PREPROC_YYSTYPE *yylval_param, PREPROC_YYLTYPE *yylloc_param, void *scanner);


#line 154 "C:/proj/reactos/output-MinGW-i386/sdk/lib/vkd3d/preproc.tab.h"

#endif /* !YY_PREPROC_YY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_SDK_LIB_VKD3D_PREPROC_TAB_H_INCLUDED  */
