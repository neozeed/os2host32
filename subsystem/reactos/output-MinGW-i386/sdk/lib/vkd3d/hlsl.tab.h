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

#ifndef YY_HLSL_YY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_SDK_LIB_VKD3D_HLSL_TAB_H_INCLUDED
# define YY_HLSL_YY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_SDK_LIB_VKD3D_HLSL_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef HLSL_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define HLSL_YYDEBUG 1
#  else
#   define HLSL_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define HLSL_YYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined HLSL_YYDEBUG */
#if HLSL_YYDEBUG
extern int hlsl_yydebug;
#endif
/* "%code requires" blocks.  */
#line 24 "C:/proj/reactos/sdk/lib/vkd3d/libs/vkd3d-shader/hlsl.y"


#include "hlsl.h"
#include <stdio.h>

#define HLSL_YYLTYPE struct vkd3d_shader_location

struct parse_fields
{
    struct hlsl_struct_field *fields;
    size_t count, capacity;
};

struct parse_initializer
{
    struct hlsl_ir_node **args;
    unsigned int args_count;
    struct hlsl_block *instrs;
    bool braces;
    struct vkd3d_shader_location loc;
};

struct parse_parameter
{
    struct hlsl_type *type;
    const char *name;
    struct hlsl_semantic semantic;
    struct hlsl_reg_reservation reg_reservation;
    uint32_t modifiers;
    struct parse_initializer initializer;
};

struct parse_colon_attributes
{
    struct hlsl_semantic semantic;
    struct hlsl_reg_reservation reg_reservation;
};

struct parse_array_sizes
{
    uint32_t *sizes; /* innermost first */
    unsigned int count;
};

struct parse_variable_def
{
    struct list entry;
    struct vkd3d_shader_location loc;

    char *name;
    struct parse_array_sizes arrays;
    struct hlsl_semantic semantic;
    struct hlsl_reg_reservation reg_reservation;
    struct parse_initializer initializer;
    struct hlsl_scope *annotations;

    struct hlsl_type *basic_type;
    uint32_t modifiers;
    struct vkd3d_shader_location modifiers_loc;

    struct hlsl_state_block **state_blocks;
    unsigned int state_block_count;
    size_t state_block_capacity;
};

struct parse_function
{
    struct hlsl_ir_function_decl *decl;
    struct hlsl_func_parameters parameters;
    struct hlsl_semantic return_semantic;
    bool first;
};

struct parse_if_body
{
    struct hlsl_block *then_block;
    struct hlsl_block *else_block;
};

enum parse_assign_op
{
    ASSIGN_OP_ASSIGN,
    ASSIGN_OP_ADD,
    ASSIGN_OP_SUB,
    ASSIGN_OP_MUL,
    ASSIGN_OP_DIV,
    ASSIGN_OP_MOD,
    ASSIGN_OP_LSHIFT,
    ASSIGN_OP_RSHIFT,
    ASSIGN_OP_AND,
    ASSIGN_OP_OR,
    ASSIGN_OP_XOR,
};

struct parse_attribute_list
{
    unsigned int count;
    const struct hlsl_attribute **attrs;
};

struct state_block_index
{
    bool has_index;
    unsigned int index;
};


#line 164 "C:/proj/reactos/output-MinGW-i386/sdk/lib/vkd3d/hlsl.tab.h"

