/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_MOC_YACC_H_INCLUDED
# define YY_YY_MOC_YACC_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    CHAR_VAL = 258,                /* CHAR_VAL  */
    INT_VAL = 259,                 /* INT_VAL  */
    DOUBLE_VAL = 260,              /* DOUBLE_VAL  */
    STRING = 261,                  /* STRING  */
    IDENTIFIER = 262,              /* IDENTIFIER  */
    FRIEND = 263,                  /* FRIEND  */
    TYPEDEF = 264,                 /* TYPEDEF  */
    AUTO = 265,                    /* AUTO  */
    REGISTER = 266,                /* REGISTER  */
    STATIC = 267,                  /* STATIC  */
    EXTERN = 268,                  /* EXTERN  */
    INLINE = 269,                  /* INLINE  */
    VIRTUAL = 270,                 /* VIRTUAL  */
    CONST = 271,                   /* CONST  */
    VOLATILE = 272,                /* VOLATILE  */
    CHAR = 273,                    /* CHAR  */
    SHORT = 274,                   /* SHORT  */
    INT = 275,                     /* INT  */
    LONG = 276,                    /* LONG  */
    SIGNED = 277,                  /* SIGNED  */
    UNSIGNED = 278,                /* UNSIGNED  */
    FLOAT = 279,                   /* FLOAT  */
    DOUBLE = 280,                  /* DOUBLE  */
    VOID = 281,                    /* VOID  */
    ENUM = 282,                    /* ENUM  */
    CLASS = 283,                   /* CLASS  */
    STRUCT = 284,                  /* STRUCT  */
    UNION = 285,                   /* UNION  */
    ASM = 286,                     /* ASM  */
    PRIVATE = 287,                 /* PRIVATE  */
    PROTECTED = 288,               /* PROTECTED  */
    PUBLIC = 289,                  /* PUBLIC  */
    OPERATOR = 290,                /* OPERATOR  */
    DBL_COLON = 291,               /* DBL_COLON  */
    TRIPLE_DOT = 292,              /* TRIPLE_DOT  */
    TEMPLATE = 293,                /* TEMPLATE  */
    NAMESPACE = 294,               /* NAMESPACE  */
    USING = 295,                   /* USING  */
    MUTABLE = 296,                 /* MUTABLE  */
    THROW = 297,                   /* THROW  */
    SIGNALS = 298,                 /* SIGNALS  */
    SLOTS = 299,                   /* SLOTS  */
    Q_OBJECT = 300,                /* Q_OBJECT  */
    Q_PROPERTY = 301,              /* Q_PROPERTY  */
    Q_OVERRIDE = 302,              /* Q_OVERRIDE  */
    Q_CLASSINFO = 303,             /* Q_CLASSINFO  */
    Q_ENUMS = 304,                 /* Q_ENUMS  */
    Q_SETS = 305,                  /* Q_SETS  */
    READ = 306,                    /* READ  */
    WRITE = 307,                   /* WRITE  */
    STORED = 308,                  /* STORED  */
    DESIGNABLE = 309,              /* DESIGNABLE  */
    SCRIPTABLE = 310,              /* SCRIPTABLE  */
    RESET = 311                    /* RESET  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define CHAR_VAL 258
#define INT_VAL 259
#define DOUBLE_VAL 260
#define STRING 261
#define IDENTIFIER 262
#define FRIEND 263
#define TYPEDEF 264
#define AUTO 265
#define REGISTER 266
#define STATIC 267
#define EXTERN 268
#define INLINE 269
#define VIRTUAL 270
#define CONST 271
#define VOLATILE 272
#define CHAR 273
#define SHORT 274
#define INT 275
#define LONG 276
#define SIGNED 277
#define UNSIGNED 278
#define FLOAT 279
#define DOUBLE 280
#define VOID 281
#define ENUM 282
#define CLASS 283
#define STRUCT 284
#define UNION 285
#define ASM 286
#define PRIVATE 287
#define PROTECTED 288
#define PUBLIC 289
#define OPERATOR 290
#define DBL_COLON 291
#define TRIPLE_DOT 292
#define TEMPLATE 293
#define NAMESPACE 294
#define USING 295
#define MUTABLE 296
#define THROW 297
#define SIGNALS 298
#define SLOTS 299
#define Q_OBJECT 300
#define Q_PROPERTY 301
#define Q_OVERRIDE 302
#define Q_CLASSINFO 303
#define Q_ENUMS 304
#define Q_SETS 305
#define READ 306
#define WRITE 307
#define STORED 308
#define DESIGNABLE 309
#define SCRIPTABLE 310
#define RESET 311

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 692 "moc.y"

    char	char_val;
    int		int_val;
    double	double_val;
    const char *string;
    Access	access;
    Function   *function;
    ArgList    *arg_list;
    Argument   *arg;

#line 190 "moc_yacc.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_MOC_YACC_H_INCLUDED  */
