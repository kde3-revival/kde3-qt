/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 57 "moc.y"

#define MOC_YACC_CODE
void yyerror( const char *msg );

#include "qplatformdefs.h"
#include "qasciidict.h"
#include "qdatetime.h"
#include "qdict.h"
#include "qfile.h"
#include "qdir.h"
#include "qptrlist.h"
#include "qregexp.h"
#include "qstrlist.h"
#ifdef MOC_MWERKS_PLUGIN
# ifdef Q_OS_MACX
#  undef OLD_DEBUG
#  ifdef DEBUG
#   define OLD_DEBUG DEBUG
#   undef DEBUG
#  endif
#  define DEBUG 0
#  ifndef __IMAGECAPTURE__
#   define __IMAGECAPTURE__
#  endif
#  include <Carbon/Carbon.h>
# endif
# include "mwerks_mac.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#if defined CONST
#undef CONST
#endif
#if defined VOID
#undef VOID
#endif

bool isEnumType( const char* type );
int enumIndex( const char* type );
bool isVariantType( const char* type );
int qvariant_nameToType( const char* name );
static void init();				// initialize
static void initClass();			// prepare for new class
static void generateClass();			// generate C++ code for class
static void initExpression();			// prepare for new expression
static void enterNameSpace( const char *name = 0 );
static void leaveNameSpace();
static void selectOutsideClassState();
static void registerClassInNamespace();
static bool suppress_func_warn = FALSE;
static void func_warn( const char *msg );
static void moc_warn( const char *msg );
static void moc_err( const char *s );
static void moc_err( const char *s1, const char *s2 );
static void operatorError();
static void checkPropertyName( const char* ident );

static const char* const utype_map[] =
{
    "bool",
    "int",
    "double",
    "QString",
    "QVariant",
    0
};