/* Token type.  */
#ifndef HLSL_YYTOKENTYPE
# define HLSL_YYTOKENTYPE
  enum hlsl_yytokentype
  {
    KW_BLENDSTATE = 258,
    KW_BREAK = 259,
    KW_BUFFER = 260,
    KW_BYTEADDRESSBUFFER = 261,
    KW_CASE = 262,
    KW_CONSTANTBUFFER = 263,
    KW_CBUFFER = 264,
    KW_CENTROID = 265,
    KW_COLUMN_MAJOR = 266,
    KW_COMPILE = 267,
    KW_COMPILESHADER = 268,
    KW_COMPUTESHADER = 269,
    KW_CONST = 270,
    KW_CONSTRUCTGSWITHSO = 271,
    KW_CONTINUE = 272,
    KW_DEFAULT = 273,
    KW_DEPTHSTENCILSTATE = 274,
    KW_DEPTHSTENCILVIEW = 275,
    KW_DISCARD = 276,
    KW_DO = 277,
    KW_DOMAINSHADER = 278,
    KW_ELSE = 279,
    KW_EXPORT = 280,
    KW_EXTERN = 281,
    KW_FALSE = 282,
    KW_FOR = 283,
    KW_FXGROUP = 284,
    KW_GEOMETRYSHADER = 285,
    KW_GROUPSHARED = 286,
    KW_HULLSHADER = 287,
    KW_IF = 288,
    KW_IN = 289,
    KW_INLINE = 290,
    KW_INOUT = 291,
    KW_LINEAR = 292,
    KW_MATRIX = 293,
    KW_NAMESPACE = 294,
    KW_NOINTERPOLATION = 295,
    KW_NOPERSPECTIVE = 296,
    KW_NULL = 297,
    KW_OUT = 298,
    KW_PACKOFFSET = 299,
    KW_PASS = 300,
    KW_PIXELSHADER = 301,
    KW_RASTERIZERORDEREDBUFFER = 302,
    KW_RASTERIZERORDEREDSTRUCTUREDBUFFER = 303,
    KW_RASTERIZERORDEREDTEXTURE1D = 304,
    KW_RASTERIZERORDEREDTEXTURE1DARRAY = 305,
    KW_RASTERIZERORDEREDTEXTURE2D = 306,
    KW_RASTERIZERORDEREDTEXTURE2DARRAY = 307,
    KW_RASTERIZERORDEREDTEXTURE3D = 308,
    KW_RASTERIZERSTATE = 309,
    KW_RENDERTARGETVIEW = 310,
    KW_RETURN = 311,
    KW_REGISTER = 312,
    KW_ROW_MAJOR = 313,
    KW_RWBUFFER = 314,
    KW_RWBYTEADDRESSBUFFER = 315,
    KW_RWSTRUCTUREDBUFFER = 316,
    KW_RWTEXTURE1D = 317,
    KW_RWTEXTURE1DARRAY = 318,
    KW_RWTEXTURE2D = 319,
    KW_RWTEXTURE2DARRAY = 320,
    KW_RWTEXTURE3D = 321,
    KW_SAMPLER = 322,
    KW_SAMPLER1D = 323,
    KW_SAMPLER2D = 324,
    KW_SAMPLER3D = 325,
    KW_SAMPLERCUBE = 326,
    KW_SAMPLER_STATE = 327,
    KW_SAMPLERCOMPARISONSTATE = 328,
    KW_SHARED = 329,
    KW_SNORM = 330,
    KW_STATEBLOCK = 331,
    KW_STATEBLOCK_STATE = 332,
    KW_STATIC = 333,
    KW_STRING = 334,
    KW_STRUCT = 335,
    KW_SWITCH = 336,
    KW_TBUFFER = 337,
    KW_TECHNIQUE = 338,
    KW_TECHNIQUE10 = 339,
    KW_TECHNIQUE11 = 340,
    KW_TEXTURE = 341,
    KW_TEXTURE1D = 342,
    KW_TEXTURE1DARRAY = 343,
    KW_TEXTURE2D = 344,
    KW_TEXTURE2DARRAY = 345,
    KW_TEXTURE2DMS = 346,
    KW_TEXTURE2DMSARRAY = 347,
    KW_TEXTURE3D = 348,
    KW_TEXTURECUBE = 349,
    KW_TEXTURECUBEARRAY = 350,
    KW_TRUE = 351,
    KW_TYPEDEF = 352,
    KW_UNSIGNED = 353,
    KW_UNIFORM = 354,
    KW_UNORM = 355,
    KW_VECTOR = 356,
    KW_VERTEXSHADER = 357,
    KW_VOID = 358,
    KW_VOLATILE = 359,
    KW_WHILE = 360,
    OP_INC = 361,
    OP_DEC = 362,
    OP_AND = 363,
    OP_OR = 364,
    OP_EQ = 365,
    OP_LEFTSHIFT = 366,
    OP_LEFTSHIFTASSIGN = 367,
    OP_RIGHTSHIFT = 368,
    OP_RIGHTSHIFTASSIGN = 369,
    OP_LE = 370,
    OP_GE = 371,
    OP_NE = 372,
    OP_ADDASSIGN = 373,
    OP_SUBASSIGN = 374,
    OP_MULASSIGN = 375,
    OP_DIVASSIGN = 376,
    OP_MODASSIGN = 377,
    OP_ANDASSIGN = 378,
    OP_ORASSIGN = 379,
    OP_XORASSIGN = 380,
    C_FLOAT = 381,
    C_INTEGER = 382,
    C_UNSIGNED = 383,
    PRE_LINE = 384,
    VAR_IDENTIFIER = 385,
    NEW_IDENTIFIER = 386,
    STRING = 387,
    TYPE_IDENTIFIER = 388
  };
