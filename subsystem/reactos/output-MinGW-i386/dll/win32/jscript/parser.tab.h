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

#ifndef YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_JSCRIPT_PARSER_TAB_H_INCLUDED
# define YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_JSCRIPT_PARSER_TAB_H_INCLUDED
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
    kBREAK = 258,
    kCASE = 259,
    kCATCH = 260,
    kCONST = 261,
    kCONTINUE = 262,
    kDEFAULT = 263,
    kDELETE = 264,
    kDO = 265,
    kELSE = 266,
    kFUNCTION = 267,
    kIF = 268,
    kFINALLY = 269,
    kFOR = 270,
    kGET = 271,
    kIN = 272,
    kLET = 273,
    kSET = 274,
    kINSTANCEOF = 275,
    kNEW = 276,
    kNULL = 277,
    kRETURN = 278,
    kSWITCH = 279,
    kTHIS = 280,
    kTHROW = 281,
    kTRUE = 282,
    kFALSE = 283,
    kTRY = 284,
    kTYPEOF = 285,
    kVAR = 286,
    kVOID = 287,
    kWHILE = 288,
    kWITH = 289,
    tANDAND = 290,
    tOROR = 291,
    tINC = 292,
    tDEC = 293,
    tHTMLCOMMENT = 294,
    kDIVEQ = 295,
    kDCOL = 296,
    tIdentifier = 297,
    tAssignOper = 298,
    tEqOper = 299,
    tShiftOper = 300,
    tRelOper = 301,
    tNumericLiteral = 302,
    tBooleanLiteral = 303,
    tStringLiteral = 304,
    LOWER_THAN_ELSE = 305
  };
#endif

/* Value type.  */
#if ! defined PARSER_STYPE && ! defined PARSER_STYPE_IS_DECLARED
union PARSER_STYPE
{
#line 142 "C:/proj/reactos/dll/win32/jscript/parser.y"

    int                     ival;
    jsstr_t                 *str;
    literal_t               *literal;
    struct _argument_list_t *argument_list;
    case_clausule_t         *case_clausule;
    struct _case_list_t     *case_list;
    catch_block_t           *catch_block;
    struct _element_list_t  *element_list;
    expression_t            *expr;
    const WCHAR            *identifier;
    struct _parameter_list_t *parameter_list;
    struct _property_list_t *property_list;
    property_definition_t   *property_definition;
    statement_t             *statement;
    struct _statement_list_t *statement_list;
    struct _variable_list_t *variable_list;
    variable_declaration_t  *variable_declaration;

#line 136 "C:/proj/reactos/output-MinGW-i386/dll/win32/jscript/parser.tab.h"

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

#endif /* !YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_DLL_WIN32_JSCRIPT_PARSER_TAB_H_INCLUDED  */