inline bool isIdentChar( char x )
{						// Avoid bug in isalnum
    return x == '_' || (x >= '0' && x <= '9') ||
	 (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z');
}

bool validUType( QCString ctype )
{
     if ( ctype.left(6) == "const " )
	ctype = ctype.mid( 6, ctype.length() - 6 );
    if ( ctype.right(1) == "&" )
	ctype = ctype.left( ctype.length() - 1 );
    else if ( ctype.right(1) == "*" )
	return TRUE;

    int i = -1;
    while ( utype_map[++i] )
	if ( ctype == utype_map[i] )
	    return TRUE;

    return isEnumType( ctype );
}

QCString castToUType( QCString ctype )
{
     if ( ctype.right(1) == "&" )
	 ctype = ctype.left( ctype.length() - 1 );
     if( ctype.right(1) == "]") {
	 int lb = ctype.findRev('[');
	 if(lb != -1)
	     ctype = ctype.left(lb) + "*";
     }
     return ctype;
}

QCString rawUType( QCString ctype )
{
    ctype = castToUType( ctype );
    if ( ctype.left(6) == "const " )
	ctype = ctype.mid( 6, ctype.length() - 6 );
    return ctype;
}

QCString uType( QCString ctype )
{
    if ( !validUType( ctype ) ) {
	if ( isVariantType( rawUType(ctype) ) )
	    return "varptr";
	else
	    return "ptr";
    }
    if ( ctype.left(6) == "const " )
	ctype = ctype.mid( 6, ctype.length() - 6 );
    if ( ctype.right(1) == "&" ) {
	ctype = ctype.left( ctype.length() - 1 );
    } else if ( ctype.right(1) == "*" ) {
	QCString raw = ctype.left( ctype.length() - 1 );
	ctype = "ptr";
	if ( raw == "char" )
	    ctype = "charstar";
	else if ( raw == "QUnknownInterface" )
	    ctype = "iface";
	else if ( raw == "QDispatchInterface" )
	    ctype = "idisp";
	else if ( isVariantType( raw ) )
	    ctype = "varptr";
    }
    if ( isEnumType( ctype ) )
	ctype = "enum";
    return ctype;
}

bool isInOut( QCString ctype )
{
    if ( ctype.left(6) == "const " )
	return FALSE;
    if ( ctype.right(1) == "&" )
	return TRUE;
    if ( ctype.right(2) == "**" )
	return TRUE;
    return FALSE;
}

QCString uTypeExtra( QCString ctype )
{
    QCString typeExtra = "0";
    if ( !validUType( ctype ) ) {
	if ( isVariantType( rawUType(ctype) ) )
	    typeExtra.sprintf("\"\\x%02x\"", qvariant_nameToType( rawUType(ctype) ) );
	else
	    typeExtra.sprintf( "\"%s\"", rawUType(ctype).data() );
	return typeExtra;
    }
    if ( ctype.left(6) == "const " )
	ctype = ctype.mid( 6, ctype.length() - 6 );
    if ( ctype.right(1) == "&" )
	ctype = ctype.left( ctype.length() - 1 );
    if ( ctype.right(1) == "*" ) {
	QCString raw = ctype.left( ctype.length() - 1 );
	ctype = "ptr";
	if ( raw == "char" )
	    ;
	else if ( isVariantType( raw ) )
	    typeExtra.sprintf("\"\\x%02x\"", qvariant_nameToType( raw ) );
	else
	    typeExtra.sprintf( "\"%s\"", raw.stripWhiteSpace().data() );

    } else if ( isEnumType( ctype ) ) {
	int idx = enumIndex( ctype );
	if ( idx >= 0 ) {
	    typeExtra.sprintf( "&enum_tbl[%d]", enumIndex( ctype ) );
	} else {
	    typeExtra.sprintf( "parentObject->enumerator(\"%s\", TRUE )", ctype.data() );
	}
	typeExtra =
	    "\n#ifndef QT_NO_PROPERTIES\n\t  " + typeExtra +
	    "\n#else"
	    "\n\t  0"
	    "\n#endif // QT_NO_PROPERTIES\n\t  ";
    }
    return typeExtra;
}

/*
  Attention!
  This table is copied from qvariant.cpp. If you change
  one, change both.
*/
static const int ntypes = 35;
static const char* const type_map[ntypes] =
{
    0,
    "QMap<QString,QVariant>",
    "QValueList<QVariant>",
    "QString",
    "QStringList",
    "QFont",
    "QPixmap",
    "QBrush",
    "QRect",
    "QSize",
    "QColor",
    "QPalette",
    "QColorGroup",
    "QIconSet",
    "QPoint",
    "QImage",
    "int",
    "uint",
    "bool",
    "double",
    "QCString",
    "QPointArray",
    "QRegion",
    "QBitmap",
    "QCursor",
    "QSizePolicy",
    "QDate",
    "QTime",
    "QDateTime",
    "QByteArray",
    "QBitArray",
    "QKeySequence",
    "QPen",
    "Q_LLONG",
    "Q_ULLONG"
};

int qvariant_nameToType( const char* name )
{
    for ( int i = 0; i < ntypes; i++ ) {
	if ( !qstrcmp( type_map[i], name ) )
	    return i;
    }
    return 0;
}

/*
  Returns TRUE if the type is a QVariant types.
*/
bool isVariantType( const char* type )
{
    return qvariant_nameToType( type ) != 0;
}

/*
  Replaces '>>' with '> >' (as in 'QValueList<QValueList<double> >').
  This function must be called to produce valid C++ code. However,
  the string representation still uses '>>'.
*/
void fixRightAngles( QCString *str )
{
    str->replace( QRegExp(">>"), "> >" );
}

static QCString rmWS( const char * );

enum Access { Private, Protected, Public };


class Argument					// single arg meta data
{
public:
    Argument( const char *left, const char *right, const char* argName = 0, bool isDefaultArgument = FALSE )
    {
	leftType = rmWS( left );
	rightType = rmWS( right );
	if ( leftType == "void" && rightType.isEmpty() )
	    leftType = "";

	int len = leftType.length();

	/*
	  Convert 'char const *' into 'const char *'. Start at index 1,
	  not 0, because 'const char *' is already OK.
	*/
	for ( int i = 1; i < len; i++ ) {
	    if ( leftType[i] == 'c' &&
		 strncmp(leftType.data() + i + 1, "onst", 4) == 0
                 && (i + 5 >= len || !isIdentChar(leftType[i + 5]))
                 && !isIdentChar(i-1)
                ) {
		leftType.remove( i, 5 );
		if ( leftType[i - 1] == ' ' )
		    leftType.remove( i - 1, 1 );
		leftType.prepend( "const " );
		break;
	    }

	    /*
	      We musn't convert 'char * const *' into 'const char **'
	      and we must beware of 'Bar<const Bla>'.
	    */
	    if ( leftType[i] == '&' || leftType[i] == '*' ||
		 leftType[i] == '<' )
		break;
	}

	name = argName;
	isDefault = isDefaultArgument;
    }

    QCString leftType;
    QCString rightType;
    QCString name;
    bool isDefault;
};

class ArgList : public QPtrList<Argument> {	// member function arg list
public:
    ArgList() { setAutoDelete( TRUE ); }
    ~ArgList() { clear(); }

    /* the clone has one default argument less, the orignal has all default arguments removed */
    ArgList* magicClone() {
	ArgList* l = new ArgList;
	bool firstDefault = FALSE;
	for ( first(); current(); next() ) {
	    bool isDefault = current()->isDefault;
	    if ( !firstDefault && isDefault ) {
		isDefault = FALSE;
		firstDefault = TRUE;
	    }
	    l->append( new Argument( current()->leftType, current()->rightType, current()->name, isDefault ) );
	}
	for ( first(); current(); ) {
	    if ( current()->isDefault )
		remove();
	    else
		next();
	}
	return l;
    }

    bool hasDefaultArguments() {
	for ( Argument* a = first(); a; a = next() ) {
	    if ( a->isDefault )
		return TRUE;
	}
	return FALSE;
    }

};


struct Function					// member function meta data
{
    Access access;
    QCString    qualifier;			// const or volatile
    QCString    name;
    QCString    type;
    QCString    signature;
    int	       lineNo;
    ArgList   *args;
    Function() { args=0;}
   ~Function() { delete args; }
    const char* accessAsString() {
	switch ( access ) {
	case Private: return "Private";
	case Protected: return "Protected";
	default: return "Public";
	}
    }
};

class FuncList : public QPtrList<Function> {	// list of member functions
public:
    FuncList( bool autoDelete = FALSE ) { setAutoDelete( autoDelete ); }

    FuncList find( const char* name )
    {
	FuncList result;
	for ( QPtrListIterator<Function> it(*this); it.current(); ++it ) {
	    if ( it.current()->name == name )
		result.append( it.current() );
	}
	return result;
    }
};

class Enum : public QStrList
{
public:
    QCString name;
    bool set;
};

class EnumList : public QPtrList<Enum> {		// list of property enums
public:
    EnumList() { setAutoDelete(TRUE); }
};


struct Property
{
    Property( int l, const char* t, const char* n, const char* s, const char* g, const char* r,
	      const QCString& st, const QCString& d, const QCString& sc, bool ov )
	: lineNo(l), type(t), name(n), set(s), get(g), reset(r), setfunc(0), getfunc(0),
	  sspec(Unspecified), gspec(Unspecified), stored( st ),
	  designable( d ), scriptable( sc ), override( ov ), oredEnum( -1 )
    {
	/*
	  The Q_PROPERTY construct cannot contain any commas, since
	  commas separate macro arguments. We therefore expect users
	  to type "QMap" instead of "QMap<QString, QVariant>". For
	  coherence, we also expect the same for
	  QValueList<QVariant>, the other template class supported by
	  QVariant.
	*/
	if ( type == "QMap" ) {
	    type = "QMap<QString,QVariant>";
	} else if ( type == "QValueList" ) {
	    type = "QValueList<QVariant>";
	} else if ( type == "LongLong" ) {
	    type = "Q_LLONG";
	} else if ( type == "ULongLong" ) {
	    type = "Q_ULLONG";
	}
    }

    int lineNo;
    QCString type;
    QCString name;
    QCString set;
    QCString get;
    QCString reset;
    QCString stored;
    QCString designable;
    QCString scriptable;
    bool override;

    Function* setfunc;
    Function* getfunc;

    int oredEnum; // If the enums item may be ored. That means the data type is int.
		  // Allowed values are 1 (True), 0 (False), and -1 (Unset)
    QCString enumsettype; // contains the set function type in case of oredEnum
    QCString enumgettype; // contains the get function type in case of oredEnum

    enum Specification  { Unspecified, Class, Reference, Pointer, ConstCharStar };
    Specification sspec;
    Specification gspec;

    bool stdSet() {
	QCString s = "set";
	s += toupper( name[0] );
	s += name.mid( 1 );
	return s == set;
    }

    static const char* specToString( Specification s )
    {
	switch ( s ) {
	case Class:
	    return "Class";
	case Reference:
	    return "Reference";
	case Pointer:
	    return "Pointer";
	case ConstCharStar:
	    return "ConstCharStar";
	default:
	    return "Unspecified";
	}
    }
};

class PropList : public QPtrList<Property> {	// list of properties
public:
    PropList() { setAutoDelete( TRUE ); }
};


struct ClassInfo
{
    ClassInfo( const char* n, const char* v )
	: name(n), value(v)
    {}
    QCString name;
    QCString value;
};

class ClassInfoList : public QPtrList<ClassInfo> {	// list of class infos
public:
    ClassInfoList() { setAutoDelete( TRUE ); }
};

class parser_reg {
 public:
    parser_reg();
    ~parser_reg();

    // some temporary values
    QCString   tmpExpression;			// Used to store the characters the lexer
						// is currently skipping (see addExpressionChar and friends)
    QCString  fileName;				// file name
    QCString  outputFile;				// output file name
    QCString  pchFile;				// name of PCH file (used on Windows)
    QStrList  includeFiles;			// name of #include files
    QCString  includePath;				// #include file path
    QCString  qtPath;				// #include qt file path
    int           gen_count; //number of classes generated
    bool	  noInclude;		// no #include <filename>
    bool	  generatedCode;		// no code generated
    bool	  mocError;			// moc parsing error occurred
    bool       hasVariantIncluded;	//whether or not qvariant.h was included yet
    QCString  className;				// name of parsed class
    QCString  superClassName;			// name of first super class
    QStrList  multipleSuperClasses;			// other superclasses
    FuncList  signals;				// signal interface
    FuncList  slots;				// slots interface
    FuncList  propfuncs;				// all possible property access functions
    FuncList  funcs;			// all parsed functions, including signals
    EnumList  enums;				// enums used in properties
    PropList  props;				// list of all properties
    ClassInfoList	infos;				// list of all class infos

// Used to store the values in the Q_PROPERTY macro
    QCString propWrite;				// set function
    QCString propRead;				// get function
    QCString propReset;				// reset function
    QCString propStored;				//
    QCString propDesignable;				// "true", "false" or function or empty if not specified
    QCString propScriptable;				// "true", "false" or function or empty if not specified
    bool propOverride;				// Wether OVERRIDE was detected

    QStrList qtEnums;				// Used to store the contents of Q_ENUMS
    QStrList qtSets;				// Used to store the contents of Q_SETS

};

static parser_reg *g = 0;

ArgList *addArg( Argument * );			// add arg to tmpArgList

enum Member { SignalMember,
	      SlotMember,
	      PropertyCandidateMember
	    };

void	 addMember( Member );			// add tmpFunc to current class
void     addEnum();				// add tmpEnum to current class

char	*stradd( const char *, const char * );	// add two strings
char	*stradd( const char *, const char *,	// add three strings
			       const char * );
char 	*stradd( const char *, const char *,	// adds 4 strings
		 const char *, const char * );

char	*straddSpc( const char *, const char * );
char	*straddSpc( const char *, const char *,
			       const char * );
char	*straddSpc( const char *, const char *,
		    const char *, const char * );

extern int yydebug;
bool	   lexDebug	   = FALSE;
int	   lineNo;			// current line number
bool	   errorControl	   = FALSE;	// controled errors
bool	   displayWarnings = TRUE;
bool	   skipClass;			// don't generate for class
bool	   skipFunc;			// don't generate for func
bool	   templateClass;		// class is a template
bool	   templateClassOld;		// previous class is a template

ArgList	  *tmpArgList;			// current argument list
Function  *tmpFunc;			// current member function
Enum      *tmpEnum;			// current enum
Access tmpAccess;			// current access permission
Access subClassPerm;			// current access permission

bool	   Q_OBJECTdetected;		// TRUE if current class
					//  contains the Q_OBJECT macro
bool	   Q_PROPERTYdetected;		// TRUE if current class
					//  contains at least one Q_PROPERTY,
					//  Q_OVERRIDE, Q_SETS or Q_ENUMS macro
bool	   tmpPropOverride;		// current property override setting

int	   tmpYYStart;			// Used to store the lexers current mode
int	   tmpYYStart2;			// Used to store the lexers current mode
					//  (if tmpYYStart is already used)

// if the format revision changes, you MUST change it in qmetaobject.h too
const int formatRevision = 26;		// moc output format revision

// if the flags change, you HAVE to change it in qmetaobject.h too
enum Flags  {
    Invalid		= 0x00000000,
    Readable		= 0x00000001,
    Writable		= 0x00000002,
    EnumOrSet		= 0x00000004,
    UnresolvedEnum	= 0x00000008,
    StdSet		= 0x00000100,
    Override		= 0x00000200,
    NotDesignable	= 0x00001000,
    DesignableOverride  = 0x00002000,
    NotScriptable	= 0x00004000,
    ScriptableOverride  = 0x00008000,
    NotStored 		= 0x00010000,
    StoredOverride 	= 0x00020000
};


#ifdef YYBISON
# if defined(Q_OS_WIN32)
# include <io.h>
# undef isatty
extern "C" int hack_isatty( int )
 {
     return 0;
 }
# define isatty hack_isatty
# else
# include <unistd.h>
# endif

# define YYDEBUG 1
# include "moc_yacc.h"
# include "moc_lex.cpp"
#endif //YYBISON

#line 703 "moc_yacc.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "moc_yacc.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_CHAR_VAL = 3,                   /* CHAR_VAL  */
  YYSYMBOL_INT_VAL = 4,                    /* INT_VAL  */
  YYSYMBOL_DOUBLE_VAL = 5,                 /* DOUBLE_VAL  */
  YYSYMBOL_STRING = 6,                     /* STRING  */
  YYSYMBOL_IDENTIFIER = 7,                 /* IDENTIFIER  */
  YYSYMBOL_FRIEND = 8,                     /* FRIEND  */
  YYSYMBOL_TYPEDEF = 9,                    /* TYPEDEF  */
  YYSYMBOL_AUTO = 10,                      /* AUTO  */
  YYSYMBOL_REGISTER = 11,                  /* REGISTER  */
  YYSYMBOL_STATIC = 12,                    /* STATIC  */
  YYSYMBOL_EXTERN = 13,                    /* EXTERN  */
  YYSYMBOL_INLINE = 14,                    /* INLINE  */
  YYSYMBOL_VIRTUAL = 15,                   /* VIRTUAL  */
  YYSYMBOL_CONST = 16,                     /* CONST  */
  YYSYMBOL_VOLATILE = 17,                  /* VOLATILE  */
  YYSYMBOL_CHAR = 18,                      /* CHAR  */
  YYSYMBOL_SHORT = 19,                     /* SHORT  */
  YYSYMBOL_INT = 20,                       /* INT  */
  YYSYMBOL_LONG = 21,                      /* LONG  */
  YYSYMBOL_SIGNED = 22,                    /* SIGNED  */
  YYSYMBOL_UNSIGNED = 23,                  /* UNSIGNED  */
  YYSYMBOL_FLOAT = 24,                     /* FLOAT  */
  YYSYMBOL_DOUBLE = 25,                    /* DOUBLE  */
  YYSYMBOL_VOID = 26,                      /* VOID  */
  YYSYMBOL_ENUM = 27,                      /* ENUM  */
  YYSYMBOL_CLASS = 28,                     /* CLASS  */
  YYSYMBOL_STRUCT = 29,                    /* STRUCT  */
  YYSYMBOL_UNION = 30,                     /* UNION  */
  YYSYMBOL_ASM = 31,                       /* ASM  */
  YYSYMBOL_PRIVATE = 32,                   /* PRIVATE  */
  YYSYMBOL_PROTECTED = 33,                 /* PROTECTED  */
  YYSYMBOL_PUBLIC = 34,                    /* PUBLIC  */
  YYSYMBOL_OPERATOR = 35,                  /* OPERATOR  */
  YYSYMBOL_DBL_COLON = 36,                 /* DBL_COLON  */
  YYSYMBOL_TRIPLE_DOT = 37,                /* TRIPLE_DOT  */
  YYSYMBOL_TEMPLATE = 38,                  /* TEMPLATE  */
  YYSYMBOL_NAMESPACE = 39,                 /* NAMESPACE  */
  YYSYMBOL_USING = 40,                     /* USING  */
  YYSYMBOL_MUTABLE = 41,                   /* MUTABLE  */
  YYSYMBOL_THROW = 42,                     /* THROW  */
  YYSYMBOL_SIGNALS = 43,                   /* SIGNALS  */
  YYSYMBOL_SLOTS = 44,                     /* SLOTS  */
  YYSYMBOL_Q_OBJECT = 45,                  /* Q_OBJECT  */
  YYSYMBOL_Q_PROPERTY = 46,                /* Q_PROPERTY  */
  YYSYMBOL_Q_OVERRIDE = 47,                /* Q_OVERRIDE  */
  YYSYMBOL_Q_CLASSINFO = 48,               /* Q_CLASSINFO  */
  YYSYMBOL_Q_ENUMS = 49,                   /* Q_ENUMS  */
  YYSYMBOL_Q_SETS = 50,                    /* Q_SETS  */
  YYSYMBOL_READ = 51,                      /* READ  */
  YYSYMBOL_WRITE = 52,                     /* WRITE  */
  YYSYMBOL_STORED = 53,                    /* STORED  */
  YYSYMBOL_DESIGNABLE = 54,                /* DESIGNABLE  */
  YYSYMBOL_SCRIPTABLE = 55,                /* SCRIPTABLE  */
  YYSYMBOL_RESET = 56,                     /* RESET  */
  YYSYMBOL_57_ = 57,                       /* '{'  */
  YYSYMBOL_58_ = 58,                       /* '}'  */
  YYSYMBOL_59_ = 59,                       /* '='  */
  YYSYMBOL_60_ = 60,                       /* ';'  */
  YYSYMBOL_61_ = 61,                       /* '<'  */
  YYSYMBOL_62_ = 62,                       /* '>'  */
  YYSYMBOL_63_ = 63,                       /* ','  */
  YYSYMBOL_64_ = 64,                       /* '['  */
  YYSYMBOL_65_ = 65,                       /* ']'  */
  YYSYMBOL_66_ = 66,                       /* '('  */
  YYSYMBOL_67_ = 67,                       /* ')'  */
  YYSYMBOL_68_ = 68,                       /* '*'  */
  YYSYMBOL_69_ = 69,                       /* '&'  */
  YYSYMBOL_70_ = 70,                       /* ':'  */
  YYSYMBOL_71_ = 71,                       /* '+'  */
  YYSYMBOL_72_ = 72,                       /* '-'  */
  YYSYMBOL_73_ = 73,                       /* '/'  */
  YYSYMBOL_74_ = 74,                       /* '%'  */
  YYSYMBOL_75_ = 75,                       /* '^'  */
  YYSYMBOL_76_ = 76,                       /* '|'  */
  YYSYMBOL_77_ = 77,                       /* '~'  */
  YYSYMBOL_78_ = 78,                       /* '!'  */
  YYSYMBOL_YYACCEPT = 79,                  /* $accept  */
  YYSYMBOL_declaration_seq = 80,           /* declaration_seq  */
  YYSYMBOL_declaration = 81,               /* declaration  */
  YYSYMBOL_namespace_def = 82,             /* namespace_def  */
  YYSYMBOL_named_namespace_def = 83,       /* named_namespace_def  */
  YYSYMBOL_84_1 = 84,                      /* $@1  */
  YYSYMBOL_85_2 = 85,                      /* $@2  */
  YYSYMBOL_unnamed_namespace_def = 86,     /* unnamed_namespace_def  */
  YYSYMBOL_87_3 = 87,                      /* $@3  */
  YYSYMBOL_88_4 = 88,                      /* $@4  */
  YYSYMBOL_namespace_body = 89,            /* namespace_body  */
  YYSYMBOL_namespace_alias_def = 90,       /* namespace_alias_def  */
  YYSYMBOL_using_directive = 91,           /* using_directive  */
  YYSYMBOL_using_declaration = 92,         /* using_declaration  */
  YYSYMBOL_class_def = 93,                 /* class_def  */
  YYSYMBOL_94_5 = 94,                      /* $@5  */
  YYSYMBOL_class_name = 95,                /* class_name  */
  YYSYMBOL_template_class_name = 96,       /* template_class_name  */
  YYSYMBOL_template_args = 97,             /* template_args  */
  YYSYMBOL_const_expression = 98,          /* const_expression  */
  YYSYMBOL_def_argument = 99,              /* def_argument  */
  YYSYMBOL_enumerator_expression = 100,    /* enumerator_expression  */
  YYSYMBOL_decl_specifier = 101,           /* decl_specifier  */
  YYSYMBOL_decl_specifiers = 102,          /* decl_specifiers  */
  YYSYMBOL_decl_specs_opt = 103,           /* decl_specs_opt  */
  YYSYMBOL_decl_specs = 104,               /* decl_specs  */
  YYSYMBOL_storage_class_specifier = 105,  /* storage_class_specifier  */
  YYSYMBOL_fct_specifier = 106,            /* fct_specifier  */
  YYSYMBOL_type_specifier = 107,           /* type_specifier  */
  YYSYMBOL_type_name = 108,                /* type_name  */
  YYSYMBOL_simple_type_names = 109,        /* simple_type_names  */
  YYSYMBOL_simple_type_name = 110,         /* simple_type_name  */
  YYSYMBOL_template_spec = 111,            /* template_spec  */
  YYSYMBOL_opt_template_spec = 112,        /* opt_template_spec  */
  YYSYMBOL_class_key = 113,                /* class_key  */
  YYSYMBOL_complete_class_name = 114,      /* complete_class_name  */
  YYSYMBOL_qualified_class_name = 115,     /* qualified_class_name  */
  YYSYMBOL_elaborated_type_specifier = 116, /* elaborated_type_specifier  */
  YYSYMBOL_argument_declaration_list = 117, /* argument_declaration_list  */
  YYSYMBOL_arg_declaration_list_opt = 118, /* arg_declaration_list_opt  */
  YYSYMBOL_opt_exception_argument = 119,   /* opt_exception_argument  */
  YYSYMBOL_triple_dot_opt = 120,           /* triple_dot_opt  */
  YYSYMBOL_arg_declaration_list = 121,     /* arg_declaration_list  */
  YYSYMBOL_argument_declaration = 122,     /* argument_declaration  */
  YYSYMBOL_123_6 = 123,                    /* $@6  */
  YYSYMBOL_124_7 = 124,                    /* $@7  */
  YYSYMBOL_abstract_decl_opt = 125,        /* abstract_decl_opt  */
  YYSYMBOL_abstract_decl = 126,            /* abstract_decl  */
  YYSYMBOL_127_8 = 127,                    /* $@8  */
  YYSYMBOL_128_9 = 128,                    /* $@9  */
  YYSYMBOL_declarator = 129,               /* declarator  */
  YYSYMBOL_130_10 = 130,                   /* $@10  */
  YYSYMBOL_dname = 131,                    /* dname  */
  YYSYMBOL_fct_decl = 132,                 /* fct_decl  */
  YYSYMBOL_fct_name = 133,                 /* fct_name  */
  YYSYMBOL_134_11 = 134,                   /* $@11  */
  YYSYMBOL_135_12 = 135,                   /* $@12  */
  YYSYMBOL_array_decls = 136,              /* array_decls  */
  YYSYMBOL_137_13 = 137,                   /* $@13  */
  YYSYMBOL_138_14 = 138,                   /* $@14  */
  YYSYMBOL_ptr_operators_opt = 139,        /* ptr_operators_opt  */
  YYSYMBOL_ptr_operators = 140,            /* ptr_operators  */
  YYSYMBOL_ptr_operator = 141,             /* ptr_operator  */
  YYSYMBOL_cv_qualifier_list_opt = 142,    /* cv_qualifier_list_opt  */
  YYSYMBOL_cv_qualifier_list = 143,        /* cv_qualifier_list  */
  YYSYMBOL_cv_qualifier = 144,             /* cv_qualifier  */
  YYSYMBOL_fct_body_or_semicolon = 145,    /* fct_body_or_semicolon  */
  YYSYMBOL_fct_body = 146,                 /* fct_body  */
  YYSYMBOL_147_15 = 147,                   /* $@15  */
  YYSYMBOL_class_specifier = 148,          /* class_specifier  */
  YYSYMBOL_149_16 = 149,                   /* $@16  */
  YYSYMBOL_whatever = 150,                 /* whatever  */
  YYSYMBOL_class_head = 151,               /* class_head  */
  YYSYMBOL_full_class_head = 152,          /* full_class_head  */
  YYSYMBOL_nested_class_head = 153,        /* nested_class_head  */
  YYSYMBOL_exception_spec_opt = 154,       /* exception_spec_opt  */
  YYSYMBOL_exception_spec = 155,           /* exception_spec  */
  YYSYMBOL_ctor_initializer_opt = 156,     /* ctor_initializer_opt  */
  YYSYMBOL_ctor_initializer = 157,         /* ctor_initializer  */
  YYSYMBOL_mem_initializer_list = 158,     /* mem_initializer_list  */
  YYSYMBOL_mem_initializer = 159,          /* mem_initializer  */
  YYSYMBOL_160_17 = 160,                   /* $@17  */
  YYSYMBOL_opt_base_spec = 161,            /* opt_base_spec  */
  YYSYMBOL_opt_obj_member_list = 162,      /* opt_obj_member_list  */
  YYSYMBOL_obj_member_list = 163,          /* obj_member_list  */
  YYSYMBOL_qt_access_specifier = 164,      /* qt_access_specifier  */
  YYSYMBOL_obj_member_area = 165,          /* obj_member_area  */
  YYSYMBOL_166_18 = 166,                   /* $@18  */
  YYSYMBOL_167_19 = 167,                   /* $@19  */
  YYSYMBOL_168_20 = 168,                   /* $@20  */
  YYSYMBOL_169_21 = 169,                   /* $@21  */
  YYSYMBOL_170_22 = 170,                   /* $@22  */
  YYSYMBOL_171_23 = 171,                   /* $@23  */
  YYSYMBOL_172_24 = 172,                   /* $@24  */
  YYSYMBOL_173_25 = 173,                   /* $@25  */
  YYSYMBOL_174_26 = 174,                   /* $@26  */
  YYSYMBOL_175_27 = 175,                   /* $@27  */
  YYSYMBOL_176_28 = 176,                   /* $@28  */
  YYSYMBOL_177_29 = 177,                   /* $@29  */
  YYSYMBOL_slot_area = 178,                /* slot_area  */
  YYSYMBOL_179_30 = 179,                   /* $@30  */
  YYSYMBOL_opt_property_candidates = 180,  /* opt_property_candidates  */
  YYSYMBOL_property_candidate_declarations = 181, /* property_candidate_declarations  */
  YYSYMBOL_property_candidate_declaration = 182, /* property_candidate_declaration  */
  YYSYMBOL_opt_signal_declarations = 183,  /* opt_signal_declarations  */
  YYSYMBOL_signal_declarations = 184,      /* signal_declarations  */
  YYSYMBOL_signal_declaration = 185,       /* signal_declaration  */
  YYSYMBOL_opt_slot_declarations = 186,    /* opt_slot_declarations  */
  YYSYMBOL_slot_declarations = 187,        /* slot_declarations  */
  YYSYMBOL_slot_declaration = 188,         /* slot_declaration  */
  YYSYMBOL_opt_semicolons = 189,           /* opt_semicolons  */
  YYSYMBOL_base_spec = 190,                /* base_spec  */
  YYSYMBOL_base_list = 191,                /* base_list  */
  YYSYMBOL_qt_macro_name = 192,            /* qt_macro_name  */
  YYSYMBOL_base_specifier = 193,           /* base_specifier  */
  YYSYMBOL_access_specifier = 194,         /* access_specifier  */
  YYSYMBOL_operator_name = 195,            /* operator_name  */
  YYSYMBOL_opt_virtual = 196,              /* opt_virtual  */
  YYSYMBOL_type_and_name = 197,            /* type_and_name  */
  YYSYMBOL_signal_or_slot = 198,           /* signal_or_slot  */
  YYSYMBOL_199_31 = 199,                   /* $@31  */
  YYSYMBOL_200_32 = 200,                   /* $@32  */
  YYSYMBOL_member_declarator_list = 201,   /* member_declarator_list  */
  YYSYMBOL_member_declarator = 202,        /* member_declarator  */
  YYSYMBOL_203_33 = 203,                   /* $@33  */
  YYSYMBOL_204_34 = 204,                   /* $@34  */
  YYSYMBOL_opt_bitfield = 205,             /* opt_bitfield  */
  YYSYMBOL_206_35 = 206,                   /* $@35  */
  YYSYMBOL_enum_specifier = 207,           /* enum_specifier  */
  YYSYMBOL_opt_komma = 208,                /* opt_komma  */
  YYSYMBOL_enum_tail = 209,                /* enum_tail  */
  YYSYMBOL_opt_identifier = 210,           /* opt_identifier  */
  YYSYMBOL_enum_list = 211,                /* enum_list  */
  YYSYMBOL_enumerator = 212,               /* enumerator  */
  YYSYMBOL_213_36 = 213,                   /* $@36  */
  YYSYMBOL_property = 214,                 /* property  */
  YYSYMBOL_215_37 = 215,                   /* $@37  */
  YYSYMBOL_prop_statements = 216,          /* prop_statements  */
  YYSYMBOL_qt_enums = 217,                 /* qt_enums  */
  YYSYMBOL_qt_sets = 218                   /* qt_sets  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   612

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  79
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  140
/* YYNRULES -- Number of rules.  */
#define YYNRULES  329
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  492

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   311


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    78,     2,     2,     2,    74,    69,     2,
      66,    67,    68,    71,    63,    72,     2,    73,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    70,    60,
      61,    59,    62,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    64,     2,    65,    75,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    57,    76,    58,    77,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   807,   807,   808,   811,   813,   814,   815,   816,   819,
     820,   824,   825,   823,   832,   833,   832,   840,   843,   848,
     851,   852,   855,   855,   864,   865,   868,   880,   893,   902,
     905,   911,   912,   913,   914,   915,   918,   921,   922,   925,
     926,   929,   930,   931,   932,   935,   936,   939,   940,   943,
     944,   945,   948,   950,   953,   954,   955,   956,   957,   958,
     959,   960,   961,   964,   970,   971,   977,   978,   981,   982,
     986,   988,   992,   993,   994,   999,  1000,  1006,  1007,  1010,
    1011,  1014,  1015,  1021,  1024,  1027,  1030,  1029,  1033,  1038,
    1036,  1044,  1045,  1048,  1050,  1050,  1055,  1055,  1060,  1061,
    1064,  1065,  1067,  1067,  1072,  1075,  1078,  1090,  1091,  1093,
    1093,  1096,  1096,  1102,  1102,  1104,  1104,  1109,  1110,  1113,
    1114,  1117,  1118,  1125,  1126,  1129,  1130,  1134,  1135,  1138,
    1139,  1140,  1143,  1143,  1151,  1150,  1156,  1158,  1160,  1162,
    1166,  1170,  1171,  1172,  1173,  1174,  1178,  1183,  1191,  1195,
    1200,  1201,  1205,  1208,  1209,  1212,  1215,  1216,  1220,  1220,
    1225,  1226,  1229,  1230,  1233,  1234,  1238,  1239,  1243,  1243,
    1245,  1245,  1247,  1255,  1258,  1255,  1262,  1265,  1262,  1269,
    1271,  1269,  1276,  1277,  1276,  1282,  1283,  1282,  1290,  1292,
    1293,  1293,  1303,  1310,  1311,  1314,  1315,  1318,  1321,  1322,
    1325,  1326,  1330,  1333,  1334,  1337,  1338,  1341,  1344,  1345,
    1348,  1351,  1352,  1355,  1357,  1361,  1362,  1363,  1364,  1365,
    1366,  1367,  1368,  1369,  1370,  1373,  1374,  1375,  1378,  1379,
    1380,  1381,  1382,  1383,  1384,  1385,  1386,  1387,  1388,  1389,
    1390,  1391,  1392,  1393,  1394,  1395,  1396,  1397,  1398,  1399,
    1400,  1401,  1402,  1403,  1404,  1405,  1406,  1407,  1408,  1409,
    1410,  1411,  1412,  1413,  1414,  1415,  1416,  1417,  1418,  1422,
    1423,  1426,  1429,  1436,  1443,  1451,  1453,  1457,  1462,  1464,
    1466,  1469,  1471,  1477,  1478,  1480,  1483,  1485,  1487,  1489,
    1492,  1495,  1498,  1494,  1503,  1504,  1507,  1508,  1508,  1510,
    1510,  1514,  1515,  1515,  1520,  1525,  1526,  1529,  1536,  1540,
    1541,  1544,  1545,  1546,  1549,  1550,  1550,  1555,  1554,  1591,
    1592,  1593,  1594,  1595,  1596,  1597,  1600,  1601,  1604,  1605
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "CHAR_VAL", "INT_VAL",
  "DOUBLE_VAL", "STRING", "IDENTIFIER", "FRIEND", "TYPEDEF", "AUTO",
  "REGISTER", "STATIC", "EXTERN", "INLINE", "VIRTUAL", "CONST", "VOLATILE",
  "CHAR", "SHORT", "INT", "LONG", "SIGNED", "UNSIGNED", "FLOAT", "DOUBLE",
  "VOID", "ENUM", "CLASS", "STRUCT", "UNION", "ASM", "PRIVATE",
  "PROTECTED", "PUBLIC", "OPERATOR", "DBL_COLON", "TRIPLE_DOT", "TEMPLATE",
  "NAMESPACE", "USING", "MUTABLE", "THROW", "SIGNALS", "SLOTS", "Q_OBJECT",
  "Q_PROPERTY", "Q_OVERRIDE", "Q_CLASSINFO", "Q_ENUMS", "Q_SETS", "READ",
  "WRITE", "STORED", "DESIGNABLE", "SCRIPTABLE", "RESET", "'{'", "'}'",
  "'='", "';'", "'<'", "'>'", "','", "'['", "']'", "'('", "')'", "'*'",
  "'&'", "':'", "'+'", "'-'", "'/'", "'%'", "'^'", "'|'", "'~'", "'!'",
  "$accept", "declaration_seq", "declaration", "namespace_def",
  "named_namespace_def", "$@1", "$@2", "unnamed_namespace_def", "$@3",
  "$@4", "namespace_body", "namespace_alias_def", "using_directive",
  "using_declaration", "class_def", "$@5", "class_name",
  "template_class_name", "template_args", "const_expression",
  "def_argument", "enumerator_expression", "decl_specifier",
  "decl_specifiers", "decl_specs_opt", "decl_specs",
  "storage_class_specifier", "fct_specifier", "type_specifier",
  "type_name", "simple_type_names", "simple_type_name", "template_spec",
  "opt_template_spec", "class_key", "complete_class_name",
  "qualified_class_name", "elaborated_type_specifier",
  "argument_declaration_list", "arg_declaration_list_opt",
  "opt_exception_argument", "triple_dot_opt", "arg_declaration_list",
  "argument_declaration", "$@6", "$@7", "abstract_decl_opt",
  "abstract_decl", "$@8", "$@9", "declarator", "$@10", "dname", "fct_decl",
  "fct_name", "$@11", "$@12", "array_decls", "$@13", "$@14",
  "ptr_operators_opt", "ptr_operators", "ptr_operator",
  "cv_qualifier_list_opt", "cv_qualifier_list", "cv_qualifier",
  "fct_body_or_semicolon", "fct_body", "$@15", "class_specifier", "$@16",
  "whatever", "class_head", "full_class_head", "nested_class_head",
  "exception_spec_opt", "exception_spec", "ctor_initializer_opt",
  "ctor_initializer", "mem_initializer_list", "mem_initializer", "$@17",
  "opt_base_spec", "opt_obj_member_list", "obj_member_list",
  "qt_access_specifier", "obj_member_area", "$@18", "$@19", "$@20", "$@21",
  "$@22", "$@23", "$@24", "$@25", "$@26", "$@27", "$@28", "$@29",
  "slot_area", "$@30", "opt_property_candidates",
  "property_candidate_declarations", "property_candidate_declaration",
  "opt_signal_declarations", "signal_declarations", "signal_declaration",
  "opt_slot_declarations", "slot_declarations", "slot_declaration",
  "opt_semicolons", "base_spec", "base_list", "qt_macro_name",
  "base_specifier", "access_specifier", "operator_name", "opt_virtual",
  "type_and_name", "signal_or_slot", "$@31", "$@32",
  "member_declarator_list", "member_declarator", "$@33", "$@34",
  "opt_bitfield", "$@35", "enum_specifier", "opt_komma", "enum_tail",
  "opt_identifier", "enum_list", "enumerator", "$@36", "property", "$@37",
  "prop_statements", "qt_enums", "qt_sets", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-356)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-276)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -356,    61,  -356,    35,    83,  -356,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,    12,    37,    -9,  -356,  -356,  -356,    21,
     469,   112,   100,    53,   118,    63,    36,    67,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,     9,  -356,  -356,   113,  -356,
     153,   155,   207,    99,  -356,  -356,  -356,   120,   231,   215,
     113,  -356,  -356,   218,  -356,  -356,   231,   209,  -356,  -356,
     -22,   176,  -356,  -356,  -356,  -356,   244,  -356,  -356,    20,
     493,   113,  -356,  -356,    -6,   247,  -356,   253,  -356,  -356,
     489,  -356,  -356,   101,    99,   101,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,   283,   493,  -356,  -356,
    -356,   284,  -356,  -356,   276,   285,  -356,  -356,  -356,  -356,
    -356,   281,   288,   289,   290,   292,   293,  -356,  -356,    23,
    -356,  -356,  -356,   243,   364,   364,   366,   369,   372,  -356,
     315,   316,  -356,  -356,   314,  -356,  -356,   306,    16,   380,
     156,   381,    87,  -356,   385,  -356,  -356,  -356,   277,   526,
    -356,  -356,   382,  -356,  -356,  -356,   138,  -356,   243,  -356,
     337,    73,  -356,   384,   409,   351,   352,   359,   369,   357,
     372,   358,  -356,   243,   243,  -356,  -356,    46,   370,   419,
    -356,  -356,   379,    97,   164,  -356,   374,   375,   388,    43,
     -13,   -21,   416,   418,   438,   -31,   439,   440,   498,   451,
    -356,   386,    36,   442,   434,  -356,   287,   437,    91,   156,
     174,   174,   313,  -356,    29,  -356,  -356,    68,   -17,  -356,
    -356,  -356,   494,   323,  -356,  -356,   214,  -356,   443,  -356,
    -356,  -356,   500,  -356,  -356,  -356,  -356,  -356,   243,  -356,
    -356,  -356,   243,  -356,  -356,  -356,  -356,  -356,  -356,   419,
     445,   465,  -356,  -356,  -356,   470,  -356,   471,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,   432,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,   137,   137,  -356,   472,  -356,
    -356,   137,  -356,  -356,  -356,  -356,  -356,   174,  -356,  -356,
      19,   137,   156,  -356,  -356,  -356,   473,   474,  -356,   135,
     410,   466,   516,   491,  -356,  -356,   474,  -356,    22,  -356,
     192,   243,   243,   468,   243,   243,  -356,  -356,  -356,   490,
    -356,  -356,   465,  -356,   419,   499,  -356,  -356,  -356,  -356,
    -356,  -356,   474,    25,  -356,   156,  -356,  -356,  -356,  -356,
    -356,   135,    10,   249,  -356,   451,   174,  -356,  -356,   441,
    -356,   474,   486,     8,  -356,   280,  -356,   256,  -356,   474,
     551,   552,   553,   554,   555,   556,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,   501,   506,  -356,  -356,  -356,   474,
     156,  -356,  -356,   505,  -356,   172,  -356,  -356,   135,  -356,
    -356,  -356,   497,  -356,  -356,  -356,   242,  -356,  -356,  -356,
    -356,    22,   192,   192,   192,   192,   192,   192,   243,  -356,
    -356,  -356,  -356,  -356,   503,  -356,  -356,   510,  -356,    36,
     528,  -356,  -356,  -356,  -356,  -356,   474,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,   474,  -356,  -356,  -356,   507,
     508,  -356,   512,   511,   384,  -356,  -356,   513,  -356,  -356,
    -356,    36,   353,   117,  -356,  -356,  -356,  -356,   504,  -356,
    -356,   569,  -356,  -356,  -356,   509,  -356,   521,   520,  -356,
    -356,  -356
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,    22,     1,    14,     0,     3,     5,     9,    10,     6,
       8,     7,     4,    64,    11,     0,    20,    21,    19,     0,
      65,     0,     0,     0,   136,     0,     0,     0,    15,    27,
     141,    41,    42,    43,    44,    45,    46,    47,    48,    54,
      55,    56,    57,    58,    59,    60,    61,    62,   144,   145,
     143,   142,   140,    66,    67,    24,    71,    25,   146,    23,
       0,     0,     0,     0,   148,   161,   134,    24,     0,     0,
      68,    12,     2,     0,    27,   147,     0,     0,   137,   138,
      24,     0,   225,   226,   227,   215,   210,   220,   212,     0,
     162,    69,    18,     2,    22,     0,    63,     0,    70,   139,
       0,   217,   222,     0,     0,     0,   219,   224,   170,   167,
     172,   173,   176,   179,   182,   185,     0,   163,   168,   165,
     166,     0,    16,    26,     0,     0,   216,   221,   211,   218,
     223,     0,     0,     0,     0,     0,     0,   135,   164,     0,
      13,   213,   214,   198,     0,     0,     0,   326,   328,   192,
       0,     0,   190,   169,    24,    34,    35,    46,     0,     0,
      37,     0,     0,    39,    64,    31,    33,    32,     0,    51,
      53,    65,     0,    50,    49,   272,     0,   171,   199,   201,
       0,   301,   202,   309,     0,     0,     0,     0,   326,     0,
     328,     0,   188,   203,   193,   109,   113,   108,    73,   311,
     304,    74,   240,   241,   242,   264,     0,     0,   232,   236,
     230,   231,   233,   234,   235,   237,   238,   239,     0,    38,
     279,     0,     0,     0,     0,    40,    37,     0,   107,    37,
     123,   123,   117,   271,     0,   119,    52,    72,   160,   291,
     208,   200,     0,    37,   302,   208,     0,   310,     0,   317,
     174,   177,     0,   327,   183,   329,   186,   189,   204,   206,
     207,   191,   194,   196,   197,    28,    28,   111,   115,   311,
     314,   305,   312,   253,   254,   256,   255,   257,   268,   267,
     245,   249,   260,   243,   262,   244,   266,   263,   246,   247,
     248,   250,   261,   251,   252,   117,   117,   289,     0,   208,
      73,   117,    72,   278,   127,   128,   121,   124,   125,   122,
       0,   118,    37,   276,   120,   149,     0,   290,   273,    91,
      64,     0,    81,    78,    84,    28,   283,   208,     0,   208,
     319,   193,   193,     0,   193,   193,   205,   195,   110,     0,
      28,    28,   305,   315,   306,     0,   258,   259,   265,   228,
     229,   208,   287,     0,   126,    37,   277,   281,   292,   209,
      94,     0,    85,    92,    98,    37,   123,    82,    75,    37,
     303,   284,   105,     0,   299,   296,   100,     0,   294,   286,
       0,     0,     0,     0,     0,     0,   318,   175,   178,   180,
     184,   187,   114,   112,     0,     0,    30,   313,   308,   288,
      37,   274,   282,     0,    28,     0,   105,    86,    91,    96,
      93,    36,   153,    76,    83,   297,     0,    28,   102,   101,
     208,     0,   319,   319,   319,   319,   319,   319,   193,   116,
     307,   316,   280,   208,     0,    99,    29,    88,    28,     0,
     150,   154,    28,   104,   300,    28,   285,   295,   320,   321,
     323,   324,   325,   322,   181,   293,    95,    87,    89,     0,
       0,   155,   156,     0,   309,   151,   298,     0,    29,    97,
     158,     0,    37,     0,   103,    90,    28,   157,     0,    80,
     132,     0,   129,   106,   130,     0,   152,     0,     0,   159,
     133,   131
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -356,   581,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,
     492,  -356,  -356,  -356,  -356,  -356,    54,  -356,   514,  -260,
     114,  -356,  -133,  -356,  -223,  -158,   563,   564,   566,  -157,
    -356,   -16,   574,  -356,   -12,   -26,   -11,  -356,  -356,  -356,
    -356,  -356,  -356,  -355,  -356,  -356,   181,   229,  -356,  -356,
     219,  -356,   232,  -356,  -159,  -356,  -356,  -356,  -356,  -356,
    -180,   423,  -216,  -218,  -356,   286,  -356,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,   124,
    -356,  -356,   360,  -356,  -356,  -356,   479,  -356,  -356,  -356,
    -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,  -356,
    -356,  -310,  -356,   335,  -356,  -356,   421,  -356,  -356,   342,
    -240,  -356,  -356,   132,   502,   -41,  -221,  -356,  -356,  -131,
    -356,  -356,  -356,   180,  -356,  -356,  -356,  -356,  -356,   260,
    -356,   139,   336,   263,  -356,   459,  -356,    47,   420,   422
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    94,     5,     6,     7,    27,    93,     8,    15,    72,
      95,     9,    10,    11,    12,    13,    56,    57,    73,   338,
     457,   431,   163,   319,   218,   164,   165,   166,   167,   168,
     169,   170,   171,    21,   172,   173,    70,   174,   321,   322,
     478,   368,   323,   324,   436,   468,   362,   363,   404,   438,
     375,   445,   376,   245,   175,   265,   340,   197,   266,   341,
     310,   311,   235,   306,   307,   308,   483,   484,   487,    23,
      90,    52,    24,    25,   176,   464,   465,   440,   441,   461,
     462,   476,    64,   116,   117,   118,   119,   139,   131,   132,
     331,   133,   332,   134,   428,   135,   334,   136,   335,   153,
     194,   261,   262,   263,   177,   178,   179,   257,   258,   259,
     317,    65,    86,    87,    88,    89,   220,   180,   181,   264,
     316,   403,   377,   378,   442,   417,   246,   325,   183,   345,
     200,   248,   271,   272,   396,   185,   330,   386,   189,   191
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      69,    22,   219,   301,    51,   326,   339,   226,   303,   233,
     232,    58,   182,   309,   414,   406,    67,   406,   314,    76,
     320,   387,   388,   198,   390,   391,   228,    80,   291,   372,
     149,   225,   228,     3,     4,   105,   228,    85,   285,    74,
     103,   286,    14,    67,   100,   292,   283,   182,    28,   120,
      19,   287,   -17,    63,   355,   101,    68,    91,   284,   352,
     400,     2,   260,   106,   312,   370,   150,   151,   219,   407,
      74,   219,    68,   199,   373,   313,   120,   126,    85,   129,
     393,   394,    29,   318,   125,   219,   225,   371,   373,   379,
      16,   357,   374,   152,    67,   314,    26,   230,   231,   225,
       3,     4,   281,   364,   -24,   267,    80,    55,    80,    75,
     268,   399,   282,    59,    81,   349,   350,   479,   454,    17,
      66,   353,    18,    68,    71,   -24,   222,   260,   -24,    74,
      98,    82,    83,    84,   402,    68,   223,    68,   -24,   243,
      53,    54,   411,   244,   434,   364,   320,   410,   412,    76,
     195,   356,   227,   236,   219,   196,   274,   444,   275,   419,
      77,   238,    78,   365,   155,   156,    31,    32,    33,    34,
      35,    36,    37,    38,   480,  -160,   481,   482,   459,   432,
     446,    74,   466,    80,    60,   467,    61,    62,    63,   410,
     304,   305,   364,   455,   401,   239,   298,   219,   240,   360,
     419,   361,   296,   230,   231,   230,   231,   219,    82,    83,
      84,   219,    68,   102,    79,   202,   485,   203,   204,   205,
     206,   107,   207,   276,   208,   209,   277,   210,   211,   212,
     213,   214,   215,   216,   217,   127,   409,   130,    67,   435,
     230,   231,   219,   380,   381,   382,   383,   384,   385,   320,
     154,   155,   156,    31,    32,    33,    34,    35,   157,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
     158,   -64,   -64,   159,   327,    92,    99,   328,   160,    68,
      96,    19,   161,   162,   228,   155,   156,    31,    32,    33,
      34,    35,    36,    37,    38,   155,   156,    31,    32,    33,
      34,    35,    36,    37,    38,   122,   418,   104,   227,   443,
     230,   231,   229,   409,   219,   123,   420,   230,   231,   421,
    -269,   155,   156,    31,    32,    33,    34,    35,    36,    37,
      38,   155,   156,    31,    32,    33,    34,    35,    36,    37,
      38,   137,   140,   141,   418,   230,   231,  -275,   230,   231,
    -275,   143,   142,  -275,   144,   145,   146,  -275,   147,   148,
     -77,   155,   156,    31,    32,    33,    34,    35,    36,    37,
      38,   184,   187,   195,  -107,    74,   188,  -107,   196,   190,
    -107,   230,   231,  -270,  -107,   192,   193,   201,   221,   237,
     -77,   247,    67,   155,   156,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,   224,   460,   242,   159,   249,    67,   250,   251,
     -79,    68,   252,    19,   254,   256,   270,   269,    39,    40,
      41,    42,    43,    44,    45,    46,    47,   224,   273,   278,
     159,   300,   279,   297,   302,   460,    68,   280,    19,   155,
     156,    31,    32,    33,    34,    35,    36,    37,    38,   155,
     156,    31,    32,    33,    34,    35,    36,    37,    38,   448,
     449,   450,   451,   452,   453,   288,    30,   289,   413,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,   124,   290,   293,   294,
     348,   228,   299,   329,   343,   295,   333,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    82,    83,    84,   344,   346,
     347,   358,   351,   366,   359,   389,   108,   109,   110,   111,
     112,   113,   114,   115,    39,    40,    41,    42,    43,    44,
      45,    46,    47,   367,   369,   392,   415,   398,   422,   423,
     424,   425,   426,   427,   430,   433,   429,   439,   456,   458,
     463,   486,   469,   488,   470,   471,   489,   472,   474,   490,
     491,     1,   475,    48,    49,   121,    50,    20,    97,   437,
     405,   234,   416,   354,   408,   477,   138,   337,   315,   241,
     336,   447,   395,   473,   186,   342,   128,   397,   253,     0,
       0,     0,   255
};

