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

#ifndef YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_HOST_TOOLS_BIN_SDK_TOOLS_WIDL_PARSER_TAB_H_INCLUDED
# define YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_HOST_TOOLS_BIN_SDK_TOOLS_WIDL_PARSER_TAB_H_INCLUDED
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
/* "%code requires" blocks.  */
#line 110 "C:/proj/reactos/sdk/tools/widl/parser.y"


#define PARSER_LTYPE struct location


#line 62 "C:/proj/reactos/output-MinGW-i386/host-tools/bin/sdk/tools/widl/parser.tab.h"

/* Token type.  */
#ifndef PARSER_TOKENTYPE
# define PARSER_TOKENTYPE
  enum parser_tokentype
  {
    aIDENTIFIER = 258,
    aPRAGMA = 259,
    aKNOWNTYPE = 260,
    aNUM = 261,
    aHEXNUM = 262,
    aDOUBLE = 263,
    aSTRING = 264,
    aWSTRING = 265,
    aSQSTRING = 266,
    tCDECL = 267,
    tFASTCALL = 268,
    tPASCAL = 269,
    tSTDCALL = 270,
    aUUID = 271,
    aEOF = 272,
    aACF = 273,
    SHL = 274,
    SHR = 275,
    MEMBERPTR = 276,
    EQUALITY = 277,
    INEQUALITY = 278,
    GREATEREQUAL = 279,
    LESSEQUAL = 280,
    LOGICALOR = 281,
    LOGICALAND = 282,
    ELLIPSIS = 283,
    tACTIVATABLE = 284,
    tAGGREGATABLE = 285,
    tAGILE = 286,
    tALLNODES = 287,
    tALLOCATE = 288,
    tANNOTATION = 289,
    tAPICONTRACT = 290,
    tAPPOBJECT = 291,
    tASYNC = 292,
    tASYNCUUID = 293,
    tAUTOHANDLE = 294,
    tBINDABLE = 295,
    tBOOLEAN = 296,
    tBROADCAST = 297,
    tBYTE = 298,
    tBYTECOUNT = 299,
    tCALLAS = 300,
    tCALLBACK = 301,
    tCASE = 302,
    tCHAR = 303,
    tCOCLASS = 304,
    tCODE = 305,
    tCOMMSTATUS = 306,
    tCOMPOSABLE = 307,
    tCONST = 308,
    tCONTEXTHANDLE = 309,
    tCONTEXTHANDLENOSERIALIZE = 310,
    tCONTEXTHANDLESERIALIZE = 311,
    tCONTRACT = 312,
    tCONTRACTVERSION = 313,
    tCONTROL = 314,
    tCPPQUOTE = 315,
    tCUSTOM = 316,
    tDECLARE = 317,
    tDECODE = 318,
    tDEFAULT = 319,
    tDEFAULTBIND = 320,
    tDELEGATE = 321,
    tDEFAULT_OVERLOAD = 322,
    tDEFAULTCOLLELEM = 323,
    tDEFAULTVALUE = 324,
    tDEFAULTVTABLE = 325,
    tDEPRECATED = 326,
    tDISABLECONSISTENCYCHECK = 327,
    tDISPLAYBIND = 328,
    tDISPINTERFACE = 329,
    tDLLNAME = 330,
    tDONTFREE = 331,
    tDOUBLE = 332,
    tDUAL = 333,
    tENABLEALLOCATE = 334,
    tENCODE = 335,
    tENDPOINT = 336,
    tENTRY = 337,
    tENUM = 338,
    tERRORSTATUST = 339,
    tEVENTADD = 340,
    tEVENTREMOVE = 341,
    tEXCLUSIVETO = 342,
    tEXPLICITHANDLE = 343,
    tEXTERN = 344,
    tFALSE = 345,
    tFAULTSTATUS = 346,
    tFLAGS = 347,
    tFLOAT = 348,
    tFORCEALLOCATE = 349,
    tHANDLE = 350,
    tHANDLET = 351,
    tHELPCONTEXT = 352,
    tHELPFILE = 353,
    tHELPSTRING = 354,
    tHELPSTRINGCONTEXT = 355,
    tHELPSTRINGDLL = 356,
    tHIDDEN = 357,
    tHYPER = 358,
    tID = 359,
    tIDEMPOTENT = 360,
    tIGNORE = 361,
    tIIDIS = 362,
    tIMMEDIATEBIND = 363,
    tIMPLICITHANDLE = 364,
    tIMPORT = 365,
    tIMPORTLIB = 366,
    tIN = 367,
    tIN_LINE = 368,
    tINLINE = 369,
    tINPUTSYNC = 370,
    tINT = 371,
    tINT32 = 372,
    tINT3264 = 373,
    tINT64 = 374,
    tINTERFACE = 375,
    tLCID = 376,
    tLENGTHIS = 377,
    tLIBRARY = 378,
    tLICENSED = 379,
    tLOCAL = 380,
    tLONG = 381,
    tMARSHALINGBEHAVIOR = 382,
    tMAYBE = 383,
    tMESSAGE = 384,
    tMETHODS = 385,
    tMODULE = 386,
    tMTA = 387,
    tNAMESPACE = 388,
    tNOCODE = 389,
    tNONBROWSABLE = 390,
    tNONCREATABLE = 391,
    tNONE = 392,
    tNONEXTENSIBLE = 393,
    tNOTIFY = 394,
    tNOTIFYFLAG = 395,
    tNULL = 396,
    tOBJECT = 397,
    tODL = 398,
    tOLEAUTOMATION = 399,
    tOPTIMIZE = 400,
    tOPTIONAL = 401,
    tOUT = 402,
    tOVERLOAD = 403,
    tPARTIALIGNORE = 404,
    tPOINTERDEFAULT = 405,
    tPRAGMA_WARNING = 406,
    tPROGID = 407,
    tPROPERTIES = 408,
    tPROPGET = 409,
    tPROPPUT = 410,
    tPROPPUTREF = 411,
    tPROTECTED = 412,
    tPROXY = 413,
    tPTR = 414,
    tPUBLIC = 415,
    tRANGE = 416,
    tREADONLY = 417,
    tREF = 418,
    tREGISTER = 419,
    tREPRESENTAS = 420,
    tREQUESTEDIT = 421,
    tREQUIRES = 422,
    tRESTRICTED = 423,
    tRETVAL = 424,
    tRUNTIMECLASS = 425,
    tSAFEARRAY = 426,
    tSHORT = 427,
    tSIGNED = 428,
    tSINGLENODE = 429,
    tSIZEIS = 430,
    tSIZEOF = 431,
    tSMALL = 432,
    tSOURCE = 433,
    tSTANDARD = 434,
    tSTATIC = 435,
    tSTRICTCONTEXTHANDLE = 436,
    tSTRING = 437,
    tSTRUCT = 438,
    tSWITCH = 439,
    tSWITCHIS = 440,
    tSWITCHTYPE = 441,
    tTHREADING = 442,
    tTRANSMITAS = 443,
    tTRUE = 444,
    tTYPEDEF = 445,
    tUIDEFAULT = 446,
    tUNION = 447,
    tUNIQUE = 448,
    tUNSIGNED = 449,
    tUSESGETLASTERROR = 450,
    tUSERMARSHAL = 451,
    tUUID = 452,
    tV1ENUM = 453,
    tVARARG = 454,
    tVERSION = 455,
    tVIPROGID = 456,
    tVOID = 457,
    tWCHAR = 458,
    tWIREMARSHAL = 459,
    tAPARTMENT = 460,
    tNEUTRAL = 461,
    tSINGLE = 462,
    tFREE = 463,
    tBOTH = 464,
    CAST = 465,
    PPTR = 466,
    POS = 467,
    NEG = 468,
    ADDRESSOF = 469
  };