#endif

/* Value type.  */
#if ! defined HLSL_YYSTYPE && ! defined HLSL_YYSTYPE_IS_DECLARED
union HLSL_YYSTYPE
{
#line 6532 "C:/proj/reactos/sdk/lib/vkd3d/libs/vkd3d-shader/hlsl.y"

    struct hlsl_type *type;
    INT intval;
    FLOAT floatval;
    bool boolval;
    char *name;
    uint32_t modifiers;
    struct hlsl_ir_node *instr;
    struct hlsl_block *block;
    struct list *list;
    struct parse_fields fields;
    struct parse_function function;
    struct parse_parameter parameter;
    struct hlsl_func_parameters parameters;
    struct parse_initializer initializer;
    struct parse_array_sizes arrays;
    struct parse_variable_def *variable_def;
    struct parse_if_body if_body;
    enum parse_assign_op assign_op;
    struct hlsl_reg_reservation reg_reservation;
    struct parse_colon_attributes colon_attributes;
    struct hlsl_semantic semantic;
    enum hlsl_buffer_type buffer_type;
    enum hlsl_sampler_dim sampler_dim;
    struct hlsl_attribute *attr;
    struct parse_attribute_list attr_list;
    struct hlsl_ir_switch_case *switch_case;
    struct hlsl_scope *scope;
    struct hlsl_state_block *state_block;
    struct state_block_index state_block_index;

#line 341 "C:/proj/reactos/output-MinGW-i386/sdk/lib/vkd3d/hlsl.tab.h"

};
typedef union HLSL_YYSTYPE HLSL_YYSTYPE;
# define HLSL_YYSTYPE_IS_TRIVIAL 1
# define HLSL_YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined HLSL_YYLTYPE && ! defined HLSL_YYLTYPE_IS_DECLARED
typedef struct HLSL_YYLTYPE HLSL_YYLTYPE;
struct HLSL_YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define HLSL_YYLTYPE_IS_DECLARED 1
# define HLSL_YYLTYPE_IS_TRIVIAL 1
#endif



int hlsl_yyparse (void *scanner, struct hlsl_ctx *ctx);
/* "%code provides" blocks.  */
#line 133 "C:/proj/reactos/sdk/lib/vkd3d/libs/vkd3d-shader/hlsl.y"


int yylex(HLSL_YYSTYPE *yylval_param, HLSL_YYLTYPE *yylloc_param, void *yyscanner);


#line 373 "C:/proj/reactos/output-MinGW-i386/sdk/lib/vkd3d/hlsl.tab.h"

#endif /* !YY_HLSL_YY_C_PROJ_REACTOS_OUTPUT_MINGW_I386_SDK_LIB_VKD3D_HLSL_TAB_H_INCLUDED  */