static const yytype_int16 yycheck[] =
{
      26,    13,   160,   226,    20,   245,   266,   164,   229,   168,
     168,    22,   143,   231,   369,     7,     7,     7,   234,    36,
     243,   331,   332,     7,   334,   335,     7,     7,    59,     7,
       7,   164,     7,    39,    40,    15,     7,    63,    59,    61,
      81,    62,     7,     7,    66,    76,    59,   178,    57,    90,
      38,    72,    58,    70,    35,    81,    36,    68,    71,   299,
      35,     0,   193,    89,    35,   325,    43,    44,   226,    59,
      61,   229,    36,    57,    66,   234,   117,   103,   104,   105,
     340,   341,    61,   242,   100,   243,   219,   327,    66,   329,
       7,   312,    70,    70,     7,   311,    59,    68,    69,   232,
      39,    40,    59,   319,    36,    59,     7,     7,     7,    55,
      64,   351,    69,    60,    15,   295,   296,   472,   428,    36,
      57,   301,    39,    36,    57,    57,    39,   258,    60,    61,
      76,    32,    33,    34,   355,    36,   162,    36,    70,    66,
      28,    29,   365,    70,   404,   361,   369,   363,   366,    36,
      59,   310,   164,   169,   312,    64,    59,   417,    61,   375,
       7,   172,     7,   320,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    57,    57,    59,    60,   438,   400,
     420,    61,   442,     7,    66,   445,    68,    69,    70,   405,
      16,    17,   408,   433,   353,    57,   222,   355,    60,    64,
     416,    66,   218,    68,    69,    68,    69,   365,    32,    33,
      34,   369,    36,    81,     7,    59,   476,    61,    62,    63,
      64,    89,    66,    59,    68,    69,    62,    71,    72,    73,
      74,    75,    76,    77,    78,   103,    64,   105,     7,    67,
      68,    69,   400,    51,    52,    53,    54,    55,    56,   472,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    60,    60,    67,    63,    35,    36,
      62,    38,    39,    40,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    58,    64,    63,   320,    67,
      68,    69,    35,    64,   472,    62,    60,    68,    69,    63,
      77,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    58,    58,    67,    64,    68,    69,    60,    68,    69,
      63,    70,    67,    66,    66,    66,    66,    70,    66,    66,
      37,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,     7,     6,    59,    60,    61,     7,    63,    64,     7,
      66,    68,    69,    77,    70,    70,    70,     7,     7,     7,
      67,     7,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,   439,    77,    30,     7,     7,    67,    67,
      67,    36,    63,    38,    67,    67,     7,    57,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    59,    65,
      30,     7,    67,    57,     7,   471,    36,    59,    38,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,   422,
     423,   424,   425,   426,   427,    59,     7,    59,    37,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     7,    59,    59,    59,
      68,     7,    60,    60,    59,     7,     6,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    32,    33,    34,    63,    59,
      59,    58,    60,    67,    60,    67,    43,    44,    45,    46,
      47,    48,    49,    50,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    37,    63,    65,    70,    58,     7,     7,
       7,     7,     7,     7,    58,    60,    65,    70,    65,    59,
      42,    67,    65,     4,    66,    63,    67,    66,    65,    58,
      60,     0,   468,    20,    20,    93,    20,    13,    74,   408,
     361,   168,   373,   307,   362,   471,   117,   262,   238,   178,
     258,   421,   342,   464,   145,   269,   104,   344,   188,    -1,
      -1,    -1,   190
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    80,     0,    39,    40,    81,    82,    83,    86,    90,
      91,    92,    93,    94,     7,    87,     7,    36,    39,    38,
     111,   112,   113,   148,   151,   152,    59,    84,    57,    61,
       7,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,   105,   106,
     107,   110,   150,    28,    29,     7,    95,    96,   115,    60,
      66,    68,    69,    70,   161,   190,    57,     7,    36,   114,
     115,    57,    88,    97,    61,    95,    36,     7,     7,     7,
       7,    15,    32,    33,    34,   114,   191,   192,   193,   194,
     149,   115,    60,    85,    80,    89,    62,    97,    95,    67,
      66,   114,   192,   194,    63,    15,   114,   192,    43,    44,
      45,    46,    47,    48,    49,    50,   162,   163,   164,   165,
     194,    89,    58,    62,     7,   110,   114,   192,   193,   114,
     192,   167,   168,   170,   172,   174,   176,    58,   165,   166,
      58,    67,    67,    70,    66,    66,    66,    66,    66,     7,
      43,    44,    70,   178,     7,     8,     9,    15,    27,    30,
      35,    39,    40,   101,   104,   105,   106,   107,   108,   109,
     110,   111,   113,   114,   116,   133,   153,   183,   184,   185,
     196,   197,   198,   207,     7,   214,   214,     6,     7,   217,
       7,   218,    70,    70,   179,    59,    64,   136,     7,    57,
     209,     7,    59,    61,    62,    63,    64,    66,    68,    69,
      71,    72,    73,    74,    75,    76,    77,    78,   103,   104,
     195,     7,    39,   114,    27,   101,   108,   113,     7,    35,
      68,    69,   104,   133,   140,   141,   110,     7,   115,    57,
      60,   185,    77,    66,    70,   132,   205,     7,   210,     7,
      67,    67,    63,   217,    67,   218,    67,   186,   187,   188,
     198,   180,   181,   182,   198,   134,   137,    59,    64,    57,
       7,   211,   212,    59,    59,    61,    59,    62,    65,    67,
      59,    59,    69,    59,    71,    59,    62,    72,    59,    59,
      59,    59,    76,    59,    59,     7,   110,    57,   114,    60,
       7,   103,     7,   195,    16,    17,   142,   143,   144,   142,
     139,   140,    35,   133,   141,   161,   199,   189,   133,   102,
     103,   117,   118,   121,   122,   206,   189,    60,    63,    60,
     215,   169,   171,     6,   175,   177,   188,   182,    98,    98,
     135,   138,   211,    59,    63,   208,    59,    59,    68,   139,
     139,    60,   189,   139,   144,    35,   133,   195,    58,    60,
      64,    66,   125,   126,   141,   108,    67,    37,   120,    63,
      98,   189,     7,    66,    70,   129,   131,   201,   202,   189,
      51,    52,    53,    54,    55,    56,   216,   180,   180,    67,
     180,   180,    65,    98,    98,   208,   213,   212,    58,   189,
      35,   133,   195,   200,   127,   126,     7,    59,   131,    64,
     141,   103,   142,    37,   122,    70,   129,   204,    64,   141,
      60,    63,     7,     7,     7,     7,     7,     7,   173,    65,
      58,   100,   195,    60,    98,    67,   123,   125,   128,    70,
     156,   157,   203,    67,    98,   130,   189,   202,   216,   216,
     216,   216,   216,   216,   180,   189,    65,    99,    59,    98,
     114,   158,   159,    42,   154,   155,    98,    98,   124,    65,
      66,    63,    66,   210,    65,    99,   160,   158,   119,   122,
      57,    59,    60,   145,   146,    98,    67,   147,     4,    67,
      58,    60
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    79,    80,    80,    81,    81,    81,    81,    81,    82,
      82,    84,    85,    83,    87,    88,    86,    89,    90,    91,
      92,    92,    94,    93,    95,    95,    96,    97,    98,    99,
     100,   101,   101,   101,   101,   101,   102,   103,   103,   104,
     104,   105,   105,   105,   105,   106,   106,   107,   107,   108,
     108,   108,   109,   109,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   111,   112,   112,   113,   113,   114,   114,
     115,   115,   116,   116,   116,   117,   117,   118,   118,   119,
     119,   120,   120,   121,   121,   122,   123,   122,   122,   124,
     122,   125,   125,   126,   127,   126,   128,   126,   126,   126,
     129,   129,   130,   129,   129,   131,   132,   133,   133,   134,
     133,   135,   133,   137,   136,   138,   136,   139,   139,   140,
     140,   141,   141,   142,   142,   143,   143,   144,   144,   145,
     145,   145,   147,   146,   149,   148,   148,   148,   148,   148,
     148,   150,   150,   150,   150,   150,   151,   151,   152,   153,
     154,   154,   155,   156,   156,   157,   158,   158,   160,   159,
     161,   161,   162,   162,   163,   163,   164,   164,   166,   165,
     167,   165,   165,   168,   169,   165,   170,   171,   165,   172,
     173,   165,   174,   175,   165,   176,   177,   165,   178,   178,
     179,   178,   178,   180,   180,   181,   181,   182,   183,   183,
     184,   184,   185,   186,   186,   187,   187,   188,   189,   189,
     190,   191,   191,   192,   192,   193,   193,   193,   193,   193,
     193,   193,   193,   193,   193,   194,   194,   194,   195,   195,
     195,   195,   195,   195,   195,   195,   195,   195,   195,   195,
     195,   195,   195,   195,   195,   195,   195,   195,   195,   195,
     195,   195,   195,   195,   195,   195,   195,   195,   195,   195,
     195,   195,   195,   195,   195,   195,   195,   195,   195,   196,
     196,   197,   197,   197,   197,   197,   197,   197,   197,   197,
     197,   197,   197,   198,   198,   198,   198,   198,   198,   198,
     198,   199,   200,   198,   201,   201,   202,   203,   202,   204,
     202,   205,   206,   205,   207,   208,   208,   209,   209,   210,
     210,   211,   211,   211,   212,   213,   212,   215,   214,   216,
     216,   216,   216,   216,   216,   216,   217,   217,   218,   218
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     0,     0,     7,     0,     0,     6,     1,     5,     2,
       2,     2,     0,     3,     1,     1,     4,     0,     0,     0,
       0,     1,     1,     1,     1,     1,     3,     0,     1,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     4,     0,     1,     2,     2,     1,     2,
       3,     1,     2,     2,     2,     2,     3,     0,     1,     0,
       1,     0,     1,     3,     1,     2,     0,     5,     4,     0,
       7,     0,     1,     2,     0,     4,     0,     5,     1,     3,
       1,     2,     0,     5,     3,     1,     8,     1,     2,     0,
       4,     0,     5,     0,     4,     0,     5,     0,     1,     1,
       2,     2,     2,     0,     1,     1,     2,     1,     1,     1,
       1,     3,     0,     3,     0,     5,     1,     3,     3,     4,
       2,     1,     1,     1,     1,     1,     2,     3,     2,     3,
       0,     1,     4,     0,     1,     2,     1,     3,     0,     5,
       0,     1,     0,     1,     2,     1,     1,     1,     0,     3,
       0,     4,     1,     0,     0,     7,     0,     0,     7,     0,
       0,     9,     0,     0,     7,     0,     0,     7,     2,     3,
       0,     3,     1,     0,     1,     2,     1,     1,     0,     1,
       2,     1,     1,     0,     1,     2,     1,     1,     0,     2,
       2,     3,     1,     4,     4,     1,     3,     2,     3,     2,
       1,     3,     2,     3,     2,     1,     1,     1,     3,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     3,     3,
       2,     2,     2,     2,     1,     3,     2,     2,     2,     0,
       1,     2,     1,     3,     5,     2,     3,     4,     3,     2,
       6,     4,     5,     3,     4,     6,     4,     4,     5,     3,
       3,     0,     0,     7,     1,     3,     1,     0,     4,     0,
       3,     0,     0,     3,     2,     0,     1,     5,     4,     0,
       1,     0,     1,     3,     1,     0,     4,     0,     4,     0,
       3,     3,     3,     3,     3,     3,     0,     2,     0,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 11: /* $@1: %empty  */
#line 824 "moc.y"
                                             { enterNameSpace((yyvsp[0].string)); }
#line 2336 "moc_yacc.cpp"
    break;

  case 12: /* $@2: %empty  */
#line 825 "moc.y"
                                             { BEGIN IN_NAMESPACE; }
#line 2342 "moc_yacc.cpp"
    break;

  case 13: /* named_namespace_def: NAMESPACE IDENTIFIER $@1 '{' $@2 namespace_body '}'  */
#line 827 "moc.y"
                                             { leaveNameSpace();
					       selectOutsideClassState();
					     }
#line 2350 "moc_yacc.cpp"
    break;

  case 14: /* $@3: %empty  */
#line 832 "moc.y"
                                             { enterNameSpace(); }
#line 2356 "moc_yacc.cpp"
    break;

  case 15: /* $@4: %empty  */
#line 833 "moc.y"
                                             { BEGIN IN_NAMESPACE; }
#line 2362 "moc_yacc.cpp"
    break;

  case 16: /* unnamed_namespace_def: NAMESPACE $@3 '{' $@4 namespace_body '}'  */
#line 835 "moc.y"
                                             { leaveNameSpace();
					       selectOutsideClassState();
					     }
#line 2370 "moc_yacc.cpp"
    break;

  case 18: /* namespace_alias_def: NAMESPACE IDENTIFIER '=' complete_class_name ';'  */
#line 844 "moc.y"
                                            { selectOutsideClassState(); }
#line 2376 "moc_yacc.cpp"
    break;

  case 19: /* using_directive: USING NAMESPACE  */
#line 848 "moc.y"
                                            { selectOutsideClassState(); }
#line 2382 "moc_yacc.cpp"
    break;

  case 20: /* using_declaration: USING IDENTIFIER  */
#line 851 "moc.y"
                                            { selectOutsideClassState(); }
#line 2388 "moc_yacc.cpp"
    break;

  case 21: /* using_declaration: USING DBL_COLON  */
#line 852 "moc.y"
                                            { selectOutsideClassState(); }
#line 2394 "moc_yacc.cpp"
    break;

  case 22: /* $@5: %empty  */
#line 855 "moc.y"
                                              { initClass(); }
#line 2400 "moc_yacc.cpp"
    break;

  case 23: /* class_def: $@5 class_specifier ';'  */
#line 856 "moc.y"
                                              { generateClass();
						registerClassInNamespace();
						selectOutsideClassState(); }
#line 2408 "moc_yacc.cpp"
    break;

  case 24: /* class_name: IDENTIFIER  */
#line 864 "moc.y"
                                              { (yyval.string) = (yyvsp[0].string); }
#line 2414 "moc_yacc.cpp"
    break;

  case 25: /* class_name: template_class_name  */
#line 865 "moc.y"
                                              { (yyval.string) = (yyvsp[0].string); }
#line 2420 "moc_yacc.cpp"
    break;

  case 26: /* template_class_name: IDENTIFIER '<' template_args '>'  */
#line 869 "moc.y"
                                   { g->tmpExpression = rmWS( g->tmpExpression );
				     (yyval.string) = stradd( (yyvsp[-3].string), "<",
						  g->tmpExpression, ">" ); }
#line 2428 "moc_yacc.cpp"
    break;

  case 27: /* template_args: %empty  */
#line 880 "moc.y"
                                                  { initExpression();
						    templLevel = 1;
						    BEGIN IN_TEMPL_ARGS; }
#line 2436 "moc_yacc.cpp"
    break;

  case 28: /* const_expression: %empty  */
#line 893 "moc.y"
                                                  { initExpression();
						    BEGIN IN_EXPR; }
#line 2443 "moc_yacc.cpp"
    break;

  case 29: /* def_argument: %empty  */
#line 902 "moc.y"
                                          { BEGIN IN_DEF_ARG; }
#line 2449 "moc_yacc.cpp"
    break;

  case 30: /* enumerator_expression: %empty  */
#line 905 "moc.y"
                                                  { initExpression();
						    BEGIN IN_ENUM; }
#line 2456 "moc_yacc.cpp"
    break;

  case 31: /* decl_specifier: storage_class_specifier  */
#line 911 "moc.y"
                                                  { (yyval.string) = ""; }
#line 2462 "moc_yacc.cpp"
    break;

  case 32: /* decl_specifier: type_specifier  */
#line 912 "moc.y"
                                                  { (yyval.string) = (yyvsp[0].string); }
#line 2468 "moc_yacc.cpp"
    break;

  case 33: /* decl_specifier: fct_specifier  */
#line 913 "moc.y"
                                                  { (yyval.string) = ""; }
#line 2474 "moc_yacc.cpp"
    break;

  case 34: /* decl_specifier: FRIEND  */
#line 914 "moc.y"
                                                  { skipFunc = TRUE; (yyval.string) = ""; }
#line 2480 "moc_yacc.cpp"
    break;

  case 35: /* decl_specifier: TYPEDEF  */
#line 915 "moc.y"
                                                  { skipFunc = TRUE; (yyval.string) = ""; }
#line 2486 "moc_yacc.cpp"
    break;

  case 36: /* decl_specifiers: decl_specs_opt type_name decl_specs_opt  */
#line 919 "moc.y"
                                                  { (yyval.string) = straddSpc((yyvsp[-2].string),(yyvsp[-1].string),(yyvsp[0].string)); }
#line 2492 "moc_yacc.cpp"
    break;

  case 37: /* decl_specs_opt: %empty  */
#line 921 "moc.y"
                                                  { (yyval.string) = ""; }
#line 2498 "moc_yacc.cpp"
    break;

  case 38: /* decl_specs_opt: decl_specs  */
#line 922 "moc.y"
                                                  { (yyval.string) = (yyvsp[0].string); }
#line 2504 "moc_yacc.cpp"
    break;

  case 39: /* decl_specs: decl_specifier  */
#line 925 "moc.y"
                                                    { (yyval.string) = (yyvsp[0].string); }
#line 2510 "moc_yacc.cpp"
    break;

  case 40: /* decl_specs: decl_specs decl_specifier  */
#line 926 "moc.y"
                                                    { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string)); }