#endif

/* Value type.  */
#if ! defined PARSER_STYPE && ! defined PARSER_STYPE_IS_DECLARED
union PARSER_STYPE
{
#line 134 "C:/proj/reactos/sdk/tools/widl/parser.y"

	attr_t *attr;
	attr_list_t *attr_list;
	str_list_t *str_list;
	expr_t *expr;
	expr_list_t *expr_list;
	type_t *type;
	var_t *var;
	var_list_t *var_list;
	declarator_t *declarator;
	declarator_list_t *declarator_list;
	statement_t *statement;
	statement_list_t *stmt_list;
	warning_t *warning;
	warning_list_t *warning_list;
	typeref_t *typeref;
	typeref_list_t *typeref_list;
	char *str;
	struct uuid *uuid;
	unsigned int num;
	struct integer integer;
	double dbl;
	typelib_t *typelib;
	struct _import_t *import;
	struct _decl_spec_t *declspec;
	enum storage_class stgclass;
	enum type_qualifier type_qualifier;
	enum function_specifier function_specifier;
	struct namespace *namespace;

#line 319 "C:/proj/reactos/output-MinGW-i386/host-tools/bin/sdk/tools/widl/parser.tab.h"

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



int parser_parse (void);
/* "%code provides" blocks.  */
#line 117 "C:/proj/reactos/sdk/tools/widl/parser.y"


int parser_lex( PARSER_STYPE *yylval, PARSER_LTYPE *yylloc );
void push_import( const char *fname, PARSER_LTYPE *yylloc );
PARSER_LTYPE pop_import(void);

# define YYLLOC_DEFAULT( cur, rhs, n ) \
        do { if (n) init_location( &(cur), &YYRHSLOC( rhs, 1 ), &YYRHSLOC( rhs, n ) ); \
             else init_location( &(cur), &YYRHSLOC( rhs, 0 ), NULL ); } while(0)


#line 357 "C:/proj/reactos/output-MinGW-i386/host-tools/bin/sdk/tools/widl/parser.tab.h"

#endif /* !YY_PARSER_C_PROJ_REACTOS_OUTPUT_MINGW_I386_HOST_TOOLS_BIN_SDK_TOOLS_WIDL_PARSER_TAB_H_INCLUDED  */