#line 2516 "moc_yacc.cpp"
    break;

  case 43: /* storage_class_specifier: STATIC  */
#line 931 "moc.y"
                                                { skipFunc = TRUE; }
#line 2522 "moc_yacc.cpp"
    break;

  case 45: /* fct_specifier: INLINE  */
#line 935 "moc.y"
                                                { }
#line 2528 "moc_yacc.cpp"
    break;

  case 46: /* fct_specifier: VIRTUAL  */
#line 936 "moc.y"
                                                { }
#line 2534 "moc_yacc.cpp"
    break;

  case 47: /* type_specifier: CONST  */
#line 939 "moc.y"
                                                { (yyval.string) = "const"; }
#line 2540 "moc_yacc.cpp"
    break;

  case 48: /* type_specifier: VOLATILE  */
#line 940 "moc.y"
                                                { (yyval.string) = "volatile"; }
#line 2546 "moc_yacc.cpp"
    break;

  case 49: /* type_name: elaborated_type_specifier  */
#line 943 "moc.y"
                                                    { (yyval.string) = (yyvsp[0].string); }
#line 2552 "moc_yacc.cpp"
    break;

  case 50: /* type_name: complete_class_name  */
#line 944 "moc.y"
                                                    { (yyval.string) = (yyvsp[0].string); }
#line 2558 "moc_yacc.cpp"
    break;

  case 51: /* type_name: simple_type_names  */
#line 945 "moc.y"
                                                    { (yyval.string) = (yyvsp[0].string); }
#line 2564 "moc_yacc.cpp"
    break;

  case 52: /* simple_type_names: simple_type_names simple_type_name  */
#line 949 "moc.y"
                                                    { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string)); }
#line 2570 "moc_yacc.cpp"
    break;

  case 53: /* simple_type_names: simple_type_name  */
#line 950 "moc.y"
                                                    { (yyval.string) = (yyvsp[0].string); }
#line 2576 "moc_yacc.cpp"
    break;

  case 54: /* simple_type_name: CHAR  */
#line 953 "moc.y"
                                                    { (yyval.string) = "char"; }
#line 2582 "moc_yacc.cpp"
    break;

  case 55: /* simple_type_name: SHORT  */
#line 954 "moc.y"
                                                    { (yyval.string) = "short"; }
#line 2588 "moc_yacc.cpp"
    break;

  case 56: /* simple_type_name: INT  */
#line 955 "moc.y"
                                                    { (yyval.string) = "int"; }
#line 2594 "moc_yacc.cpp"
    break;

  case 57: /* simple_type_name: LONG  */
#line 956 "moc.y"
                                                    { (yyval.string) = "long"; }
#line 2600 "moc_yacc.cpp"
    break;

  case 58: /* simple_type_name: SIGNED  */
#line 957 "moc.y"
                                                    { (yyval.string) = "signed"; }
#line 2606 "moc_yacc.cpp"
    break;

  case 59: /* simple_type_name: UNSIGNED  */
#line 958 "moc.y"
                                                    { (yyval.string) = "unsigned"; }
#line 2612 "moc_yacc.cpp"
    break;

  case 60: /* simple_type_name: FLOAT  */
#line 959 "moc.y"
                                                    { (yyval.string) = "float"; }
#line 2618 "moc_yacc.cpp"
    break;

  case 61: /* simple_type_name: DOUBLE  */
#line 960 "moc.y"
                                                    { (yyval.string) = "double"; }
#line 2624 "moc_yacc.cpp"
    break;

  case 62: /* simple_type_name: VOID  */
#line 961 "moc.y"
                                                    { (yyval.string) = "void"; }
#line 2630 "moc_yacc.cpp"
    break;

  case 63: /* template_spec: TEMPLATE '<' template_args '>'  */
#line 965 "moc.y"
                                   { g->tmpExpression = rmWS( g->tmpExpression );
				     (yyval.string) = stradd( "template<",
						  g->tmpExpression, ">" ); }
#line 2638 "moc_yacc.cpp"
    break;

  case 65: /* opt_template_spec: template_spec  */
#line 971 "moc.y"
                                                { templateClassOld = templateClass;
						  templateClass = TRUE;
						}
#line 2646 "moc_yacc.cpp"
    break;

  case 66: /* class_key: opt_template_spec CLASS  */
#line 977 "moc.y"
                                                        { (yyval.string) = "class"; }
#line 2652 "moc_yacc.cpp"
    break;

  case 67: /* class_key: opt_template_spec STRUCT  */
#line 978 "moc.y"
                                                        { (yyval.string) = "struct"; }
#line 2658 "moc_yacc.cpp"
    break;

  case 68: /* complete_class_name: qualified_class_name  */
#line 981 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2664 "moc_yacc.cpp"
    break;

  case 69: /* complete_class_name: DBL_COLON qualified_class_name  */
#line 983 "moc.y"
                                                { (yyval.string) = stradd( "::", (yyvsp[0].string) ); }
#line 2670 "moc_yacc.cpp"
    break;

  case 70: /* qualified_class_name: qualified_class_name DBL_COLON class_name  */
#line 987 "moc.y"
                                                { (yyval.string) = stradd( (yyvsp[-2].string), "::", (yyvsp[0].string) );}
#line 2676 "moc_yacc.cpp"
    break;

  case 71: /* qualified_class_name: class_name  */
#line 988 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2682 "moc_yacc.cpp"
    break;

  case 72: /* elaborated_type_specifier: class_key IDENTIFIER  */
#line 992 "moc.y"
                                                { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string)); }
#line 2688 "moc_yacc.cpp"
    break;

  case 73: /* elaborated_type_specifier: ENUM IDENTIFIER  */
#line 993 "moc.y"
                                                { (yyval.string) = stradd("enum ",(yyvsp[0].string)); }
#line 2694 "moc_yacc.cpp"
    break;

  case 74: /* elaborated_type_specifier: UNION IDENTIFIER  */
#line 994 "moc.y"
                                                { (yyval.string) = stradd("union ",(yyvsp[0].string)); }
#line 2700 "moc_yacc.cpp"
    break;

  case 75: /* argument_declaration_list: arg_declaration_list_opt triple_dot_opt  */
#line 999 "moc.y"
                                                                    { (yyval.arg_list) = (yyvsp[-1].arg_list);}
#line 2706 "moc_yacc.cpp"
    break;

  case 76: /* argument_declaration_list: arg_declaration_list ',' TRIPLE_DOT  */
#line 1000 "moc.y"
                                                                    { (yyval.arg_list) = (yyvsp[-2].arg_list);
				       func_warn("Ellipsis not supported"
						 " in signals and slots.\n"
						 "Ellipsis argument ignored."); }
#line 2715 "moc_yacc.cpp"
    break;

  case 77: /* arg_declaration_list_opt: %empty  */
#line 1006 "moc.y"
                                                { (yyval.arg_list) = tmpArgList; }
#line 2721 "moc_yacc.cpp"
    break;

  case 78: /* arg_declaration_list_opt: arg_declaration_list  */
#line 1007 "moc.y"
                                                { (yyval.arg_list) = (yyvsp[0].arg_list); }
#line 2727 "moc_yacc.cpp"
    break;

  case 79: /* opt_exception_argument: %empty  */
#line 1010 "moc.y"
                                                { (yyval.arg) = 0; }
#line 2733 "moc_yacc.cpp"
    break;

  case 82: /* triple_dot_opt: TRIPLE_DOT  */
#line 1015 "moc.y"
                                     { func_warn("Ellipsis not supported"
						 " in signals and slots.\n"
						 "Ellipsis argument ignored."); }
#line 2741 "moc_yacc.cpp"
    break;

  case 83: /* arg_declaration_list: arg_declaration_list ',' argument_declaration  */
#line 1023 "moc.y"
                                                { (yyval.arg_list) = addArg((yyvsp[0].arg)); }
#line 2747 "moc_yacc.cpp"
    break;

  case 84: /* arg_declaration_list: argument_declaration  */
#line 1024 "moc.y"
                                                { (yyval.arg_list) = addArg((yyvsp[0].arg)); }
#line 2753 "moc_yacc.cpp"
    break;

  case 85: /* argument_declaration: decl_specifiers abstract_decl_opt  */
#line 1028 "moc.y"
                                { (yyval.arg) = new Argument(straddSpc((yyvsp[-1].string),(yyvsp[0].string)),""); }
#line 2759 "moc_yacc.cpp"
    break;

  case 86: /* $@6: %empty  */
#line 1030 "moc.y"
                              { expLevel = 1; }
#line 2765 "moc_yacc.cpp"
    break;

  case 87: /* argument_declaration: decl_specifiers abstract_decl_opt '=' $@6 def_argument  */
#line 1032 "moc.y"
                                { (yyval.arg) = new Argument(straddSpc((yyvsp[-4].string),(yyvsp[-3].string)),"", 0, TRUE ); }
#line 2771 "moc_yacc.cpp"
    break;

  case 88: /* argument_declaration: decl_specifiers abstract_decl_opt dname abstract_decl_opt  */
#line 1035 "moc.y"
                                { (yyval.arg) = new Argument(straddSpc((yyvsp[-3].string),(yyvsp[-2].string)),(yyvsp[0].string), (yyvsp[-1].string)); }
#line 2777 "moc_yacc.cpp"
    break;

  case 89: /* $@7: %empty  */
#line 1038 "moc.y"
                                { expLevel = 1; }
#line 2783 "moc_yacc.cpp"
    break;

  case 90: /* argument_declaration: decl_specifiers abstract_decl_opt dname abstract_decl_opt '=' $@7 def_argument  */
#line 1040 "moc.y"
                                { (yyval.arg) = new Argument(straddSpc((yyvsp[-6].string),(yyvsp[-5].string)),(yyvsp[-3].string), (yyvsp[-4].string), TRUE); }
#line 2789 "moc_yacc.cpp"
    break;

  case 91: /* abstract_decl_opt: %empty  */
#line 1044 "moc.y"
                                                { (yyval.string) = ""; }
#line 2795 "moc_yacc.cpp"
    break;

  case 92: /* abstract_decl_opt: abstract_decl  */
#line 1045 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2801 "moc_yacc.cpp"
    break;

  case 93: /* abstract_decl: abstract_decl ptr_operator  */
#line 1049 "moc.y"
                                                { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string)); }
#line 2807 "moc_yacc.cpp"
    break;

  case 94: /* $@8: %empty  */
#line 1050 "moc.y"
                                                { expLevel = 1; }
#line 2813 "moc_yacc.cpp"
    break;

  case 95: /* abstract_decl: '[' $@8 const_expression ']'  */
#line 1052 "moc.y"
                                   { (yyval.string) = stradd( "[",
				     g->tmpExpression =
				     g->tmpExpression.stripWhiteSpace(), "]" ); }
#line 2821 "moc_yacc.cpp"
    break;

  case 96: /* $@9: %empty  */
#line 1055 "moc.y"
                                                { expLevel = 1; }
#line 2827 "moc_yacc.cpp"
    break;

  case 97: /* abstract_decl: abstract_decl '[' $@9 const_expression ']'  */
#line 1057 "moc.y"
                                   { (yyval.string) = stradd( (yyvsp[-4].string),"[",
				     g->tmpExpression =
				     g->tmpExpression.stripWhiteSpace(),"]" ); }
#line 2835 "moc_yacc.cpp"
    break;

  case 98: /* abstract_decl: ptr_operator  */
#line 1060 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2841 "moc_yacc.cpp"
    break;

  case 99: /* abstract_decl: '(' abstract_decl ')'  */
#line 1061 "moc.y"
                                                { (yyval.string) = (yyvsp[-1].string); }
#line 2847 "moc_yacc.cpp"
    break;

  case 100: /* declarator: dname  */
#line 1064 "moc.y"
                                                { (yyval.string) = ""; }
#line 2853 "moc_yacc.cpp"
    break;

  case 101: /* declarator: declarator ptr_operator  */
#line 1066 "moc.y"
                                                { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string));}
#line 2859 "moc_yacc.cpp"
    break;

  case 102: /* $@10: %empty  */
#line 1067 "moc.y"
                                                { expLevel = 1; }
#line 2865 "moc_yacc.cpp"
    break;

  case 103: /* declarator: declarator '[' $@10 const_expression ']'  */
#line 1069 "moc.y"
                                   { (yyval.string) = stradd( (yyvsp[-4].string),"[",
				     g->tmpExpression =
				     g->tmpExpression.stripWhiteSpace(),"]" ); }
#line 2873 "moc_yacc.cpp"
    break;

  case 104: /* declarator: '(' declarator ')'  */
#line 1072 "moc.y"
                                                { (yyval.string) = (yyvsp[-1].string); }
#line 2879 "moc_yacc.cpp"
    break;

  case 106: /* fct_decl: '(' argument_declaration_list ')' cv_qualifier_list_opt ctor_initializer_opt exception_spec_opt opt_identifier fct_body_or_semicolon  */
#line 1086 "moc.y"
                                                { tmpFunc->args	     = (yyvsp[-6].arg_list);
						  tmpFunc->qualifier = (yyvsp[-4].string); }
#line 2886 "moc_yacc.cpp"
    break;

  case 108: /* fct_name: IDENTIFIER array_decls  */
#line 1092 "moc.y"
                                { func_warn("Variable as signal or slot."); }
#line 2892 "moc_yacc.cpp"
    break;

  case 109: /* $@11: %empty  */
#line 1093 "moc.y"
                                         { expLevel=0; }
#line 2898 "moc_yacc.cpp"
    break;

  case 110: /* fct_name: IDENTIFIER '=' $@11 const_expression  */
#line 1095 "moc.y"
                                                { skipFunc = TRUE; }
#line 2904 "moc_yacc.cpp"
    break;

  case 111: /* $@12: %empty  */
#line 1096 "moc.y"
                                                     { expLevel=0; }
#line 2910 "moc_yacc.cpp"
    break;

  case 112: /* fct_name: IDENTIFIER array_decls '=' $@12 const_expression  */
#line 1098 "moc.y"
                                                { skipFunc = TRUE; }
#line 2916 "moc_yacc.cpp"
    break;

  case 113: /* $@13: %empty  */
#line 1102 "moc.y"
                                                { expLevel = 1; }
#line 2922 "moc_yacc.cpp"
    break;

  case 115: /* $@14: %empty  */
#line 1104 "moc.y"
                                                 { expLevel = 1; }
#line 2928 "moc_yacc.cpp"
    break;

  case 117: /* ptr_operators_opt: %empty  */
#line 1109 "moc.y"
                                                { (yyval.string) = ""; }
#line 2934 "moc_yacc.cpp"
    break;

  case 118: /* ptr_operators_opt: ptr_operators  */
#line 1110 "moc.y"
                                                 { (yyval.string) = (yyvsp[0].string); }
#line 2940 "moc_yacc.cpp"
    break;

  case 119: /* ptr_operators: ptr_operator  */
#line 1113 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2946 "moc_yacc.cpp"
    break;

  case 120: /* ptr_operators: ptr_operators ptr_operator  */
#line 1114 "moc.y"
                                                     { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string));}
#line 2952 "moc_yacc.cpp"
    break;

  case 121: /* ptr_operator: '*' cv_qualifier_list_opt  */
#line 1117 "moc.y"
                                                    { (yyval.string) = straddSpc("*",(yyvsp[0].string));}
#line 2958 "moc_yacc.cpp"
    break;

  case 122: /* ptr_operator: '&' cv_qualifier_list_opt  */
#line 1118 "moc.y"
                                                    { (yyval.string) = stradd("&",(yyvsp[0].string));}
#line 2964 "moc_yacc.cpp"
    break;

  case 123: /* cv_qualifier_list_opt: %empty  */
#line 1125 "moc.y"
                                                { (yyval.string) = ""; }
#line 2970 "moc_yacc.cpp"
    break;

  case 124: /* cv_qualifier_list_opt: cv_qualifier_list  */
#line 1126 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2976 "moc_yacc.cpp"
    break;

  case 125: /* cv_qualifier_list: cv_qualifier  */
#line 1129 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 2982 "moc_yacc.cpp"
    break;

  case 126: /* cv_qualifier_list: cv_qualifier_list cv_qualifier  */
#line 1131 "moc.y"
                                                { (yyval.string) = straddSpc((yyvsp[-1].string),(yyvsp[0].string)); }
#line 2988 "moc_yacc.cpp"
    break;

  case 127: /* cv_qualifier: CONST  */
#line 1134 "moc.y"
                                                { (yyval.string) = "const"; }
#line 2994 "moc_yacc.cpp"
    break;

  case 128: /* cv_qualifier: VOLATILE  */
#line 1135 "moc.y"
                                                { (yyval.string) = "volatile"; }
#line 3000 "moc_yacc.cpp"
    break;

  case 132: /* $@15: %empty  */
#line 1143 "moc.y"
                              { BEGIN IN_FCT; fctLevel = 1;}
#line 3006 "moc_yacc.cpp"
    break;

  case 133: /* fct_body: '{' $@15 '}'  */
#line 1144 "moc.y"
                              { BEGIN QT_DEF; }
#line 3012 "moc_yacc.cpp"
    break;

  case 134: /* $@16: %empty  */
#line 1151 "moc.y"
                                                { BEGIN IN_CLASS;
						  classPLevel = 1;
						}
#line 3020 "moc_yacc.cpp"
    break;

  case 135: /* class_specifier: full_class_head '{' $@16 opt_obj_member_list '}'  */
#line 1155 "moc.y"
                                                { BEGIN QT_DEF; }
#line 3026 "moc_yacc.cpp"
    break;

  case 136: /* class_specifier: class_head  */
#line 1156 "moc.y"
                                                { BEGIN QT_DEF;	  /* -- " -- */
						  skipClass = TRUE; }
#line 3033 "moc_yacc.cpp"
    break;

  case 137: /* class_specifier: class_head '*' IDENTIFIER  */
#line 1158 "moc.y"
                                                        { BEGIN QT_DEF;	  /* -- " -- */
 						  skipClass = TRUE; }
#line 3040 "moc_yacc.cpp"
    break;

  case 138: /* class_specifier: class_head '&' IDENTIFIER  */
#line 1160 "moc.y"
                                                        { BEGIN QT_DEF;	  /* -- " -- */
 						  skipClass = TRUE; }
#line 3047 "moc_yacc.cpp"
    break;

  case 139: /* class_specifier: class_head '(' IDENTIFIER ')'  */
#line 1164 "moc.y"
                                                { BEGIN QT_DEF; /* catch ';' */
						  skipClass = TRUE; }
#line 3054 "moc_yacc.cpp"
    break;

  case 140: /* class_specifier: template_spec whatever  */
#line 1166 "moc.y"
                                                 { skipClass = TRUE;
						  BEGIN GIMME_SEMICOLON; }
#line 3061 "moc_yacc.cpp"
    break;

  case 144: /* whatever: storage_class_specifier  */
#line 1173 "moc.y"
                                                  { (yyval.string) = ""; }
#line 3067 "moc_yacc.cpp"
    break;

  case 146: /* class_head: class_key qualified_class_name  */
#line 1179 "moc.y"
                                                { g->className = (yyvsp[0].string);
						  if ( g->className == "QObject" )
						     Q_OBJECTdetected = TRUE;
						}
#line 3076 "moc_yacc.cpp"
    break;

  case 147: /* class_head: class_key IDENTIFIER class_name  */
#line 1185 "moc.y"
                                                { g->className = (yyvsp[0].string);
						  if ( g->className == "QObject" )
						     Q_OBJECTdetected = TRUE;
						}
#line 3085 "moc_yacc.cpp"
    break;

  case 148: /* full_class_head: class_head opt_base_spec  */
#line 1192 "moc.y"
                                                { g->superClassName = (yyvsp[0].string); }
#line 3091 "moc_yacc.cpp"
    break;

  case 149: /* nested_class_head: class_key qualified_class_name opt_base_spec  */
#line 1197 "moc.y"
                                                { templateClass = templateClassOld; }
#line 3097 "moc_yacc.cpp"
    break;

  case 158: /* $@17: %empty  */
#line 1220 "moc.y"
                                                { expLevel = 1; }
#line 3103 "moc_yacc.cpp"
    break;

  case 160: /* opt_base_spec: %empty  */
#line 1225 "moc.y"
                                                { (yyval.string) = 0; }
#line 3109 "moc_yacc.cpp"
    break;

  case 161: /* opt_base_spec: base_spec  */
#line 1226 "moc.y"
                                                { (yyval.string) = (yyvsp[0].string); }
#line 3115 "moc_yacc.cpp"
    break;

  case 166: /* qt_access_specifier: access_specifier  */
#line 1238 "moc.y"
                                                { tmpAccess = (yyvsp[0].access); }
#line 3121 "moc_yacc.cpp"
    break;

  case 167: /* qt_access_specifier: SLOTS  */
#line 1239 "moc.y"
                                      { moc_err( "Missing access specifier"
						   " before \"slots:\"." ); }
#line 3128 "moc_yacc.cpp"
    break;

  case 168: /* $@18: %empty  */
#line 1243 "moc.y"
                                                { BEGIN QT_DEF; }
#line 3134 "moc_yacc.cpp"
    break;

  case 170: /* $@19: %empty  */
#line 1245 "moc.y"
                                                { BEGIN QT_DEF; }
#line 3140 "moc_yacc.cpp"
    break;

  case 172: /* obj_member_area: Q_OBJECT  */
#line 1247 "moc.y"
                                                {
			      if ( tmpAccess )
				  moc_warn("Q_OBJECT is not in the private"
					   " section of the class.\n"
					   "Q_OBJECT is a macro that resets"
					   " access permission to \"private\".");
			      Q_OBJECTdetected = TRUE;
			  }
#line 3153 "moc_yacc.cpp"
    break;

  case 173: /* $@20: %empty  */
#line 1255 "moc.y"
                                     { tmpYYStart = YY_START;
				       tmpPropOverride = FALSE;
				       BEGIN IN_PROPERTY; }
#line 3161 "moc_yacc.cpp"
    break;

  case 174: /* $@21: %empty  */
#line 1258 "moc.y"
                                           {
						BEGIN tmpYYStart;
					   }
#line 3169 "moc_yacc.cpp"
    break;

  case 176: /* $@22: %empty  */
#line 1262 "moc.y"
                                     { tmpYYStart = YY_START;
				       tmpPropOverride = TRUE;
				       BEGIN IN_PROPERTY; }
#line 3177 "moc_yacc.cpp"
    break;

  case 177: /* $@23: %empty  */
#line 1265 "moc.y"
                                           {
						BEGIN tmpYYStart;
					   }
#line 3185 "moc_yacc.cpp"
    break;

  case 179: /* $@24: %empty  */
#line 1269 "moc.y"
                                      { tmpYYStart = YY_START; BEGIN IN_CLASSINFO; }
#line 3191 "moc_yacc.cpp"
    break;

  case 180: /* $@25: %empty  */
#line 1271 "moc.y"
                                  {
				      g->infos.append( new ClassInfo( (yyvsp[-3].string), (yyvsp[-1].string) ) );
				      BEGIN tmpYYStart;
				  }
#line 3200 "moc_yacc.cpp"
    break;

  case 182: /* $@26: %empty  */
#line 1276 "moc.y"
                                  { tmpYYStart = YY_START; BEGIN IN_PROPERTY; }
#line 3206 "moc_yacc.cpp"
    break;

  case 183: /* $@27: %empty  */
#line 1277 "moc.y"
                                           {
						Q_PROPERTYdetected = TRUE;
						BEGIN tmpYYStart;
					   }
#line 3215 "moc_yacc.cpp"
    break;

  case 185: /* $@28: %empty  */
#line 1282 "moc.y"
                                 { tmpYYStart = YY_START; BEGIN IN_PROPERTY; }
#line 3221 "moc_yacc.cpp"
    break;

  case 186: /* $@29: %empty  */
#line 1283 "moc.y"
                                          {
						Q_PROPERTYdetected = TRUE;
						BEGIN tmpYYStart;
					   }
#line 3230 "moc_yacc.cpp"
    break;

  case 188: /* slot_area: SIGNALS ':'  */
#line 1290 "moc.y"
                                        { moc_err( "Signals cannot "
						 "have access specifiers" ); }
#line 3237 "moc_yacc.cpp"
    break;

  case 190: /* $@30: %empty  */
#line 1293 "moc.y"
                                        { if ( tmpAccess == Public && Q_PROPERTYdetected )
						  BEGIN QT_DEF;
					      else
						  BEGIN IN_CLASS;
					  suppress_func_warn = TRUE;
					}
#line 3248 "moc_yacc.cpp"
    break;

  case 191: /* slot_area: ':' $@30 opt_property_candidates  */
#line 1300 "moc.y"
                                        {
					  suppress_func_warn = FALSE;
					}
#line 3256 "moc_yacc.cpp"
    break;

  case 192: /* slot_area: IDENTIFIER  */
#line 1303 "moc.y"
                                        { BEGIN IN_CLASS;
					   if ( classPLevel != 1 )
					       moc_warn( "unexpected access"
							 "specifier" );
					}
#line 3266 "moc_yacc.cpp"
    break;

  case 197: /* property_candidate_declaration: signal_or_slot  */
#line 1318 "moc.y"
                                               { addMember( PropertyCandidateMember ); }
#line 3272 "moc_yacc.cpp"
    break;

  case 202: /* signal_declaration: signal_or_slot  */
#line 1330 "moc.y"
                                                { addMember( SignalMember ); }
#line 3278 "moc_yacc.cpp"
    break;

  case 207: /* slot_declaration: signal_or_slot  */
#line 1341 "moc.y"
                                                { addMember( SlotMember ); }
#line 3284 "moc_yacc.cpp"
    break;

  case 210: /* base_spec: ':' base_list  */
#line 1348 "moc.y"
                                                { (yyval.string)=(yyvsp[0].string); }
#line 3290 "moc_yacc.cpp"
    break;

  case 211: /* base_list: base_list ',' base_specifier  */
#line 1351 "moc.y"
                                                        { g->multipleSuperClasses.append( (yyvsp[0].string) ); }
#line 3296 "moc_yacc.cpp"
    break;

  case 213: /* qt_macro_name: IDENTIFIER '(' IDENTIFIER ')'  */
#line 1356 "moc.y"
                                           { (yyval.string) = stradd( (yyvsp[-3].string), "(", (yyvsp[-1].string), ")" ); }
#line 3302 "moc_yacc.cpp"
    break;

  case 214: /* qt_macro_name: IDENTIFIER '(' simple_type_name ')'  */
#line 1358 "moc.y"
                                           { (yyval.string) = stradd( (yyvsp[-3].string), "(", (yyvsp[-1].string), ")" ); }
#line 3308 "moc_yacc.cpp"
    break;

  case 215: /* base_specifier: complete_class_name  */
#line 1361 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3314 "moc_yacc.cpp"
    break;

  case 216: /* base_specifier: VIRTUAL access_specifier complete_class_name  */
#line 1362 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3320 "moc_yacc.cpp"
    break;

  case 217: /* base_specifier: VIRTUAL complete_class_name  */
#line 1363 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3326 "moc_yacc.cpp"
    break;

  case 218: /* base_specifier: access_specifier VIRTUAL complete_class_name  */
#line 1364 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3332 "moc_yacc.cpp"
    break;

  case 219: /* base_specifier: access_specifier complete_class_name  */
#line 1365 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3338 "moc_yacc.cpp"
    break;

  case 220: /* base_specifier: qt_macro_name  */
#line 1366 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3344 "moc_yacc.cpp"
    break;

  case 221: /* base_specifier: VIRTUAL access_specifier qt_macro_name  */
#line 1367 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3350 "moc_yacc.cpp"
    break;

  case 222: /* base_specifier: VIRTUAL qt_macro_name  */
#line 1368 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3356 "moc_yacc.cpp"
    break;

  case 223: /* base_specifier: access_specifier VIRTUAL qt_macro_name  */
#line 1369 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3362 "moc_yacc.cpp"
    break;

  case 224: /* base_specifier: access_specifier qt_macro_name  */
#line 1370 "moc.y"
                                                                       {(yyval.string)=(yyvsp[0].string);}
#line 3368 "moc_yacc.cpp"
    break;

  case 225: /* access_specifier: PRIVATE  */
#line 1373 "moc.y"
                                                { (yyval.access)=Private; }
#line 3374 "moc_yacc.cpp"
    break;

  case 226: /* access_specifier: PROTECTED  */
#line 1374 "moc.y"
                                                { (yyval.access)=Protected; }
#line 3380 "moc_yacc.cpp"
    break;

  case 227: /* access_specifier: PUBLIC  */
#line 1375 "moc.y"
                                                { (yyval.access)=Public; }
#line 3386 "moc_yacc.cpp"
    break;

  case 228: /* operator_name: decl_specs_opt IDENTIFIER ptr_operators_opt  */
#line 1378 "moc.y"
                                                                      { }
#line 3392 "moc_yacc.cpp"
    break;

  case 229: /* operator_name: decl_specs_opt simple_type_name ptr_operators_opt  */
#line 1379 "moc.y"
                                                                            { }
#line 3398 "moc_yacc.cpp"
    break;

  case 271: /* type_and_name: type_name fct_name  */
#line 1427 "moc.y"
                                                { tmpFunc->type = (yyvsp[-1].string);
						  tmpFunc->name = (yyvsp[0].string); }
#line 3405 "moc_yacc.cpp"
    break;

  case 272: /* type_and_name: fct_name  */
#line 1430 "moc.y"
                                                { tmpFunc->type = "int";
						  tmpFunc->name = (yyvsp[0].string);
				  if ( tmpFunc->name == g->className )
				      func_warn( "Constructors cannot be"
						 " signals or slots.");
						}
#line 3416 "moc_yacc.cpp"
    break;

  case 273: /* type_and_name: opt_virtual '~' fct_name  */
#line 1437 "moc.y"
                                                { tmpFunc->type = "void";
						  tmpFunc->name = "~";
						  tmpFunc->name += (yyvsp[0].string);
				       func_warn( "Destructors cannot be"
						  " signals or slots.");
						}
#line 3427 "moc_yacc.cpp"
    break;

  case 274: /* type_and_name: decl_specs type_name decl_specs_opt ptr_operators_opt fct_name  */
#line 1445 "moc.y"
                                                {
						    char *tmp =
							straddSpc((yyvsp[-4].string),(yyvsp[-3].string),(yyvsp[-2].string),(yyvsp[-1].string));
						    tmpFunc->type = rmWS(tmp);
						    delete [] tmp;
						    tmpFunc->name = (yyvsp[0].string); }
#line 3438 "moc_yacc.cpp"
    break;

  case 275: /* type_and_name: decl_specs type_name  */
#line 1452 "moc.y"
                                                { skipFunc = TRUE; }
#line 3444 "moc_yacc.cpp"
    break;

  case 276: /* type_and_name: type_name ptr_operators fct_name  */
#line 1454 "moc.y"
                                                { tmpFunc->type =
						      straddSpc((yyvsp[-2].string),(yyvsp[-1].string));
						  tmpFunc->name = (yyvsp[0].string); }
#line 3452 "moc_yacc.cpp"
    break;

  case 277: /* type_and_name: type_name decl_specs ptr_operators_opt fct_name  */
#line 1459 "moc.y"
                                                { tmpFunc->type =
						      straddSpc((yyvsp[-3].string),(yyvsp[-2].string),(yyvsp[-1].string));
						  tmpFunc->name = (yyvsp[0].string); }
#line 3460 "moc_yacc.cpp"
    break;

  case 278: /* type_and_name: type_name OPERATOR operator_name  */
#line 1463 "moc.y"
                                                { operatorError();    }
#line 3466 "moc_yacc.cpp"
    break;

  case 279: /* type_and_name: OPERATOR operator_name  */
#line 1465 "moc.y"
                                                { operatorError();    }
#line 3472 "moc_yacc.cpp"
    break;

  case 280: /* type_and_name: decl_specs type_name decl_specs_opt ptr_operators_opt OPERATOR operator_name  */
#line 1468 "moc.y"
                                                { operatorError();    }
#line 3478 "moc_yacc.cpp"
    break;

  case 281: /* type_and_name: type_name ptr_operators OPERATOR operator_name  */
#line 1470 "moc.y"
                                                { operatorError();    }
#line 3484 "moc_yacc.cpp"
    break;

  case 282: /* type_and_name: type_name decl_specs ptr_operators_opt OPERATOR operator_name  */
#line 1473 "moc.y"
                                                { operatorError();    }
#line 3490 "moc_yacc.cpp"
    break;

  case 284: /* signal_or_slot: type_and_name opt_bitfield ';' opt_semicolons  */
#line 1479 "moc.y"
                                { func_warn("Unexpected variable declaration."); }
#line 3496 "moc_yacc.cpp"
    break;

  case 285: /* signal_or_slot: type_and_name opt_bitfield ',' member_declarator_list ';' opt_semicolons  */
#line 1482 "moc.y"
                                { func_warn("Unexpected variable declaration."); }
#line 3502 "moc_yacc.cpp"
    break;

  case 286: /* signal_or_slot: enum_specifier opt_identifier ';' opt_semicolons  */
#line 1484 "moc.y"
                                { func_warn("Unexpected enum declaration."); }
#line 3508 "moc_yacc.cpp"
    break;

  case 287: /* signal_or_slot: USING complete_class_name ';' opt_semicolons  */
#line 1486 "moc.y"
                                { func_warn("Unexpected using declaration."); }
#line 3514 "moc_yacc.cpp"
    break;

  case 288: /* signal_or_slot: USING NAMESPACE complete_class_name ';' opt_semicolons  */
#line 1488 "moc.y"
                                { func_warn("Unexpected using declaration."); }
#line 3520 "moc_yacc.cpp"
    break;

  case 289: /* signal_or_slot: NAMESPACE IDENTIFIER '{'  */
#line 1490 "moc.y"
                                { classPLevel++;
				  moc_err("Unexpected namespace declaration."); }
#line 3527 "moc_yacc.cpp"
    break;

  case 290: /* signal_or_slot: nested_class_head ';' opt_semicolons  */
#line 1493 "moc.y"
                                { func_warn("Unexpected class declaration.");}
#line 3533 "moc_yacc.cpp"
    break;

  case 291: /* $@31: %empty  */
#line 1495 "moc.y"
                                { func_warn("Unexpected class declaration.");
				  BEGIN IN_FCT; fctLevel=1;
				}
#line 3541 "moc_yacc.cpp"
    break;

  case 292: /* $@32: %empty  */
#line 1498 "moc.y"
                               { BEGIN QT_DEF; }
#line 3547 "moc_yacc.cpp"
    break;

  case 296: /* member_declarator: declarator  */
#line 1507 "moc.y"
                                                 { }
#line 3553 "moc_yacc.cpp"
    break;

  case 297: /* $@33: %empty  */
#line 1508 "moc.y"
                                                 { expLevel = 0; }
#line 3559 "moc_yacc.cpp"
    break;

  case 299: /* $@34: %empty  */
#line 1510 "moc.y"
                                                 { expLevel = 0; }
#line 3565 "moc_yacc.cpp"
    break;

  case 302: /* $@35: %empty  */
#line 1515 "moc.y"
                                                 { expLevel = 0; }
#line 3571 "moc_yacc.cpp"
    break;

  case 307: /* enum_tail: IDENTIFIER '{' enum_list opt_komma '}'  */
#line 1530 "moc.y"
                                { BEGIN QT_DEF;
				  if ( tmpAccess == Public) {
				      tmpEnum->name = (yyvsp[-4].string);
				      addEnum();
				  }
				}
#line 3582 "moc_yacc.cpp"
    break;

  case 308: /* enum_tail: '{' enum_list opt_komma '}'  */
#line 1537 "moc.y"
                                { tmpEnum->clear();}
#line 3588 "moc_yacc.cpp"
    break;

  case 310: /* opt_identifier: IDENTIFIER  */
#line 1541 "moc.y"
                                                 { }
#line 3594 "moc_yacc.cpp"
    break;

  case 314: /* enumerator: IDENTIFIER  */
#line 1549 "moc.y"
                                     { if ( tmpAccess == Public) tmpEnum->append( (yyvsp[0].string) ); }
#line 3600 "moc_yacc.cpp"
    break;

  case 315: /* $@36: %empty  */
#line 1550 "moc.y"
                                         { enumLevel=0; }
#line 3606 "moc_yacc.cpp"
    break;

  case 316: /* enumerator: IDENTIFIER '=' $@36 enumerator_expression  */
#line 1551 "moc.y"
                                                {  if ( tmpAccess == Public) tmpEnum->append( (yyvsp[-3].string) );  }
#line 3612 "moc_yacc.cpp"
    break;

  case 317: /* $@37: %empty  */
#line 1555 "moc.y"
                                {
				     g->propWrite = "";
				     g->propRead = "";
				     g->propOverride = tmpPropOverride;
				     g->propReset = "";
				     if ( g->propOverride ) {
					 g->propStored = "";
					 g->propDesignable = "";
					 g->propScriptable = "";
				     } else {
					 g->propStored = "true";
					 g->propDesignable = "true";
					 g->propScriptable = "true";
				     }
				}
#line 3632 "moc_yacc.cpp"
    break;

  case 318: /* property: IDENTIFIER IDENTIFIER $@37 prop_statements  */
#line 1571 "moc.y"
                                {
				    if ( g->propRead.isEmpty() && !g->propOverride )
					moc_err( "A property must at least feature a read method." );
				    checkPropertyName( (yyvsp[-2].string) );
				    Q_PROPERTYdetected = TRUE;
				    // Avoid duplicates
				    for( QPtrListIterator<Property> lit( g->props ); lit.current(); ++lit ) {
					if ( lit.current()->name == (yyvsp[-2].string) ) {
					    if ( displayWarnings )
						moc_err( "Property '%s' defined twice.",
							 (const char*)lit.current()->name );
					}
				    }
				    g->props.append( new Property( lineNo, (yyvsp[-3].string), (yyvsp[-2].string),
								g->propWrite, g->propRead, g->propReset,
								   g->propStored, g->propDesignable,
								   g->propScriptable, g->propOverride ) );
				}
#line 3655 "moc_yacc.cpp"
    break;

  case 320: /* prop_statements: READ IDENTIFIER prop_statements  */
#line 1592 "moc.y"
                                                          { g->propRead = (yyvsp[-1].string); }
#line 3661 "moc_yacc.cpp"
    break;

  case 321: /* prop_statements: WRITE IDENTIFIER prop_statements  */
#line 1593 "moc.y"
                                                           { g->propWrite = (yyvsp[-1].string); }
#line 3667 "moc_yacc.cpp"
    break;

  case 322: /* prop_statements: RESET IDENTIFIER prop_statements  */
#line 1594 "moc.y"
                                                           { g->propReset = (yyvsp[-1].string); }
#line 3673 "moc_yacc.cpp"
    break;

  case 323: /* prop_statements: STORED IDENTIFIER prop_statements  */
#line 1595 "moc.y"
                                                            { g->propStored = (yyvsp[-1].string); }
#line 3679 "moc_yacc.cpp"
    break;

  case 324: /* prop_statements: DESIGNABLE IDENTIFIER prop_statements  */
#line 1596 "moc.y"
                                                                { g->propDesignable = (yyvsp[-1].string); }
#line 3685 "moc_yacc.cpp"
    break;

  case 325: /* prop_statements: SCRIPTABLE IDENTIFIER prop_statements  */
#line 1597 "moc.y"
                                                                { g->propScriptable = (yyvsp[-1].string); }
#line 3691 "moc_yacc.cpp"
    break;

  case 326: /* qt_enums: %empty  */
#line 1600 "moc.y"
                                      { }
#line 3697 "moc_yacc.cpp"
    break;

  case 327: /* qt_enums: IDENTIFIER qt_enums  */
#line 1601 "moc.y"
                                              { g->qtEnums.append( (yyvsp[-1].string) ); }
#line 3703 "moc_yacc.cpp"
    break;

  case 328: /* qt_sets: %empty  */
#line 1604 "moc.y"
                                      { }
#line 3709 "moc_yacc.cpp"
    break;

  case 329: /* qt_sets: IDENTIFIER qt_sets  */
#line 1605 "moc.y"
                                             { g->qtSets.append( (yyvsp[-1].string) ); }
#line 3715 "moc_yacc.cpp"
    break;


#line 3719 "moc_yacc.cpp"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1608 "moc.y"


#ifndef YYBISON
# if defined(Q_OS_WIN32)
# include <io.h>
# undef isatty
extern "C" int hack_isatty( int )
{
    return 0;
}
# define isatty hack_isatty
# else
# include <unistd.h>
# endif
# include "moc_lex.cpp"
#endif //YYBISON

void      cleanup();
QCString  combinePath( const char *, const char * );

FILE  *out;					// output file

parser_reg::parser_reg() : funcs(TRUE)
{
    gen_count = 0;
    noInclude     = FALSE;		// no #include <filename>
    generatedCode = FALSE;		// no code generated
    mocError = FALSE;			// moc parsing error occurred
    hasVariantIncluded = FALSE;
}


parser_reg::~parser_reg()
{
    slots.clear();
    signals.clear();
    propfuncs.clear();
    funcs.clear();
    infos.clear();
    props.clear();
    infos.clear();
}

int yyparse();

void replace( char *s, char c1, char c2 );

void setDefaultIncludeFile()
{
    if ( g->includeFiles.isEmpty() ) {
	if ( g->includePath.isEmpty() ) {
	    if ( !g->fileName.isEmpty() && !g->outputFile.isEmpty() ) {
		g->includeFiles.append( combinePath(g->fileName, g->outputFile) );
	    } else {
		g->includeFiles.append( g->fileName );
	    }
	} else {
	    g->includeFiles.append( combinePath(g->fileName, g->fileName) );
	}
    }
}

#ifdef Q_CC_MSVC
#define ErrorFormatString "%s(%d):"
#else
#define ErrorFormatString "%s:%d:"
#endif

#ifndef MOC_MWERKS_PLUGIN
int main( int argc, char **argv )
{
    init();

    bool autoInclude = TRUE;
    const char *error	     = 0;
    g->qtPath = "";
    for ( int n=1; n<argc && error==0; n++ ) {
	QCString arg = argv[n];
	if ( arg[0] == '-' ) {			// option
	    QCString opt = &arg[1];
	    if ( opt[0] == 'o' ) {		// output redirection
		if ( opt[1] == '\0' ) {
		    if ( !(n < argc-1) ) {
			error = "Missing output file name";
			break;
		    }
		    g->outputFile = argv[++n];
		} else
		    g->outputFile = &opt[1];
	    } else if ( opt == "i" ) {		// no #include statement
		g->noInclude   = TRUE;
		autoInclude = FALSE;
	    } else if ( opt[0] == 'f' ) {	// produce #include statement
		g->noInclude   = FALSE;
		autoInclude = FALSE;
		if ( opt[1] )			// -fsomething.h
		    g->includeFiles.append( &opt[1] );
	    } else if ( opt == "pch" ) {	// produce #include statement for PCH
		if ( !(n < argc-1) ) {
		    error = "Missing name of PCH file";
		    break;
		}
		g->pchFile = argv[++n];
	    } else if ( opt[0] == 'p' ) {	// include file path
		if ( opt[1] == '\0' ) {
		    if ( !(n < argc-1) ) {
			error = "Missing path name for the -p option.";
			break;
		    }
		    g->includePath = argv[++n];
		} else {
		    g->includePath = &opt[1];
		}
	    } else if ( opt[0] == 'q' ) {	// qt include file path
		if ( opt[1] == '\0' ) {
		    if ( !(n < argc-1) ) {
			error = "Missing path name for the -q option.";
			break;
		    }
		    g->qtPath = argv[++n];
		} else {
		    g->qtPath = &opt[1];
		}
		replace(g->qtPath.data(),'\\','/');
		if ( g->qtPath.right(1) != "/" )
		    g->qtPath += '/';
	    } else if ( opt == "v" ) {		// version number
		fprintf( stderr, "Qt Meta Object Compiler version %d"
				 " (Qt %s)\n", formatRevision,
				 QT_VERSION_STR );
		cleanup();
		return 1;
	    } else if ( opt == "k" ) {		// stop on errors
		errorControl = TRUE;
	    } else if ( opt == "nw" ) {		// don't display warnings
		displayWarnings = FALSE;
	    } else if ( opt == "ldbg" ) {	// lex debug output
		lexDebug = TRUE;
	    } else if ( opt == "ydbg" ) {	// yacc debug output
		yydebug = TRUE;
	    } else {
		error = "Invalid argument";
	    }
	} else {
	    if ( !g->fileName.isNull() )		// can handle only one file
		error = "Too many input files specified";
	    else
		g->fileName = arg.copy();
	}
    }

    if ( autoInclude ) {
	int ppos = g->fileName.findRev('.');
	if ( ppos != -1 && tolower( g->fileName[ppos + 1] ) == 'h' )
	    g->noInclude = FALSE;
	else
	    g->noInclude = TRUE;
    }
    setDefaultIncludeFile();

    if ( g->fileName.isNull() && !error ) {
	g->fileName = "standard input";
	yyin	 = stdin;
    } else if ( argc < 2 || error ) {		// incomplete/wrong args
	fprintf( stderr, "Qt meta object compiler\n" );
	if ( error )
	    fprintf( stderr, "moc: %s\n", error );
	fprintf( stderr, "Usage:  moc [options] <header-file>\n"
		 "\t-o file    Write output to file rather than stdout\n"
		 "\t-f[file]   Force #include, optional file name\n"
		 "\t-p path    Path prefix for included file\n"
		 "\t-i         Do not generate an #include statement\n"
		 "\t-k         Do not stop on errors\n"
		 "\t-nw        Do not display warnings\n"
		 "\t-v         Display version of moc\n" );
	cleanup();
	return 1;
    } else {
	yyin = fopen( (const char *)g->fileName, "r" );
	if ( !yyin ) {
	    fprintf( stderr, "moc: %s: No such file\n", (const char*)g->fileName);
	    cleanup();
	    return 1;
	}
    }
    if ( !g->outputFile.isEmpty() ) {		// output file specified
	out = fopen( (const char *)g->outputFile, "w" );	// create output file
	if ( !out ) {
	    fprintf( stderr, "moc: Cannot create %s\n",
		     (const char*)g->outputFile );
	    cleanup();
	    return 1;
	}
    } else {					// use stdout
	out = stdout;
    }
    yyparse();
    fclose( yyin );
    if ( !g->outputFile.isNull() )
	fclose( out );

    if ( !g->generatedCode && displayWarnings && !g->mocError ) {
	fprintf( stderr, ErrorFormatString" Warning: %s\n", g->fileName.data(), 0,
		 "No relevant classes found. No output generated." );
    }

    int ret = g->mocError ? 1 : 0;
    cleanup();
    return ret;
}
#else
bool qt_is_gui_used = FALSE;
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifdef Q_OS_MAC9
# include <Files.h>
# include <Strings.h>
# include <Errors.h>
# include "Aliases.h"
#endif
#include "CWPluginErrors.h"
#include <CWPlugins.h>
#include "DropInCompilerLinker.h"
#include <stat.h>

const unsigned char *p_str(const char *, int =-1);

CWPluginContext g_ctx;

moc_status do_moc( CWPluginContext ctx, const QCString &fin, const QCString &fout, CWFileSpec *dspec, bool i)
{
    init();

    g_ctx = ctx;
    g->noInclude = i;
    g->fileName = fin;
    g->outputFile = fout;

    setDefaultIncludeFile();

    CWFileInfo fi;
    memset(&fi, 0, sizeof(fi));
	fi.fullsearch = TRUE;
	fi.dependencyType = cwNormalDependency;
	fi.isdependentoffile = kCurrentCompiledFile;
    if(CWFindAndLoadFile( ctx, fin.data(), &fi) != cwNoErr) {
	cleanup();
	return moc_no_source;
    }

    if(dspec) {
	memcpy(dspec, &fi.filespec, sizeof(fi.filespec));
	const unsigned char *f = p_str(fout.data());
	memcpy(dspec->name, f, f[0]+1);
	free(f);
    }
    buf_size_total = fi.filedatalength;
    buf_buffer = fi.filedata;

    QCString path("");
    AliasHandle alias;
    Str63 str;
    AliasInfoType x = 1;
    char tmp[sizeof(Str63)+2];
    if(NewAlias( NULL, &fi.filespec, &alias) != noErr) {
	cleanup();
	return moc_general_error;
    }
    for(;;) {
	 GetAliasInfo(alias, x++, str);
	 if(!str[0])
	    break;
	 strncpy((char *)tmp, (const char *)str+1, str[0]);
	 tmp[str[0]] = '\0';
	 path.prepend(":");
	 path.prepend((char *)tmp);
    }
    path.prepend("MacOS 9:"); //FIXME

    QString inpath = path + fin, outpath = path + fout;
    struct stat istat, ostat;
    if(stat(inpath, &istat) == -1) {
	cleanup();
	return moc_no_source;
    }
    if(stat(outpath, &ostat) == 0 && istat.st_mtime < ostat.st_mtime) {
	cleanup();
	return moc_not_time;
    }

    unlink(outpath.data());
    out = fopen(outpath.data(), "w+");
    if(!out) {
	cleanup();
	return moc_general_error;
    }

    yyparse();
    if(out != stdout)
      fclose(out);

   if(g->mocError || !g->generatedCode) {
	unlink(outpath.data());
	moc_status ret = !g->generatedCode ? moc_no_qobject : moc_parse_error;
	cleanup();
	return ret;
    }

    cleanup();
    return moc_success;
}
#endif
void replace( char *s, char c1, char c2 )
{
    if ( !s )
	return;
    while ( *s ) {
	if ( *s == c1 )
	    *s = c2;
	s++;
    }
}

/*
    This function looks at two file names and returns the name of the
    infile with a path relative to outfile.

    Examples:

	/tmp/abc, /tmp/bcd -> abc
	xyz/a/bc, xyz/b/ac -> ../a/bc
	/tmp/abc, xyz/klm -> /tmp/abc
 */

QCString combinePath( const char *infile, const char *outfile )
{
    QFileInfo inFileInfo( QDir::current(), QFile::decodeName(infile) );
    QFileInfo outFileInfo( QDir::current(), QFile::decodeName(outfile) );
    int numCommonComponents = 0;

    QStringList inSplitted =
	QStringList::split( '/', inFileInfo.dir().canonicalPath(), TRUE );
    QStringList outSplitted =
	QStringList::split( '/', outFileInfo.dir().canonicalPath(), TRUE );

    while ( !inSplitted.isEmpty() && !outSplitted.isEmpty() &&
	    inSplitted.first() == outSplitted.first() ) {
	inSplitted.remove( inSplitted.begin() );
	outSplitted.remove( outSplitted.begin() );
	numCommonComponents++;
    }

    if ( numCommonComponents < 2 ) {
	/*
	  The paths don't have the same drive, or they don't have the
	  same root directory. Use an absolute path.
	*/
	return QFile::encodeName( inFileInfo.absFilePath() );
    } else {
	/*
	  The paths have something in common. Use a path relative to
	  the output file.
	*/
	while ( !outSplitted.isEmpty() ) {
	    outSplitted.remove( outSplitted.begin() );
	    inSplitted.prepend( ".." );
	}
	inSplitted.append( inFileInfo.fileName() );
	return QFile::encodeName( inSplitted.join("/") );
    }
}


#define getenv hack_getenv			// workaround for byacc
char *getenv()		     { return 0; }
char *getenv( const char * ) { return 0; }

void init()					// initialize
{
    BEGIN OUTSIDE;
    if(g)
	delete g;
    g = new parser_reg;
    lineNo	 = 1;
    skipClass	 = FALSE;
    skipFunc	 = FALSE;
    tmpArgList	 = new ArgList;
    tmpFunc	 = new Function;
    tmpEnum	 = new Enum;

#ifdef MOC_MWERKS_PLUGIN
    buf_buffer = NULL;
    buf_index = 0;
    buf_size_total = 0;
#endif
}

void cleanup()
{
    delete g;
    g = NULL;

#ifdef MOC_MWERKS_PLUGIN
    if(buf_buffer && g_ctx)
	CWReleaseFileText(g_ctx, buf_buffer);
#endif
}

void initClass()				 // prepare for new class
{
    tmpAccess      = Private;
    subClassPerm       = Private;
    Q_OBJECTdetected   = FALSE;
    Q_PROPERTYdetected = FALSE;
    skipClass	       = FALSE;
    templateClass      = FALSE;
    g->slots.clear();
    g->signals.clear();
    g->propfuncs.clear();
    g->enums.clear();
    g->funcs.clear();
    g->props.clear();
    g->infos.clear();
    g->qtSets.clear();
    g->qtEnums.clear();
    g->multipleSuperClasses.clear();
}

struct NamespaceInfo
{
    QCString name;
    int pLevelOnEntering; // Parenthesis level on entering the namespace
    QDict<char> definedClasses; // Classes defined in the namespace
};

QPtrList<NamespaceInfo> namespaces;

void enterNameSpace( const char *name )	 // prepare for new class
{
    static bool first = TRUE;
    if ( first ) {
	namespaces.setAutoDelete( TRUE );
	first = FALSE;
    }

    NamespaceInfo *tmp = new NamespaceInfo;
    if ( name )
	tmp->name = name;
    tmp->pLevelOnEntering = namespacePLevel;
    namespaces.append( tmp );
}

void leaveNameSpace()				 // prepare for new class
{
    NamespaceInfo *tmp = namespaces.last();
    namespacePLevel = tmp->pLevelOnEntering;
    namespaces.remove();
}

QCString nameQualifier()
{
    QPtrListIterator<NamespaceInfo> iter( namespaces );
    NamespaceInfo *tmp;
    QCString qualifier = "";
    for( ; (tmp = iter.current()) ; ++iter ) {
	if ( !tmp->name.isNull() ) {  // If not unnamed namespace
	    qualifier += tmp->name;
	    qualifier += "::";
	}
    }
    return qualifier;
}

int openNameSpaceForMetaObject( FILE *out )
{
    int levels = 0;
    QPtrListIterator<NamespaceInfo> iter( namespaces );
    NamespaceInfo *tmp;
    QCString indent = "";
    for( ; (tmp = iter.current()) ; ++iter ) {
	if ( !tmp->name.isNull() ) {  // If not unnamed namespace
	    fprintf( out, "%snamespace %s {\n", (const char *)indent,
		     (const char *) tmp->name );
	    indent += "    ";
	    levels++;
	}
    }
    QCString nm = g->className;
    int pos;
    while( (pos = nm.find( "::" )) != -1 ) {
	QCString spaceName = nm.left( pos );
	nm = nm.right( nm.length() - pos - 2 );
	if ( !spaceName.isEmpty() ) {
	    fprintf( out, "%snamespace %s {\n", (const char *)indent,
		     (const char *) spaceName );
	    indent += "    ";
	    levels++;
	}
    }
    return levels;
}

void closeNameSpaceForMetaObject( FILE *out, int levels )
{
    int i;
    for( i = 0 ; i < levels ; i++ )
	    fprintf( out, "}" );
    if ( levels )
	fprintf( out, "\n" );

}

void selectOutsideClassState()
{
    if ( namespaces.count() == 0 )
	BEGIN OUTSIDE;
    else
	BEGIN IN_NAMESPACE;
}

void registerClassInNamespace()
{
    if ( namespaces.count() == 0 )
	return;
    namespaces.last()->definedClasses.insert((const char *)g->className,(char*)1);
}

//
// Remove white space from SIGNAL and SLOT names.
// This function has been copied from qobject.cpp.
//

inline bool isSpace( char x )
{
#if defined(Q_CC_BOR)
  /*
    Borland C++ 4.5 has a weird isspace() bug.
    isspace() usually works, but not here.
    This implementation is sufficient for our internal use: rmWS()
  */
    return (uchar) x <= 32;
#else
    return isspace( (uchar) x );
#endif
}

static QCString rmWS( const char *src )
{
    QCString result( qstrlen(src)+1 );
    char *d = result.data();
    char *s = (char *)src;
    char last = 0;
    while( *s && isSpace(*s) )			// skip leading space
	s++;
    while ( *s ) {
	while ( *s && !isSpace(*s) )
	    last = *d++ = *s++;
	while ( *s && isSpace(*s) )
	    s++;
	if ( *s && isIdentChar(*s) && isIdentChar(last) )
	    last = *d++ = ' ';
    }
    result.truncate( (int)(d - result.data()) );
    return result;
}


void initExpression()
{
    g->tmpExpression = "";
}

void addExpressionString( const char *s )
{
    g->tmpExpression += s;
}

void addExpressionChar( const char c )
{
    g->tmpExpression += c;
}

void yyerror( const char *msg )			// print yacc error message
{
    g->mocError = TRUE;
#ifndef MOC_MWERKS_PLUGIN
    fprintf( stderr, ErrorFormatString" Error: %s\n", g->fileName.data(), lineNo, msg );
#else
    char	msg2[200];
    sprintf(msg2, ErrorFormatString" Error: %s", g->fileName.data(), lineNo, msg);
    CWReportMessage(g_ctx, NULL, msg2, NULL, messagetypeError, 0);
#endif
    if ( errorControl ) {
	if ( !g->outputFile.isEmpty() && yyin && fclose(yyin) == 0 )
	    remove( g->outputFile );
	exit( -1 );
    }
}

void moc_err( const char *s )
{
    yyerror( s );
}

void moc_err( const char *s1, const char *s2 )
{
    static char tmp[1024];
    sprintf( tmp, s1, s2 );
    yyerror( tmp );
}

void moc_warn( const char *msg )
{
    if ( displayWarnings )
	fprintf( stderr, ErrorFormatString" Warning: %s\n", g->fileName.data(), lineNo, msg);
}

void moc_warn( char *s1, char *s2 )
{
    static char tmp[1024];
    sprintf( tmp, s1, s2 );
    if ( displayWarnings )
	fprintf( stderr, ErrorFormatString" Warning: %s\n", g->fileName.data(), lineNo, tmp);
}

void func_warn( const char *msg )
{
    if ( !suppress_func_warn )
	moc_warn( msg );
    skipFunc = TRUE;
}

void operatorError()
{
    if ( !suppress_func_warn )
	moc_warn("Operator functions cannot be signals or slots.");
    skipFunc = TRUE;
}

#ifndef yywrap
int yywrap()					// more files?
{
    return 1;					// end of file
}
#endif

char *stradd( const char *s1, const char *s2 )	// adds two strings
{
    char *n = new char[qstrlen(s1)+qstrlen(s2)+1];
    qstrcpy( n, s1 );
    strcat( n, s2 );
    return n;
}

char *stradd( const char *s1, const char *s2, const char *s3 )// adds 3 strings
{
    char *n = new char[qstrlen(s1)+qstrlen(s2)+qstrlen(s3)+1];
    qstrcpy( n, s1 );
    strcat( n, s2 );
    strcat( n, s3 );
    return n;
}

char *stradd( const char *s1, const char *s2,
	      const char *s3, const char *s4 )// adds 4 strings
{
    char *n = new char[qstrlen(s1)+qstrlen(s2)+qstrlen(s3)+qstrlen(s4)+1];
    qstrcpy( n, s1 );
    strcat( n, s2 );
    strcat( n, s3 );
    strcat( n, s4 );
    return n;
}


char *straddSpc( const char *s1, const char *s2 )
{
    char *n = new char[qstrlen(s1)+qstrlen(s2)+2];
    qstrcpy( n, s1 );
    strcat( n, " " );
    strcat( n, s2 );
    return n;
}

char *straddSpc( const char *s1, const char *s2, const char *s3 )
{
    char *n = new char[qstrlen(s1)+qstrlen(s2)+qstrlen(s3)+3];
    qstrcpy( n, s1 );
    strcat( n, " " );
    strcat( n, s2 );
    strcat( n, " " );
    strcat( n, s3 );
    return n;
}

char *straddSpc( const char *s1, const char *s2,
	      const char *s3, const char *s4 )
{
    char *n = new char[qstrlen(s1)+qstrlen(s2)+qstrlen(s3)+qstrlen(s4)+4];
    qstrcpy( n, s1 );
    strcat( n, " " );
    strcat( n, s2 );
    strcat( n, " " );
    strcat( n, s3 );
    strcat( n, " " );
    strcat( n, s4 );
    return n;
}

// Generate C++ code for building member function table


/*
  We call B::qt_invoke() rather than A::B::qt_invoke() to
  work around a bug in MSVC 6. The bug occurs if the
  super-class is in a namespace and the sub-class isn't.

  Exception: If the superclass has the same name as the subclass, we
  want non-MSVC users to have a working generated files.
*/
QCString purestSuperClassName()
{
    QCString sc = g->superClassName;
    QCString c = g->className;
    /*
      Make sure qualified template arguments (e.g., foo<bar::baz>)
      don't interfere.
    */
    int pos = sc.findRev( "::", sc.find( '<' ) );
    if ( pos != -1 ) {
	sc = sc.right( sc.length() - pos - 2 );
	pos = c.findRev( "::" );
	if ( pos != -1 )
	    c = c.right( c.length() - pos - 2 );
	if ( sc == c )
	    sc = g->superClassName;
    }
    return sc;
}

QCString qualifiedClassName()
{
    return nameQualifier() + g->className;
}

const int Slot_Num = 1;
const int Signal_Num = 2;
const int Prop_Num = 3;

void generateFuncs( FuncList *list, const char *functype, int num )
{
    Function *f;
    for ( f=list->first(); f; f=list->next() ) {
	bool hasReturnValue = f->type != "void" && (validUType( f->type ) || isVariantType( f->type) );

	if ( hasReturnValue || !f->args->isEmpty() ) {
	    fprintf( out, "    static const QUParameter param_%s_%d[] = {\n", functype, list->at() );
	    if ( hasReturnValue ) {
		if ( validUType( f->type ) )
		    fprintf( out, "\t{ 0, &static_QUType_%s, %s, QUParameter::Out }", uType(f->type).data(), uTypeExtra(f->type).data() );
		else
		    fprintf( out, "\t{ 0, &static_QUType_QVariant, %s, QUParameter::Out }", uTypeExtra(f->type).data() );
		if ( !f->args->isEmpty() )
		    fprintf( out, ",\n" );
	    }
	    Argument* a = f->args->first();
	    while ( a ) {
		QCString type = a->leftType + ' ' + a->rightType;
		type = type.simplifyWhiteSpace();
		if( a->name.isEmpty() )
		    fprintf( out, "\t{ 0, &static_QUType_%s, %s, QUParameter::%s }",
			     uType( type ).data(), uTypeExtra( type ).data(),
			     isInOut( type ) ? "InOut" : "In" );
		else
		    fprintf( out, "\t{ \"%s\", &static_QUType_%s, %s, QUParameter::%s }",
			     a->name.data(), uType( type ).data(), uTypeExtra( type ).data(),
			     isInOut( type ) ? "InOut" : "In" );
		a = f->args->next();
		if ( a )
		    fprintf( out, ",\n" );
	    }
	    fprintf( out, "\n    };\n");
	}

	fprintf( out, "    static const QUMethod %s_%d = {", functype, list->at() );
	int n = f->args->count();
	if ( hasReturnValue )
	    n++;
	fprintf( out, "\"%s\", %d,", f->name.data(), n );
	if ( n )
	    fprintf( out, " param_%s_%d };\n", functype, list->at() );
	else
	    fprintf( out, " 0 };\n" );

	QCString typstr = "";
	int count = 0;
	Argument *a = f->args->first();
	while ( a ) {
	    if ( !a->leftType.isEmpty() || ! a->rightType.isEmpty() ) {
		if ( count++ )
		    typstr += ",";
		typstr += a->leftType;
		typstr += a->rightType;
	    }
	    a = f->args->next();
	}
	f->signature = f->name;
	f->signature += "(";
	f->signature += typstr;
	f->signature += ")";
    }
    if ( list->count() ) {
	fprintf(out,"    static const QMetaData %s_tbl[] = {\n", functype );
	f = list->first();
	while ( f ) {
	    fprintf( out, "\t{ \"%s\",", f->signature.data() );
	    fprintf( out, " &%s_%d,", functype, list->at() );
	    fprintf( out, " QMetaData::%s }", f->accessAsString() );
	    f = list->next();
	    if ( f )
		fprintf( out, ",\n");
	}
	fprintf( out, "\n    };\n" );
    }
}


int enumIndex( const char* type )
{
    int index = 0;
    for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit ) {
	if ( lit.current()->name == type )
	    return index;
	index++;
    }
    return -1;
}

bool isEnumType( const char* type )
{
    return enumIndex( type ) >= 0 ||  ( g->qtEnums.contains( type ) || g->qtSets.contains( type ) );
}

bool isPropertyType( const char* type )
{
    if ( isVariantType( type ) )
	return TRUE;

    return isEnumType( type );
}

int generateEnums()
{
    if ( g->enums.count() == 0 )
	return 0;

    fprintf( out, "#ifndef QT_NO_PROPERTIES\n" );
    int i = 0;
    for ( QPtrListIterator<Enum> it( g->enums ); it.current(); ++it, ++i ) {
	fprintf( out, "    static const QMetaEnum::Item enum_%i[] = {\n", i );
	int k = 0;
	for( QStrListIterator eit( *it.current() ); eit.current(); ++eit, ++k ) {
	    if ( k )
		fprintf( out, ",\n" );
	    fprintf( out, "\t{ \"%s\",  (int) %s::%s }", eit.current(), (const char*) g->className, eit.current() );
	}
	fprintf( out, "\n    };\n" );
    }
    fprintf( out, "    static const QMetaEnum enum_tbl[] = {\n" );
    i = 0;
    for ( QPtrListIterator<Enum> it2( g->enums ); it2.current(); ++it2, ++i ) {
	if ( i )
	    fprintf( out, ",\n" );
	fprintf( out, "\t{ \"%s\", %u, enum_%i, %s }",
		 (const char*)it2.current()->name,
		 it2.current()->count(),
		 i,
		 it2.current()->set ? "TRUE" : "FALSE" );
    }
    fprintf( out, "\n    };\n" );
    fprintf( out, "#endif // QT_NO_PROPERTIES\n" );

    return g->enums.count();
}

int generateProps()
{
    //
    // Resolve and verify property access functions
    //
    for( QPtrListIterator<Property> it( g->props ); it.current(); ) {
	Property* p = it.current();
	++it;

	// verify get function
	if ( !p->get.isEmpty() ) {
	    FuncList candidates = g->propfuncs.find( p->get );
	    for ( Function* f = candidates.first(); f; f = candidates.next() ) {
		if ( f->qualifier != "const" ) // get functions must be const
		    continue;
		if ( f->args && !f->args->isEmpty() ) // and must not take any arguments
		    continue;
		QCString tmp = f->type;
		Property::Specification spec = Property::Unspecified;
		if ( p->type == "QCString" && (tmp == "const char*" || tmp == "const char *" ) ) {
		    tmp = "QCString";
		    spec = Property::ConstCharStar;
		} else if ( tmp.right(1) == "&" ) {
		    tmp = tmp.left( tmp.length() - 1 );
		    spec = Property::Reference;
		} else if ( tmp.right(1) == "*" ) {
		    tmp = tmp.left( tmp.length() - 1 );
		    spec = Property::Pointer;
		} else {
		    spec = Property::Class;
		}
		if ( tmp.left(6) == "const " )
		    tmp = tmp.mid( 6, tmp.length() - 6 );
		tmp = tmp.simplifyWhiteSpace();
		if ( p->type == tmp ) {
		    // If it is an enum then it may not be a set
		    bool ok = TRUE;
		    for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit )
			if ( lit.current()->name == p->type && lit.current()->set )
			    ok = FALSE;
		    if ( !ok ) continue;
		    p->gspec = spec;
		    p->getfunc = f;
		    p->oredEnum = 0;
		    break;
		}
		else if ( !isVariantType( p->type ) ) {
		    if ( tmp == "int" || tmp == "uint" || tmp == "unsigned int" ) {
			// Test whether the enum is really a set (unfortunately we don't know enums of super classes)
			bool ok = TRUE;
			for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit )
			    if ( lit.current()->name == p->type && !lit.current()->set )
				ok = FALSE;
			if ( !ok ) continue;
			p->gspec = spec;
			p->getfunc = f;
			p->oredEnum = 1;
			p->enumgettype = tmp;
		    }
		}
	    }
	    if ( p->getfunc == 0 ) {
		if ( displayWarnings ) {

		    // Is the type a set, that means, mentioned in Q_SETS?
		    bool set = FALSE;
		    for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit )
			if ( lit.current()->name == p->type && lit.current()->set )
			    set = TRUE;

		    fprintf( stderr, ErrorFormatString" Warning: Property '%s' not available.\n",
			     g->fileName.data(), p->lineNo, (const char*) p->name );
		    fprintf( stderr, "   Have been looking for public get functions \n");
		    if ( !set ) {
			fprintf( stderr,
			     "      %s %s() const\n"
			     "      %s& %s() const\n"
			     "      const %s& %s() const\n"
			     "      %s* %s() const\n",
			     (const char*) p->type, (const char*) p->get,
			     (const char*) p->type, (const char*) p->get,
			     (const char*) p->type, (const char*) p->get,
			     (const char*) p->type, (const char*) p->get );
		    }
		    if ( set || !isPropertyType( p->type ) ) {
			fprintf( stderr,
			     "      int %s() const\n"
			     "      uint %s() const\n"
			     "      unsigned int %s() const\n",
			     (const char*) p->get,
			     (const char*) p->get,
			     (const char*) p->get );
		    }
		    if ( p->type == "QCString" )
			fprintf( stderr, "      const char* %s() const\n",
				 (const char*)p->get );

		    if ( candidates.isEmpty() ) {
			fprintf( stderr, "   but found nothing.\n");
		    } else {
			fprintf( stderr, "   but only found the mismatching candidate(s)\n");
			for ( Function* f = candidates.first(); f; f = candidates.next() ) {
			    QCString typstr = "";
			    Argument *a = f->args->first();
			    int count = 0;
			    while ( a ) {
				if ( !a->leftType.isEmpty() || ! a->rightType.isEmpty() ) {
				    if ( count++ )
					typstr += ",";
				    typstr += a->leftType;
				    typstr += a->rightType;
				}
				a = f->args->next();
			    }
			    fprintf( stderr, "      %s:%d: %s %s(%s) %s\n", g->fileName.data(), f->lineNo,
				     (const char*) f->type,(const char*) f->name, (const char*) typstr,
				     f->qualifier.isNull()?"":(const char*) f->qualifier );
			}
		    }
		}
	    }
	}

	// verify set function
	if ( !p->set.isEmpty() ) {
	    FuncList candidates = g->propfuncs.find( p->set );
	    for ( Function* f = candidates.first(); f; f = candidates.next() ) {
		if ( !f->args || f->args->isEmpty() )
		    continue;
		QCString tmp = f->args->first()->leftType;
		tmp = tmp.simplifyWhiteSpace();
		Property::Specification spec = Property::Unspecified;
		if ( tmp.right(1) == "&" ) {
		    tmp = tmp.left( tmp.length() - 1 );
		    spec = Property::Reference;
		}
		else {
		    spec = Property::Class;
		}
		if ( p->type == "QCString" && (tmp == "const char*" || tmp == "const char *" ) ) {
		    tmp = "QCString";
		    spec = Property::ConstCharStar;
		}
		if ( tmp.left(6) == "const " )
		    tmp = tmp.mid( 6, tmp.length() - 6 );
		tmp = tmp.simplifyWhiteSpace();

		if ( p->type == tmp && f->args->count() == 1 ) {
		    // If it is an enum then it may not be a set
		    if ( p->oredEnum == 1 )
			continue;
		    bool ok = TRUE;
		    for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit )
			if ( lit.current()->name == p->type && lit.current()->set )
			    ok = FALSE;
		    if ( !ok ) continue;
		    p->sspec = spec;
		    p->setfunc = f;
		    p->oredEnum = 0;
		    break;
		} else if ( !isVariantType( p->type ) && f->args->count() == 1 ) {
		    if ( tmp == "int" || tmp == "uint" || tmp == "unsigned int" ) {
			if ( p->oredEnum == 0 )
			    continue;
			// Test wether the enum is really a set (unfortunately we don't know enums of super classes)
			bool ok = TRUE;
			for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit )
			    if ( lit.current()->name == p->type && !lit.current()->set )
				ok = FALSE;
			if ( !ok ) continue;
			p->sspec = spec;
			p->setfunc = f;
			p->oredEnum = 1;
			p->enumsettype = tmp;
		    }
		}
	    }
	    if ( p->setfunc == 0 ) {
		if ( displayWarnings ) {

		    // Is the type a set, that means, mentioned in Q_SETS ?
		    bool set = FALSE;
		    for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit )
			if ( lit.current()->name == p->type && lit.current()->set )
			    set = TRUE;

		    fprintf( stderr, ErrorFormatString" Warning: Property '%s' not writable.\n",
			     g->fileName.data(), p->lineNo, (const char*) p->name );
		    fprintf( stderr, "   Have been looking for public set functions \n");
		    if ( !set && p->oredEnum != 1 ) {
			fprintf( stderr,
			     "      void %s( %s )\n"
			     "      void %s( %s& )\n"
			     "      void %s( const %s& )\n",
			     (const char*) p->set, (const char*) p->type,
			     (const char*) p->set, (const char*) p->type,
			     (const char*) p->set, (const char*) p->type );
		    }
		    if ( set || ( !isPropertyType( p->type ) && p->oredEnum != 0 ) ) {
			fprintf( stderr,
			     "      void %s( int )\n"
			     "      void %s( uint )\n"
			     "      void %s( unsigned int )\n",
			     (const char*) p->set,
			     (const char*) p->set,
			     (const char*) p->set );
		    }

		    if ( p->type == "QCString" )
			fprintf( stderr, "      void %s( const char* ) const\n",
				 (const char*) p->set );

		    if ( !candidates.isEmpty() ) {
			fprintf( stderr, "   but only found the mismatching candidate(s)\n");
			for ( Function* f = candidates.first(); f; f = candidates.next() ) {
			    QCString typstr = "";
			    Argument *a = f->args->first();
			    int count = 0;
			    while ( a ) {
				if ( !a->leftType.isEmpty() || ! a->rightType.isEmpty() ) {
				    if ( count++ )
					typstr += ",";
				    typstr += a->leftType;
				    typstr += a->rightType;
				}
				a = f->args->next();
			    }
			    fprintf( stderr, "      %s:%d: %s %s(%s)\n", g->fileName.data(), f->lineNo,
				     (const char*) f->type,(const char*) f->name, (const char*) typstr );
			}
		    }
		}
	    }
	}
    }

    //
    // Create meta data
    //
    if ( g->props.count() )   {
	if ( displayWarnings && !Q_OBJECTdetected )
		moc_err("The declaration of the class \"%s\" contains properties"
		" but no Q_OBJECT macro.", g->className.data());

	fprintf( out, "#ifndef QT_NO_PROPERTIES\n" );

	fprintf( out, "    static const QMetaProperty props_tbl[%d] = {\n ", g->props.count() );
	for( QPtrListIterator<Property> it( g->props ); it.current(); ++it ) {

	    fprintf( out, "\t{ \"%s\",\"%s\", ", it.current()->type.data(), it.current()->name.data() );
	    int flags = Invalid;
	    if ( !isVariantType( it.current()->type ) ) {
		flags |= EnumOrSet;
		if ( !isEnumType( it.current()->type ) )
		    flags |= UnresolvedEnum;
	    } else {
		flags |= qvariant_nameToType( it.current()->type ) << 24;
	    }
	    if ( it.current()->getfunc )
		flags |= Readable;
	    if ( it.current()->setfunc ) {
		flags |= Writable;
		if ( it.current()->stdSet() )
		    flags |= StdSet;
	    }
	    if ( it.current()->override )
		flags |= Override;

	    if ( it.current()->designable.isEmpty() )
		flags |= DesignableOverride;
	    else if ( it.current()->designable == "false" )
		flags |= NotDesignable;

	    if ( it.current()->scriptable.isEmpty() )
		flags |= ScriptableOverride;
	    else if ( it.current()->scriptable == "false" )
		flags |= NotScriptable;

	    if ( it.current()->stored.isEmpty() )
		flags |= StoredOverride;
	    else if ( it.current()->stored == "false" )
		flags |= NotStored;


	    fprintf( out, "0x%.4x, ", flags );
	    fprintf( out, "&%s::metaObj, ", (const char*) qualifiedClassName() );
	    if ( !isVariantType( it.current()->type ) ) {
		int enumpos = -1;
		int k = 0;
		for( QPtrListIterator<Enum> eit( g->enums ); eit.current(); ++eit, ++k ) {
		    if ( eit.current()->name == it.current()->type )
			enumpos = k;
		}

		// Is it an enum of this class ?
		if ( enumpos != -1 )
		    fprintf( out, "&enum_tbl[%i], ", enumpos );
		else
		    fprintf( out, "0, ");
	    } else {
		fprintf( out, "0, ");
	    }
	    fprintf( out, "-1 }" );
	    if ( !it.atLast() )
		fprintf( out, ",\n" );
	    else
		fprintf( out, "\n" );
	}
	fprintf( out, "    };\n" );
	fprintf( out, "#endif // QT_NO_PROPERTIES\n" );
    }

    return g->props.count();
}



int generateClassInfos()
{
    if ( g->infos.isEmpty() )
	return 0;

    if ( displayWarnings && !Q_OBJECTdetected )
	moc_err("The declaration of the class \"%s\" contains class infos"
		" but no Q_OBJECT macro.", g->className.data());

    fprintf( out, "    static const QClassInfo classinfo_tbl[] = {\n" );
    int i = 0;
    for( QPtrListIterator<ClassInfo> it( g->infos ); it.current(); ++it, ++i ) {
	if ( i )
	    fprintf( out, ",\n" );
	fprintf( out, "\t{ \"%s\", \"%s\" }", it.current()->name.data(),it.current()->value.data() );
    }
    fprintf( out, "\n    };\n" );
    return i;
}


void generateClass()		      // generate C++ source code for a class
{
    const char *hdr1 = "/****************************************************************************\n"
		 "** %s meta object code from reading C++ file '%s'\n**\n";
    const char *hdr2 = "** Created: %s\n"
		 "**      by: The Qt MOC ($Id: qt/moc_yacc.cpp   3.3.8   edited Feb 2 14:59 $)\n**\n";
    const char *hdr3 = "** WARNING! All changes made in this file will be lost!\n";
    const char *hdr4 = "*****************************************************************************/\n\n";
    int   i;

    if ( skipClass )				// don't generate for class
	return;

    if ( !Q_OBJECTdetected ) {
	if ( g->signals.count() == 0 && g->slots.count() == 0 && g->props.count() == 0 && g->infos.count() == 0 )
	    return;
	if ( displayWarnings && (g->signals.count() + g->slots.count()) != 0 )
	    moc_err("The declaration of the class \"%s\" contains signals "
		    "or slots\n\t but no Q_OBJECT macro.", g->className.data());
    } else {
	if ( g->superClassName.isEmpty() )
	    moc_err("The declaration of the class \"%s\" contains the\n"
		    "\tQ_OBJECT macro but does not inherit from any class!\n"
		    "\tInherit from QObject or one of its descendants"
		    " or remove Q_OBJECT.", g->className.data() );
    }
    if ( templateClass ) {			// don't generate for class
	moc_err( "Sorry, Qt does not support templates that contain\n"
		 "\tsignals, slots or Q_OBJECT." );
	return;
    }
    g->generatedCode = TRUE;
    g->gen_count++;

    if ( g->gen_count == 1 ) {			// first class to be generated
	QDateTime dt = QDateTime::currentDateTime();
	QCString dstr = dt.toString().ascii();
	QCString fn = g->fileName;
	i = g->fileName.length()-1;
	while ( i>0 && g->fileName[i-1] != '/' && g->fileName[i-1] != '\\' )
	    i--;				// skip path
	if ( i >= 0 )
	    fn = &g->fileName[i];
	fprintf( out, hdr1, (const char*)qualifiedClassName(),(const char*)fn);
	fprintf( out, hdr2, (const char*)dstr );
	fprintf( out, hdr3 );
	fprintf( out, hdr4 );

	if ( !g->noInclude ) {
	    /*
	      The header file might be a Qt header file with
	      QT_NO_COMPAT macros around signals, slots or
	      properties. Without the #undef, we cannot compile the
	      Qt library with QT_NO_COMPAT defined.

	      Header files of libraries build around Qt can also use
	      QT_NO_COMPAT, so this #undef might be beneficial to
	      users of Qt, and not only to developers of Qt.
	    */
	    fprintf( out, "#undef QT_NO_COMPAT\n" );

	    if ( !g->pchFile.isEmpty() )
	    	fprintf( out, "#include \"%s\" // PCH include\n", (const char*)g->pchFile );
	    if ( !g->includePath.isEmpty() && g->includePath.right(1) != "/" )
		g->includePath += "/";

	    g->includeFiles.first();
	    while ( g->includeFiles.current() ) {
		QCString inc = g->includeFiles.current();
		if ( inc[0] != '<' && inc[0] != '"' ) {
		    if ( !g->includePath.isEmpty() && g->includePath != "./" )
			inc.prepend( g->includePath );
		    inc = "\"" + inc + "\"";
		}
		fprintf( out, "#include %s\n", (const char *)inc );
		g->includeFiles.next();
	    }
	}
	fprintf( out, "#include <%sqmetaobject.h>\n", (const char*)g->qtPath );
	fprintf( out, "#include <%sqapplication.h>\n\n", (const char*)g->qtPath );
	fprintf( out, "#include <%sprivate/qucomextra_p.h>\n", (const char*)g->qtPath );
	fprintf( out, "#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != %d)\n", formatRevision );
	fprintf( out, "#error \"This file was generated using the moc from %s."
		 " It\"\n#error \"cannot be used with the include files from"
		 " this version of Qt.\"\n#error \"(The moc has changed too"
		 " much.)\"\n", QT_VERSION_STR );
	fprintf( out, "#endif\n\n" );
    } else {
	fprintf( out, "\n\n" );
    }

    if ( !g->hasVariantIncluded ) {
	bool needToIncludeVariant = !g->props.isEmpty();
	for ( Function* f =g->slots.first(); f && !needToIncludeVariant; f=g->slots.next() )
	    needToIncludeVariant = ( f->type != "void" && !validUType( f->type ) && isVariantType( f->type) );

	if ( needToIncludeVariant ) {
	    fprintf( out, "#include <%sqvariant.h>\n", (const char*)g->qtPath );
	    g->hasVariantIncluded = TRUE;
	}
    }

    bool isQObject =  g->className == "QObject" ;


//
// Generate virtual function className()
//
    fprintf( out, "const char *%s::className() const\n{\n    ",
	     (const char*)qualifiedClassName() );
    fprintf( out, "return \"%s\";\n}\n\n", (const char*)qualifiedClassName() );

//
// Generate static metaObj variable
//
    fprintf( out, "QMetaObject *%s::metaObj = 0;\n", (const char*)qualifiedClassName());

//
// Generate static cleanup object variable
//
    QCString cleanup = qualifiedClassName().copy();
    for ( int cnpos = 0; cnpos < cleanup.length(); cnpos++ ) {
	if ( cleanup[cnpos] == ':' )
	    cleanup[cnpos] = '_';
    }

    fprintf( out, "static QMetaObjectCleanUp cleanUp_%s( \"%s\", &%s::staticMetaObject );\n\n", (const char*)cleanup, (const char*)qualifiedClassName(), (const char*)qualifiedClassName() );

//
// Generate tr and trUtf8 member functions
//
    fprintf( out, "#ifndef QT_NO_TRANSLATION\n" );
    fprintf( out, "QString %s::tr( const char *s, const char *c )\n{\n",
	     (const char*)qualifiedClassName() );
    fprintf( out, "    if ( qApp )\n" );
    fprintf( out, "\treturn qApp->translate( \"%s\", s, c,"
		  " QApplication::DefaultCodec );\n",
	     (const char*)qualifiedClassName() );
    fprintf( out, "    else\n" );
    fprintf( out, "\treturn QString::fromLatin1( s );\n");
    fprintf( out, "}\n" );
    fprintf( out, "#ifndef QT_NO_TRANSLATION_UTF8\n" );
    fprintf( out, "QString %s::trUtf8( const char *s, const char *c )\n{\n",
	     (const char*)qualifiedClassName() );
    fprintf( out, "    if ( qApp )\n" );
    fprintf( out, "\treturn qApp->translate( \"%s\", s, c,"
		  " QApplication::UnicodeUTF8 );\n",
	     (const char*)qualifiedClassName() );
    fprintf( out, "    else\n" );
    fprintf( out, "\treturn QString::fromUtf8( s );\n" );
    fprintf( out, "}\n" );
    fprintf( out, "#endif // QT_NO_TRANSLATION_UTF8\n\n" );
    fprintf( out, "#endif // QT_NO_TRANSLATION\n\n" );

//
// Generate staticMetaObject member function
//
    fprintf( out, "QMetaObject* %s::staticMetaObject()\n{\n", (const char*)qualifiedClassName() );
    fprintf( out, "    if ( metaObj )\n\treturn metaObj;\n" );
    if ( isQObject )
	fprintf( out, "    QMetaObject* parentObject = staticQtMetaObject();\n" );
    else if ( !g->superClassName.isEmpty() )
	fprintf( out, "    QMetaObject* parentObject = %s::staticMetaObject();\n", (const char*)g->superClassName );
    else
	fprintf( out, "    QMetaObject* parentObject = 0;\n" );

//
// Build the classinfo array
//
   int n_infos = generateClassInfos();

// Build the enums array
// Enums HAVE to be generated BEFORE the properties and slots
//
    int n_enums = generateEnums();

//
// Build slots array in staticMetaObject()
//
    generateFuncs( &g->slots, "slot", Slot_Num );

//
// Build signals array in staticMetaObject()
//
    generateFuncs( &g->signals, "signal", Signal_Num );

//
// Build property array in staticMetaObject()
//
   int n_props = generateProps();

//
// Finally code to create and return meta object
//
    fprintf( out, "    metaObj = QMetaObject::new_metaobject(\n"
		  "\t\"%s\", parentObject,\n", (const char*)qualifiedClassName() );

    if ( g->slots.count() )
	fprintf( out, "\tslot_tbl, %d,\n", g->slots.count() );
    else
	fprintf( out, "\t0, 0,\n" );

    if ( g->signals.count() )
	fprintf( out, "\tsignal_tbl, %d,\n", g->signals.count() );
    else
	fprintf( out, "\t0, 0,\n" );

    fprintf( out, "#ifndef QT_NO_PROPERTIES\n" );
    if ( n_props )
	fprintf( out, "\tprops_tbl, %d,\n", n_props );
    else
	fprintf( out, "\t0, 0,\n" );
    if ( n_enums )
	fprintf( out, "\tenum_tbl, %d,\n", n_enums );
    else
	fprintf( out, "\t0, 0,\n" );
    fprintf( out, "#endif // QT_NO_PROPERTIES\n" );

    if ( n_infos )
	fprintf( out, "\tclassinfo_tbl, %d );\n", n_infos );
    else
	fprintf( out, "\t0, 0 );\n" );


//
// Setup cleanup handler and return meta object
//
    fprintf( out, "    cleanUp_%s.setMetaObject( metaObj );\n", cleanup.data() );
    fprintf( out, "    return metaObj;\n}\n" );

//
// End of function staticMetaObject()
//

//
// Generate smart cast function
//
    fprintf( out, "\nvoid* %s::qt_cast( const char* clname )\n{\n",
	     (const char*)qualifiedClassName() );
    fprintf( out, "    if ( !qstrcmp( clname, \"%s\" ) )\n"
		  "\treturn this;\n",
	     (const char*)qualifiedClassName() );
    for ( const char* cname = g->multipleSuperClasses.first(); cname; cname = g->multipleSuperClasses.next() ) {
	fprintf( out, "    if ( !qstrcmp( clname, \"%s\" ) )\n", cname);
        QCString fixed(cname);
        while (fixed.find(">>") != -1)
            fixed = fixed.replace(">>", "> >");
        fprintf( out, "\treturn (%s*)this;\n", fixed.data());
    }
    if ( !g->superClassName.isEmpty() && !isQObject )
	fprintf( out, "    return %s::qt_cast( clname );\n",
		 (const char*)purestSuperClassName() );
    else
	fprintf( out, "    return 0;\n" );
    fprintf( out, "}\n" );

//
// Generate internal signal functions
//
    Function *f;
    f = g->signals.first();			// make internal signal methods
    static bool included_list_headers = FALSE;
    int sigindex = 0;
    while ( f ) {
	QCString argstr;
	char buf[12];
	Argument *a = f->args->first();
	int offset = 0;
	const char *predef_call_func = 0;
	bool hasReturnValue = f->type != "void" && (validUType( f->type ) || isVariantType( f->type) );
	if ( hasReturnValue ) {
	    ; // no predefined function available
	} else if ( !a ) {
	    predef_call_func = "activate_signal";
	} else if ( f->args->count() == 1 ) {
	    QCString ctype = (a->leftType + ' ' + a->rightType).simplifyWhiteSpace();
	    if ( !isInOut( ctype ) ) {
		QCString utype = uType( ctype );
		if ( utype == "bool" )
		    predef_call_func = "activate_signal_bool";
		else if ( utype == "QString" || utype == "int" || utype == "double"  )
		    predef_call_func = "activate_signal";
	    }
	}

	if ( !predef_call_func && !included_list_headers ) {
	    // yes we need it, because otherwise QT_VERSION may not be defined
	    fprintf( out, "\n#include <%sqobjectdefs.h>\n", (const char*)g->qtPath );
	    fprintf( out, "#include <%sqsignalslotimp.h>\n", (const char*)g->qtPath );
	    included_list_headers = TRUE;
	}

	while ( a ) { // argument list
	    if ( !a->leftType.isEmpty() || !a->rightType.isEmpty() ) {
		argstr += a->leftType;
		argstr += " ";
		sprintf( buf, "t%d", offset++ );
		argstr += buf;
		argstr += a->rightType;
		a = f->args->next();
		if ( a )
		    argstr += ", ";
	    } else {
		a = f->args->next();
	    }
	}

	fixRightAngles( &argstr );

	fprintf( out, "\n// SIGNAL %s\n", (const char*)f->name );
	fprintf( out, "%s %s::%s(", (const char*) f->type,
		 (const char*)qualifiedClassName(),
		 (const char*)f->name );

	if ( argstr.isEmpty() )
	    fprintf( out, ")\n{\n" );
	else
	    fprintf( out, " %s )\n{\n", (const char*)argstr );

	if ( predef_call_func ) {
	    fprintf( out, "    %s( staticMetaObject()->signalOffset() + %d", predef_call_func, sigindex );
	    if ( !argstr.isEmpty() )
		fprintf( out, ", t0" );
	    fprintf( out, " );\n}\n" );
	} else {
	    if ( hasReturnValue )
		fprintf( out, "    %s something;\n", f->type.data() );
	    int nargs = f->args->count();
	    fprintf( out, "    if ( signalsBlocked() )\n\treturn%s;\n", hasReturnValue ? " something" : "" );
	    fprintf( out, "    QConnectionList *clist = receivers( staticMetaObject()->signalOffset() + %d );\n",
		     sigindex );
	    fprintf( out, "    if ( !clist )\n\treturn%s;\n", hasReturnValue ? " something" : "" );
	    fprintf( out, "    QUObject o[%d];\n", f->args->count() + 1 );

	    // initialize return value to something
	    if ( hasReturnValue ) {
		if ( validUType( f->type ) ) {
		    QCString utype = uType( f->type );
		    fprintf( out, "    static_QUType_%s.set(o,something);\n", utype.data() );
		} else if ( uType( f->type ) == "varptr" ) {
		    fprintf( out, "    static_QUType_varptr.set(o,&something);\n" );
		} else {
		    fprintf( out, "    static_QUType_ptr.set(o,&something);\n" );
		}
	    }

	    // initialize arguments
	    if ( !f->args->isEmpty() ) {
		offset = 0;
		Argument* a = f->args->first();
		while ( a ) {
		    QCString type = a->leftType + ' ' + a->rightType;
		    type = type.simplifyWhiteSpace();
		    if ( validUType( type ) ) {
			QCString utype = uType( type );
			fprintf( out, "    static_QUType_%s.set(o+%d,t%d);\n", utype.data(), offset+1, offset );
		    } else if ( uType( type ) == "varptr" ) {
			fprintf( out, "    static_QUType_varptr.set(o+%d,&t%d);\n", offset+1, offset );
		    } else {
			fprintf( out, "    static_QUType_ptr.set(o+%d,&t%d);\n", offset+1, offset );
		    }
		    a = f->args->next();
		    offset++;
		}
	    }
	    fprintf( out, "    activate_signal( clist, o );\n" );

	    // get return values from inOut parameters
	    if ( !f->args->isEmpty() ) {
		offset = 0;
		Argument* a = f->args->first();
		while ( a ) {
		    QCString type = a->leftType + ' ' + a->rightType;
		    type = type.simplifyWhiteSpace();
		    if ( validUType( type ) && isInOut( type ) ) {
			QCString utype = uType( type );
			if ( utype == "enum" )
			    fprintf( out, "    t%d = (%s)static_QUType_%s.get(o+%d);\n", offset, type.data(), utype.data(), offset+1 );
			else if ( utype == "ptr" && type.right(2) == "**" )
			    fprintf( out, "    if (t%d) *t%d = *(%s)static_QUType_ptr.get(o+%d);\n", offset, offset, type.data(), offset+1 );
			else
			    fprintf( out, "    t%d = static_QUType_%s.get(o+%d);\n", offset, utype.data(), offset+1 );
		    }
		    a = f->args->next();
		    offset++;
		}
	    }

	    // get and return return value
	    if ( hasReturnValue ) {
		QCString utype = uType( f->type );
		if ( utype == "enum" || utype == "ptr" || utype == "varptr" ) // need cast
		    fprintf( out, "    return (%s)static_QUType_%s.get(o);\n", f->type.data(), utype.data() );
		else
		    fprintf( out, "    return static_QUType_%s.get(o);\n", utype.data() );
	    }

	    fprintf( out, "}\n" );
	}

	f = g->signals.next();
	sigindex++;
    }


//
// Generate internal qt_invoke()  function
//
    fprintf( out, "\nbool %s::qt_invoke( int _id, QUObject* _o )\n{\n", qualifiedClassName().data() );

    if( !g->slots.isEmpty() ) {
	fprintf( out, "    switch ( _id - staticMetaObject()->slotOffset() ) {\n" );
	int slotindex = -1;
	for ( f = g->slots.first(); f; f = g->slots.next() ) {
	    slotindex ++;
	    if ( f->type == "void" && f->args->isEmpty() ) {
		fprintf( out, "    case %d: %s(); break;\n", slotindex, f->name.data() );
		continue;
	    }

	    fprintf( out, "    case %d: ", slotindex );
	    bool hasReturnValue = FALSE;
	    bool hasVariantReturn = FALSE;
	    if ( f->type != "void" )  {
		if (  validUType( f->type )) {
		    hasReturnValue = TRUE;
		    fprintf( out, "static_QUType_%s.set(_o,", uType(f->type).data() );
		} else if ( isVariantType( f->type ) ) {
		    hasReturnValue = hasVariantReturn = TRUE;
		    // do not need special handling for bool since this is handled as utype
		    fprintf( out, "static_QUType_QVariant.set(_o,QVariant(" );
		}
	    }
	    int offset = 0;
	    fprintf( out, "%s(", f->name.data() );
	    Argument* a = f->args->first();
	    while ( a ) {
		QCString type = a->leftType + ' ' + a->rightType;
		type = type.simplifyWhiteSpace();
		fixRightAngles( &type );
		if ( validUType( type ) ) {
		    QCString utype = uType( type );
		    if ( utype == "ptr" || utype == "varptr" || utype == "enum" )
			fprintf( out, "(%s)static_QUType_%s.get(_o+%d)", type.data(), utype.data(), offset+1 );
		    else
			fprintf( out, "(%s)static_QUType_%s.get(_o+%d)", type.data(), utype.data(), offset+1 );
		} else {
		    QCString castType = castToUType( type );
		    if(castType == type)
			fprintf( out, "(%s)(*((%s*)static_QUType_ptr.get(_o+%d)))", type.data(),
				 castType.data(), offset+1 );
		    else
			fprintf( out, "(%s)*((%s*)static_QUType_ptr.get(_o+%d))", type.data(),
				 castType.data(), offset+1 );
		}
		a = f->args->next();
		if ( a )
		    fprintf( out, "," );
		offset++;
	    }
	    fprintf( out, ")" );
	    if ( hasReturnValue )
		fprintf( out, ")" );
	    if ( hasVariantReturn )
		fprintf( out, ")" );
	    fprintf( out, "; break;\n" );
	}
	fprintf( out, "    default:\n" );

	if ( !g->superClassName.isEmpty() && !isQObject ) {
	    fprintf( out, "\treturn %s::qt_invoke( _id, _o );\n",
		     (const char *) purestSuperClassName() );
	} else {
	    fprintf( out, "\treturn FALSE;\n" );
	}
	fprintf( out, "    }\n" );
	fprintf( out, "    return TRUE;\n}\n" );
    } else {
	if ( !g->superClassName.isEmpty()  && !isQObject )
	    fprintf( out, "    return %s::qt_invoke(_id,_o);\n}\n",
		     (const char *) purestSuperClassName() );
	else
	    fprintf( out, "    return FALSE;\n}\n" );
    }


//
// Generate internal qt_emit()  function
//
    fprintf( out, "\nbool %s::qt_emit( int _id, QUObject* _o )\n{\n", qualifiedClassName().data() );

    if ( !g->signals.isEmpty() ) {
	fprintf( out, "    switch ( _id - staticMetaObject()->signalOffset() ) {\n" );
	int signalindex = -1;
	for ( f = g->signals.first(); f; f = g->signals.next() ) {
	    signalindex++;
	    if ( f->type == "void" && f->args->isEmpty() ) {
		fprintf( out, "    case %d: %s(); break;\n", signalindex, f->name.data() );
		continue;
	    }

	    fprintf( out, "    case %d: ", signalindex );
	    bool hasReturnValue = FALSE;
	    if ( f->type != "void" && validUType( f->type )) {
		hasReturnValue = TRUE;
		fprintf( out, "static_QUType_%s.set(_o,", uType(f->type).data() );
	    }
	    int offset = 0;
	    fprintf( out, "%s(", f->name.data() );
	    Argument* a = f->args->first();
	    while ( a ) {
		QCString type = a->leftType + ' ' + a->rightType;
		type = type.simplifyWhiteSpace();
		fixRightAngles( &type );
		if ( validUType( type ) ) {
		    QCString utype = uType( type );
		    if ( utype == "ptr" || utype == "varptr" || utype == "enum" )
			fprintf( out, "(%s)static_QUType_%s.get(_o+%d)", type.data(), utype.data(), offset+1 );
		    else
			fprintf( out, "(%s)static_QUType_%s.get(_o+%d)", type.data(), utype.data(), offset+1 );
		} else {
		    QCString castType = castToUType( type );
		    if(castType == type)
			fprintf( out, "(%s)(*((%s*)static_QUType_ptr.get(_o+%d)))", type.data(),
				 castType.data(), offset+1 );
		    else
			fprintf( out, "(%s)*((%s*)static_QUType_ptr.get(_o+%d))", type.data(),
				 castType.data(), offset+1 );
		}
		a = f->args->next();
		if ( a )
		    fprintf( out, "," );
		offset++;
	    }
	    fprintf( out, ")" );
	    if ( hasReturnValue )
		fprintf( out, ")" );
	    fprintf( out, "; break;\n" );
	}
	fprintf( out, "    default:\n" );
	if ( !g->superClassName.isEmpty()  && !isQObject )
	    fprintf( out, "\treturn %s::qt_emit(_id,_o);\n",
		     (const char *) purestSuperClassName() );
	else
	    fprintf( out, "\treturn FALSE;\n" );
	fprintf( out, "    }\n" );
	fprintf( out, "    return TRUE;\n}\n" );
    } else {
	if ( !g->superClassName.isEmpty()  && !isQObject )
	    fprintf( out, "    return %s::qt_emit(_id,_o);\n}\n",
		     (const char *) purestSuperClassName() );
	else
	    fprintf( out, "    return FALSE;\n}\n" );
    }


    fprintf( out, "#ifndef QT_NO_PROPERTIES\n" );
//
// Generate internal qt_property()  functions
//

    fprintf( out, "\nbool %s::qt_property( int id, int f, QVariant* v)\n{\n", qualifiedClassName().data() );

    if ( !g->props.isEmpty() ) {
	fprintf( out, "    switch ( id - staticMetaObject()->propertyOffset() ) {\n" );
	int propindex = -1;
	bool need_resolve = FALSE;

	for( QPtrListIterator<Property> it( g->props ); it.current(); ++it ){
	    propindex ++;
	    fprintf( out, "    case %d: ", propindex );
	    fprintf( out, "switch( f ) {\n" );

	    uint flag_break = 0;
	    uint flag_propagate = 0;

	    if ( it.current()->setfunc ) {
		fprintf( out, "\tcase 0: %s(", it.current()->setfunc->name.data() );
		QCString type = it.current()->type.copy(); // detach on purpose
		if ( it.current()->oredEnum )
		    type = it.current()->enumsettype;
		if ( type == "uint" )
		    fprintf( out, "v->asUInt()" );
		else if ( type == "unsigned int" )
		    fprintf( out, "(uint)v->asUInt()" );
		else if ( type == "QMap<QString,QVariant>" )
		    fprintf( out, "v->asMap()" );
		else if ( type == "QValueList<QVariant>" )
		    fprintf( out, "v->asList()" );
		else if ( type == "Q_LLONG" )
		    fprintf( out, "v->asLongLong()" );
		else if ( type == "Q_ULLONG" )
		    fprintf( out, "v->asULongLong()" );
		else if ( isVariantType( type ) ) {
		    if ( type[0] == 'Q' )
			type = type.mid(1);
		    else
			type[0] = toupper( type[0] );
		    fprintf( out, "v->as%s()", type.data() );
		} else {
		    fprintf( out, "(%s&)v->asInt()", type.data() );
		}
		fprintf( out, "); break;\n" );

	    } else if ( it.current()->override ) {
		flag_propagate |= 1 << (0+1);
	    }
	    if ( it.current()->getfunc ) {
		if ( it.current()->gspec == Property::Pointer )
		    fprintf( out, "\tcase 1: if ( this->%s() ) *v = QVariant( %s*%s()%s ); break;\n",
			     it.current()->getfunc->name.data(),
			     !isVariantType( it.current()->type ) ? "(int)" : "",
			     it.current()->getfunc->name.data(),
			     it.current()->type == "bool" ? ", 0" : "" );
		else
		    fprintf( out, "\tcase 1: *v = QVariant( %sthis->%s()%s ); break;\n",
			     !isVariantType( it.current()->type ) ? "(int)" : "",
			     it.current()->getfunc->name.data(),
			     it.current()->type == "bool" ? ", 0" : "" );
	    } else if ( it.current()->override ) {
		flag_propagate |= 1<< (1+1);
	    }

	    if ( !it.current()->reset.isEmpty() )
		fprintf( out, "\tcase 2: this->%s(); break;\n", it.current()->reset.data() );

	    if ( it.current()->designable.isEmpty() )
		flag_propagate |= 1 << (3+1);
	    else if ( it.current()->designable == "true" )
		flag_break |= 1 << (3+1);
	    else if ( it.current()->designable != "false" )
		fprintf( out, "\tcase 3: return this->%s();\n", it.current()->designable.data() );

	    if ( it.current()->scriptable.isEmpty() )
		flag_propagate |= 1 << (4+1);
	    else if ( it.current()->scriptable == "true" )
		flag_break |= 1 << (4+1);
	    else if ( it.current()->scriptable != "false" )
		fprintf( out, "\tcase 4: return this->%s();\n", it.current()->scriptable.data() );

	    if ( it.current()->stored.isEmpty() )
		flag_propagate |= 1 << (5+1);
	    else if ( it.current()->stored == "true" )
		flag_break |= 1 << (5+1);
	    else if ( it.current()->stored != "false" )
		fprintf( out, "\tcase 5: return this->%s();\n", it.current()->stored.data() );

	    int i = 0;
	    if ( flag_propagate != 0 ) {
		fprintf( out, "\t" );
		for ( i = 0; i <= 5; i++ ) {
		    if ( flag_propagate & (1 << (i+1) ) )
			fprintf( out, "case %d: ", i );
		}
		if (!g->superClassName.isEmpty() &&  !isQObject ) {
		    fprintf( out, "goto resolve;\n" );
		    need_resolve = TRUE;
		} else {
		    fprintf( out, "    return FALSE;\n" );
		}
	    }
	    if ( flag_break != 0 ) {
		fprintf( out, "\t" );
		for ( i = 0; i <= 5; i++ ) {
		    if ( flag_break & (1 << (i+1) ) )
			fprintf( out, "case %d: ", i );
		}
		fprintf( out, "break;\n");
	    }

	    fprintf( out, "\tdefault: return FALSE;\n    } break;\n" );
	}
	fprintf( out, "    default:\n" );
	if ( !g->superClassName.isEmpty()  && !isQObject )
	    fprintf( out, "\treturn %s::qt_property( id, f, v );\n",
		     (const char *) purestSuperClassName() );
	else
	    fprintf( out, "\treturn FALSE;\n" );
	fprintf( out, "    }\n" );
	fprintf( out, "    return TRUE;\n" );

	if ( need_resolve )
	    fprintf( out, "resolve:\n    return %s::qt_property( staticMetaObject()->resolveProperty(id), f, v );\n",
		     (const char *) purestSuperClassName() );
	fprintf( out, "}\n" );
    } else {
	if ( !g->superClassName.isEmpty() &&  !isQObject )
	    fprintf( out, "    return %s::qt_property( id, f, v);\n}\n",
		     (const char *) purestSuperClassName() );
	else
	    fprintf( out, "    return FALSE;\n}\n" );
    }

    fprintf( out, "\nbool %s::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }\n", qualifiedClassName().data() );
    fprintf( out, "#endif // QT_NO_PROPERTIES\n" );
}


ArgList *addArg( Argument *a )			// add argument to list
{
    if ( (!a->leftType.isEmpty() || !a->rightType.isEmpty() ) )  //filter out truely void arguments
	tmpArgList->append( a );
    return tmpArgList;
}

void addEnum()
{
    // Avoid duplicates
    for( QPtrListIterator<Enum> lit( g->enums ); lit.current(); ++lit ) {
	if ( lit.current()->name == tmpEnum->name )
	{
	    if ( displayWarnings )
		moc_err( "Enum %s defined twice.", (const char*)tmpEnum->name );
	}
    }

    // Only look at types mentioned  in Q_ENUMS and Q_SETS
    if ( g->qtEnums.contains( tmpEnum->name ) || g->qtSets.contains( tmpEnum->name ) )
    {
	g->enums.append( tmpEnum );
	if ( g->qtSets.contains( tmpEnum->name ) )
	    tmpEnum->set = TRUE;
	else
	    tmpEnum->set = FALSE;
    }
    else
	delete tmpEnum;
    tmpEnum = new Enum;
}

void addMember( Member m )
{
    if ( skipFunc ) {
	tmpFunc->args = tmpArgList; // just to be sure
	delete tmpFunc;
	tmpArgList  = new ArgList;   // ugly but works
	tmpFunc	    = new Function;
	skipFunc    = FALSE;
	return;
    }

    tmpFunc->type = tmpFunc->type.simplifyWhiteSpace();
    tmpFunc->access = tmpAccess;
    tmpFunc->args = tmpArgList;
    tmpFunc->lineNo = lineNo;

    for ( ;; ) {
	g->funcs.append( tmpFunc );

	if ( m == SignalMember ) {
	    g->signals.append( tmpFunc );
	    break;
	} else {
	    if ( m == SlotMember )
		g->slots.append( tmpFunc );
	    // PropertyCandidateMember or SlotMember
	    if ( !tmpFunc->name.isEmpty() && tmpFunc->access == Public )
		g->propfuncs.append( tmpFunc );
	    if ( !tmpFunc->args || !tmpFunc->args->hasDefaultArguments() )
		break;
	    tmpFunc = new Function( *tmpFunc );
	    tmpFunc->args = tmpFunc->args->magicClone();
	}
    }

    skipFunc = FALSE;
    tmpFunc = new Function;
    tmpArgList = new ArgList;
}

void checkPropertyName( const char* ident )
{
    if ( ident[0] == '_' ) {
	moc_err( "Invalid property name '%s'.", ident );
	return;
    }
}
