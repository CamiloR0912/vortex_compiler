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
#line 19 "parser.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vortex.h"

int yylex(void);
void yyerror(const char *s);

Node *program_root = NULL;      /* el AST del programa completo */

/* ---- Error de sintaxis pendiente ----------------------------------------
 * Bison llama a yyerror() ANTES de saber qué regla de error va a recuperar
 * el análisis. Por eso yyerror() no imprime: guarda el mensaje. Luego la
 * regla que recupera decide cómo se reporta:
 *   - una regla específica  -> [Invalid construct] (invalid_from_error)
 *   - la regla genérica     -> [Syntax error]      (flush_pending)        */
static char pending_msg[256];
static int  pending_line = 0;
static int  has_pending  = 0;

/* Imprime el error pendiente como error de sintaxis genérico */
void flush_pending(void) {
    if (!has_pending) return;
    printf("[Syntax error] line %d: %s\n", pending_line, pending_msg);
    num_errors++;
    has_pending = 0;
}

/* Constructo inválido detectado por una regla SIN el token error */
static void invalid_at(int line, const char *msg) {
    flush_pending();
    printf("[Invalid construct] line %d: %s\n", line, msg);
    num_errors++;
}

/* Constructo inválido detectado por una regla CON el token error: agrega el
 * detalle de Bison ("unexpected X, expecting Y") entre paréntesis */
static void invalid_from_error(const char *msg) {
    const char *detail = pending_msg;
    if (!has_pending) return;                 /* ya se reportó este error */
    if (strncmp(detail, "syntax error, ", 14) == 0) detail += 14;
    printf("[Invalid construct] line %d: %s (%s)\n", pending_line, msg, detail);
    num_errors++;
    has_pending = 0;
}

/* Igual que invalid_at, pero con el nombre de un identificador en el mensaje */
static void invalid_name(int line, const char *fmt, const char *name) {
    char msg[256];
    snprintf(msg, sizeof msg, fmt, name);
    invalid_at(line, msg);
}

/* ¿Se puede asignar a esta expresión? Solo a una variable o a v[i], m[i][j] */
static int is_lvalue(Node *n) {
    while (n != NULL && n->kind == N_INDEX) n = n->a;
    return n != NULL && n->kind == N_VAR;
}

/* ¿La lista de argumentos ya tiene alguno nombrado? */
static int has_named(Node *args) {
    for (; args != NULL; args = args->next)
        if (args->kind == N_NAMED) return 1;
    return 0;
}

/* Nodo de operador binario o unario */
static Node *binary(Op op, Node *l, Node *r, int line) {
    Node *n = mk(N_BINARY, line, NULL, l, r, NULL, NULL);
    n->op = op;
    return n;
}
static Node *unary(Op op, Node *e, int line) {
    Node *n = mk(N_UNARY, line, NULL, e, NULL, NULL, NULL);
    n->op = op;
    return n;
}

#line 151 "parser.tab.c"

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

#include "parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_T_INT = 3,                      /* "int"  */
  YYSYMBOL_T_FLOAT = 4,                    /* "float"  */
  YYSYMBOL_T_BOOL = 5,                     /* "bool"  */
  YYSYMBOL_T_STRING = 6,                   /* "string"  */
  YYSYMBOL_T_VOID = 7,                     /* "void"  */
  YYSYMBOL_T_VECTOR = 8,                   /* "vector"  */
  YYSYMBOL_T_MATRIX = 9,                   /* "matrix"  */
  YYSYMBOL_FUNC = 10,                      /* "func"  */
  YYSYMBOL_IF = 11,                        /* "if"  */
  YYSYMBOL_ELSE = 12,                      /* "else"  */
  YYSYMBOL_WHILE = 13,                     /* "while"  */
  YYSYMBOL_FOR = 14,                       /* "for"  */
  YYSYMBOL_SWITCH = 15,                    /* "switch"  */
  YYSYMBOL_CASE = 16,                      /* "case"  */
  YYSYMBOL_DEFAULT = 17,                   /* "default"  */
  YYSYMBOL_BREAK = 18,                     /* "break"  */
  YYSYMBOL_CONTINUE = 19,                  /* "continue"  */
  YYSYMBOL_RETURN = 20,                    /* "return"  */
  YYSYMBOL_TRY = 21,                       /* "try"  */
  YYSYMBOL_CATCH = 22,                     /* "catch"  */
  YYSYMBOL_THROW = 23,                     /* "throw"  */
  YYSYMBOL_ASSERT = 24,                    /* "assert"  */
  YYSYMBOL_PRINT = 25,                     /* "print"  */
  YYSYMBOL_SUM = 26,                       /* "sum"  */
  YYSYMBOL_PROD = 27,                      /* "prod"  */
  YYSYMBOL_MAX = 28,                       /* "max"  */
  YYSYMBOL_MIN = 29,                       /* "min"  */
  YYSYMBOL_LEN = 30,                       /* "len"  */
  YYSYMBOL_INT_LIT = 31,                   /* "integer literal"  */
  YYSYMBOL_FLOAT_LIT = 32,                 /* "float literal"  */
  YYSYMBOL_STRING_LIT = 33,                /* "string literal"  */
  YYSYMBOL_BOOL_LIT = 34,                  /* "boolean literal"  */
  YYSYMBOL_IDENTIFIER = 35,                /* "identifier"  */
  YYSYMBOL_EQ = 36,                        /* "=="  */
  YYSYMBOL_NE = 37,                        /* "!="  */
  YYSYMBOL_LE = 38,                        /* "<="  */
  YYSYMBOL_GE = 39,                        /* ">="  */
  YYSYMBOL_AND = 40,                       /* "&&"  */
  YYSYMBOL_OR = 41,                        /* "||"  */
  YYSYMBOL_NO_ELSE = 42,                   /* NO_ELSE  */
  YYSYMBOL_NO_CATCH = 43,                  /* NO_CATCH  */
  YYSYMBOL_44_ = 44,                       /* ';'  */
  YYSYMBOL_45_ = 45,                       /* '{'  */
  YYSYMBOL_46_ = 46,                       /* '}'  */
  YYSYMBOL_47_ = 47,                       /* '<'  */
  YYSYMBOL_48_ = 48,                       /* '>'  */
  YYSYMBOL_49_ = 49,                       /* '='  */
  YYSYMBOL_50_ = 50,                       /* '('  */
  YYSYMBOL_51_ = 51,                       /* ')'  */
  YYSYMBOL_52_ = 52,                       /* ':'  */
  YYSYMBOL_53_ = 53,                       /* ','  */
  YYSYMBOL_54_ = 54,                       /* '+'  */
  YYSYMBOL_55_ = 55,                       /* '-'  */
  YYSYMBOL_56_ = 56,                       /* '*'  */
  YYSYMBOL_57_ = 57,                       /* '/'  */
  YYSYMBOL_58_ = 58,                       /* '%'  */
  YYSYMBOL_59_ = 59,                       /* '!'  */
  YYSYMBOL_60_ = 60,                       /* '['  */
  YYSYMBOL_61_ = 61,                       /* ']'  */
  YYSYMBOL_YYACCEPT = 62,                  /* $accept  */
  YYSYMBOL_program = 63,                   /* program  */
  YYSYMBOL_stmt_list = 64,                 /* stmt_list  */
  YYSYMBOL_stmt = 65,                      /* stmt  */
  YYSYMBOL_block = 66,                     /* block  */
  YYSYMBOL_basic_type = 67,                /* basic_type  */
  YYSYMBOL_type = 68,                      /* type  */
  YYSYMBOL_var_decl = 69,                  /* var_decl  */
  YYSYMBOL_simple_stmt = 70,               /* simple_stmt  */
  YYSYMBOL_condition = 71,                 /* condition  */
  YYSYMBOL_if_stmt = 72,                   /* if_stmt  */
  YYSYMBOL_73_1 = 73,                      /* $@1  */
  YYSYMBOL_while_stmt = 74,                /* while_stmt  */
  YYSYMBOL_75_2 = 75,                      /* $@2  */
  YYSYMBOL_for_stmt = 76,                  /* for_stmt  */
  YYSYMBOL_77_3 = 77,                      /* $@3  */
  YYSYMBOL_for_init = 78,                  /* for_init  */
  YYSYMBOL_for_step = 79,                  /* for_step  */
  YYSYMBOL_expr_opt = 80,                  /* expr_opt  */
  YYSYMBOL_switch_stmt = 81,               /* switch_stmt  */
  YYSYMBOL_switch_error = 82,              /* switch_error  */
  YYSYMBOL_case_list = 83,                 /* case_list  */
  YYSYMBOL_default_opt = 84,               /* default_opt  */
  YYSYMBOL_func_def = 85,                  /* func_def  */
  YYSYMBOL_return_type = 86,               /* return_type  */
  YYSYMBOL_params_opt = 87,                /* params_opt  */
  YYSYMBOL_param_list = 88,                /* param_list  */
  YYSYMBOL_param = 89,                     /* param  */
  YYSYMBOL_try_stmt = 90,                  /* try_stmt  */
  YYSYMBOL_assert_stmt = 91,               /* assert_stmt  */
  YYSYMBOL_jump_stmt = 92,                 /* jump_stmt  */
  YYSYMBOL_print_stmt = 93,                /* print_stmt  */
  YYSYMBOL_invalid_stmt = 94,              /* invalid_stmt  */
  YYSYMBOL_expr = 95,                      /* expr  */
  YYSYMBOL_or_expr = 96,                   /* or_expr  */
  YYSYMBOL_and_expr = 97,                  /* and_expr  */
  YYSYMBOL_eq_expr = 98,                   /* eq_expr  */
  YYSYMBOL_rel_expr = 99,                  /* rel_expr  */
  YYSYMBOL_add_expr = 100,                 /* add_expr  */
  YYSYMBOL_mul_expr = 101,                 /* mul_expr  */
  YYSYMBOL_unary = 102,                    /* unary  */
  YYSYMBOL_postfix = 103,                  /* postfix  */
  YYSYMBOL_primary = 104,                  /* primary  */
  YYSYMBOL_reduction = 105,                /* reduction  */
  YYSYMBOL_expr_list_opt = 106,            /* expr_list_opt  */
  YYSYMBOL_expr_list = 107,                /* expr_list  */
  YYSYMBOL_args_opt = 108,                 /* args_opt  */
  YYSYMBOL_arg_list = 109,                 /* arg_list  */
  YYSYMBOL_arg = 110                       /* arg  */
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

#if 1

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
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   456

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  62
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  49
/* YYNRULES -- Number of rules.  */
#define YYNRULES  144
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  278

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   298


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
       2,     2,     2,    59,     2,     2,     2,    58,     2,     2,
      50,    51,    56,    54,    53,    55,     2,    57,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    52,    44,
      47,    49,    48,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    60,     2,    61,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    45,     2,    46,     2,     2,     2,     2,
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
      35,    36,    37,    38,    39,    40,    41,    42,    43
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   151,   151,   155,   156,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   174,   175,
     179,   185,   186,   187,   188,   192,   193,   194,   198,   201,
     203,   205,   211,   213,   221,   223,   225,   230,   232,   234,
     238,   237,   241,   243,   248,   252,   251,   255,   261,   265,
     264,   268,   273,   274,   276,   280,   281,   285,   286,   291,
     294,   299,   300,   301,   302,   306,   307,   312,   313,   320,
     323,   328,   329,   333,   334,   338,   339,   343,   345,   352,
     355,   360,   362,   365,   370,   371,   372,   373,   374,   378,
     384,   386,   388,   390,   392,   402,   406,   407,   411,   412,
     416,   417,   418,   422,   423,   424,   425,   426,   430,   431,
     432,   436,   437,   438,   439,   443,   444,   445,   449,   450,
     454,   455,   456,   457,   458,   459,   460,   462,   463,   464,
     468,   469,   470,   471,   472,   476,   477,   481,   482,   487,
     488,   492,   493,   500,   501
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "\"int\"", "\"float\"",
  "\"bool\"", "\"string\"", "\"void\"", "\"vector\"", "\"matrix\"",
  "\"func\"", "\"if\"", "\"else\"", "\"while\"", "\"for\"", "\"switch\"",
  "\"case\"", "\"default\"", "\"break\"", "\"continue\"", "\"return\"",
  "\"try\"", "\"catch\"", "\"throw\"", "\"assert\"", "\"print\"",
  "\"sum\"", "\"prod\"", "\"max\"", "\"min\"", "\"len\"",
  "\"integer literal\"", "\"float literal\"", "\"string literal\"",
  "\"boolean literal\"", "\"identifier\"", "\"==\"", "\"!=\"", "\"<=\"",
  "\">=\"", "\"&&\"", "\"||\"", "NO_ELSE", "NO_CATCH", "';'", "'{'", "'}'",
  "'<'", "'>'", "'='", "'('", "')'", "':'", "','", "'+'", "'-'", "'*'",
  "'/'", "'%'", "'!'", "'['", "']'", "$accept", "program", "stmt_list",
  "stmt", "block", "basic_type", "type", "var_decl", "simple_stmt",
  "condition", "if_stmt", "$@1", "while_stmt", "$@2", "for_stmt", "$@3",
  "for_init", "for_step", "expr_opt", "switch_stmt", "switch_error",
  "case_list", "default_opt", "func_def", "return_type", "params_opt",
  "param_list", "param", "try_stmt", "assert_stmt", "jump_stmt",
  "print_stmt", "invalid_stmt", "expr", "or_expr", "and_expr", "eq_expr",
  "rel_expr", "add_expr", "mul_expr", "unary", "postfix", "primary",
  "reduction", "expr_list_opt", "expr_list", "args_opt", "arg_list", "arg", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-85)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-53)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -85,    10,    40,   -85,    49,   -85,   -85,   -85,   -85,   -31,
     -24,   106,    26,    -3,    30,   -22,    57,   361,    73,    76,
     101,   315,   107,   111,   361,   113,   124,   -85,   -85,   -85,
     -85,   -85,   -85,   -85,   -85,   -85,   135,   -85,   361,   361,
     361,   303,   -85,   -85,   -85,   154,   -85,   148,   -85,   -85,
     -85,   -85,   -85,   -85,   -85,   -85,   -85,   -85,   146,   158,
     160,   -23,   -18,    75,    60,   -85,   142,   -85,   166,   -85,
     -85,   155,    29,   168,    29,   -85,   172,   173,   -85,   187,
     -85,   180,    83,   -85,   -85,   -85,    90,   138,   178,   174,
     107,   -85,   -85,   -85,   181,   205,   193,   188,   350,   361,
     396,   268,   182,   -85,   -85,   -85,   -85,   -36,   -37,   -85,
     361,   361,   361,   361,   361,   361,   361,   361,   361,   361,
     361,   361,   361,   361,   361,   361,   361,   183,   361,   189,
     186,   107,   190,   -25,   194,   232,   107,   201,   -85,   196,
     213,   -85,   207,   -85,   107,   -85,   -85,   199,   202,   -85,
     208,    68,   204,   203,    88,   -85,   206,   209,   -85,   -85,
     -85,   361,   -85,   -85,   361,   -85,   160,   -23,   -18,   -18,
      75,    75,    75,    75,    60,    60,   -85,   -85,   -85,   197,
     210,   215,   -85,   219,   -85,   145,   -85,   -85,   361,   -85,
     -85,    36,   -85,   -85,   -85,   216,   361,   117,   -85,   231,
     107,   -85,   224,   361,   260,   361,   -85,   396,   -85,   261,
     -85,   -85,   -85,   -85,   240,   255,   254,   -85,   257,   265,
     -85,   -85,   107,   361,   266,   -85,   361,   259,    37,   264,
     -85,   -85,   269,   -85,   -85,   -85,   -85,   263,   132,   145,
     -85,   -85,   -85,   -85,   361,   267,   107,   134,   -85,    85,
     107,   272,   361,   -85,   -85,   -85,   -85,   270,   107,   -85,
     -85,   -85,   361,   273,   -85,   -85,   -85,   -85,    91,   -85,
     274,   107,   278,   -85,   107,   -85,   -85,   -85
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     1,     0,    21,    22,    23,    24,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   130,   131,   132,
     133,   134,   120,   121,   122,   123,   124,     3,     0,     0,
       0,     0,     4,     7,    25,     0,     5,     0,     8,     9,
      10,    11,    12,    13,    15,    14,    16,    17,    32,    95,
      96,    98,   100,   103,   108,   111,   115,   118,     0,    18,
      19,     0,     0,     0,     0,    72,     0,     0,    71,     0,
      40,     0,     0,    90,    91,    45,     0,     0,     0,     0,
       0,    86,    87,    84,     0,    80,     0,     0,     0,   135,
     139,     0,     0,   116,   117,   128,   137,     0,     0,     6,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    37,     0,     0,    44,     0,
       0,    54,     0,    65,     0,    93,    85,     0,     0,    88,
       0,     0,     0,   136,   124,   143,     0,   140,   141,    20,
     127,     0,   129,    29,     0,    33,    97,    99,   101,   102,
     106,   107,   104,   105,   109,   110,   112,   113,   114,     0,
       0,     0,    26,     0,    27,    73,    41,    36,     0,    34,
      42,     0,    46,    47,    49,     0,    57,    67,    92,     0,
       0,    83,     0,     0,     0,     0,   125,     0,   138,     0,
     119,   126,    30,    31,     0,     0,    74,    75,     0,     0,
      38,    39,     0,     0,     0,    58,     0,     0,     0,     0,
      94,    81,     0,    89,   144,   142,    28,    77,     0,     0,
      35,    43,    50,    53,    55,     0,     0,     0,    59,     0,
       0,     0,     0,    70,    69,    76,    56,     0,     0,    68,
      62,    61,     0,     0,    60,    79,    82,    78,     0,    66,
       0,     0,     0,    48,     0,    64,    51,    63
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -85,   -85,   280,   -85,    -4,    84,   -10,   -85,   -84,   141,
     -11,   -85,   -85,   -85,   -85,   -85,   -85,   -85,   -85,   -85,
     -85,   -85,   -85,   -85,   -85,   -85,   -85,   100,   -85,   -85,
     -85,   -85,   -85,    -2,   -85,   229,   212,    70,   -12,    71,
     -34,   -85,   -85,   -85,   -85,   252,   -85,   -85,   147
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,    42,    43,    44,    45,    46,    47,    82,
      48,   131,    49,   136,    50,   222,   142,   257,   224,    51,
     249,   197,   228,    52,    79,   215,   216,   217,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,   152,   107,   156,   157,   158
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      70,    78,    84,   141,    71,   103,   104,   163,    12,    83,
       3,    73,   164,   113,   114,    89,    72,   161,    95,    94,
     115,   116,    97,    74,   188,   162,   189,    80,    87,   117,
     118,    85,     5,     6,     7,     8,   102,   219,   247,   106,
      -2,     4,    37,     5,     6,     7,     8,    12,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    81,   140,   135,   133,
      81,    37,   138,   248,   134,    37,   145,   176,   177,   178,
      38,   137,   272,    69,    37,    39,   151,   106,   155,    40,
      41,   262,   263,   170,   171,   172,   173,    81,   165,     5,
       6,     7,     8,    75,    76,    77,   121,   122,   123,   202,
      91,   203,   179,   180,   181,    90,   183,   186,    37,   119,
     120,   264,   192,   226,   227,    37,    37,   205,   100,   139,
     198,     5,     6,     7,     8,    92,    76,    77,     5,     6,
       7,     8,    37,    76,    77,    86,   127,    88,   129,   208,
     256,    96,   209,    98,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    99,   214,   253,    37,   260,    37,
     221,   132,   -52,   168,   169,   100,   218,   220,    38,   108,
     174,   175,   109,    39,   225,   110,   230,    40,    41,   111,
     112,   232,   124,   234,   126,   155,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,   125,   128,   242,    72,
      74,   243,   130,   143,   245,   146,   144,   147,   148,   214,
      38,   182,   149,   160,   254,    39,   185,   184,   190,    40,
      41,   187,   259,   261,   191,   193,   265,   194,   195,   199,
     267,   196,   201,   200,   269,   204,   161,   206,   210,   212,
     270,   211,   207,   213,   273,   223,   229,   275,   231,     4,
     277,     5,     6,     7,     8,   237,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,   233,   236,   238,   239,   240,   241,
     244,   246,   252,    37,   159,   250,   266,   101,    38,   258,
     251,   268,   276,    39,   167,   271,   274,    40,    41,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,   255,
     166,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,   153,     0,    38,   235,     0,     0,     0,    39,    93,
       0,     0,    40,    41,   105,    38,     0,     0,     0,     0,
      39,     0,     0,     0,    40,    41,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,     0,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,     0,     0,     0,
      38,   150,     0,     0,     0,    39,     0,     0,     0,    40,
      41,    38,     0,     0,     0,     0,    39,     0,     0,     0,
      40,    41,    27,    28,    29,    30,    31,    32,    33,    34,
      35,   154,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    38,     0,     0,     0,
       0,    39,     0,     0,     0,    40,    41
};

static const yytype_int16 yycheck[] =
{
       4,    11,    13,    87,    35,    39,    40,    44,    11,    13,
       0,    35,    49,    36,    37,    17,    47,    53,    22,    21,
      38,    39,    24,    47,    49,    61,    51,     1,    50,    47,
      48,     1,     3,     4,     5,     6,    38,     1,     1,    41,
       0,     1,    45,     3,     4,     5,     6,    11,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    50,    87,    82,    81,
      50,    45,    86,    46,     1,    45,    90,   121,   122,   123,
      50,     1,     1,    44,    45,    55,    98,    99,   100,    59,
      60,    16,    17,   115,   116,   117,   118,    50,   110,     3,
       4,     5,     6,     7,     8,     9,    56,    57,    58,    51,
      44,    53,   124,   125,   126,    52,   128,   131,    45,    54,
      55,    46,   136,    16,    17,    45,    45,    49,    50,     1,
     144,     3,     4,     5,     6,    44,     8,     9,     3,     4,
       5,     6,    45,     8,     9,    14,    72,    16,    74,   161,
     244,    50,   164,    50,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    50,   185,    44,    45,    44,    45,
     191,     1,    44,   113,   114,    50,   188,   191,    50,    35,
     119,   120,    44,    55,   196,    49,   200,    59,    60,    41,
      40,   203,    60,   205,    49,   207,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    50,    49,   222,    47,
      47,   223,    35,    45,   226,    44,    52,    22,    35,   239,
      50,    48,    44,    51,   238,    55,    50,    48,    44,    59,
      60,    51,   246,   247,    12,    44,   250,    51,    35,    50,
     252,    44,    44,    51,   258,    51,    53,    51,    61,    44,
     262,    51,    53,    44,   268,    49,    35,   271,    44,     1,
     274,     3,     4,     5,     6,    35,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    44,    44,    51,    53,    51,    44,
      44,    52,    49,    45,    46,    51,    44,    37,    50,    52,
      51,    51,    44,    55,   112,    52,    52,    59,    60,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,   239,
     111,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    99,    -1,    50,   207,    -1,    -1,    -1,    55,    44,
      -1,    -1,    59,    60,    61,    50,    -1,    -1,    -1,    -1,
      55,    -1,    -1,    -1,    59,    60,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    -1,
      50,    51,    -1,    -1,    -1,    55,    -1,    -1,    -1,    59,
      60,    50,    -1,    -1,    -1,    -1,    55,    -1,    -1,    -1,
      59,    60,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    50,    -1,    -1,    -1,
      -1,    55,    -1,    -1,    -1,    59,    60
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    63,    64,     0,     1,     3,     4,     5,     6,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    45,    50,    55,
      59,    60,    65,    66,    67,    68,    69,    70,    72,    74,
      76,    81,    85,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,    44,
      66,    35,    47,    35,    47,     7,     8,     9,    68,    86,
       1,    50,    71,    66,    72,     1,    71,    50,    71,    95,
      52,    44,    44,    44,    95,    66,    50,    95,    50,    50,
      50,    64,    95,   102,   102,    61,    95,   107,    35,    44,
      49,    41,    40,    36,    37,    38,    39,    47,    48,    54,
      55,    56,    57,    58,    60,    50,    49,    67,    49,    67,
      35,    73,     1,    95,     1,    66,    75,     1,    66,     1,
      68,    70,    78,    45,    52,    66,    44,    22,    35,    44,
      51,    95,   106,   107,    35,    95,   108,   109,   110,    46,
      51,    53,    61,    44,    49,    95,    97,    98,    99,    99,
     100,   100,   100,   100,   101,   101,   102,   102,   102,    95,
      95,    95,    48,    95,    48,    50,    66,    51,    49,    51,
      44,    12,    66,    44,    51,    35,    44,    83,    66,    50,
      51,    44,    51,    53,    51,    49,    51,    53,    95,    95,
      61,    51,    44,    44,    68,    87,    88,    89,    95,     1,
      66,    72,    77,    49,    80,    95,    16,    17,    84,    35,
      66,    44,    95,    44,    95,   110,    44,    35,    51,    53,
      51,    44,    66,    95,    44,    95,    52,     1,    46,    82,
      51,    51,    49,    44,    66,    89,    70,    79,    52,    66,
      44,    66,    16,    17,    46,    66,    44,    95,    51,    66,
      95,    52,     1,    66,    52,    66,    44,    66
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    62,    63,    64,    64,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      66,    67,    67,    67,    67,    68,    68,    68,    69,    69,
      69,    69,    70,    70,    71,    71,    71,    72,    72,    72,
      73,    72,    72,    72,    74,    75,    74,    74,    76,    77,
      76,    76,    78,    78,    78,    79,    79,    80,    80,    81,
      81,    82,    82,    82,    82,    83,    83,    84,    84,    85,
      85,    86,    86,    87,    87,    88,    88,    89,    89,    90,
      90,    91,    91,    91,    92,    92,    92,    92,    92,    93,
      94,    94,    94,    94,    94,    95,    96,    96,    97,    97,
      98,    98,    98,    99,    99,    99,    99,    99,   100,   100,
     100,   101,   101,   101,   101,   102,   102,   102,   103,   103,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     105,   105,   105,   105,   105,   106,   106,   107,   107,   108,
     108,   109,   109,   110,   110
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       3,     1,     1,     1,     1,     1,     4,     4,     5,     3,
       5,     5,     1,     3,     3,     5,     3,     3,     5,     5,
       0,     4,     4,     6,     3,     0,     4,     4,     9,     0,
       6,    10,     0,     4,     1,     0,     1,     0,     1,     6,
       7,     2,     2,     5,     4,     0,     5,     0,     3,     7,
       7,     1,     1,     0,     1,     1,     3,     2,     4,     7,
       2,     5,     7,     4,     2,     3,     2,     2,     3,     5,
       2,     2,     4,     3,     5,     1,     1,     3,     1,     3,
       1,     3,     3,     1,     3,     3,     3,     3,     1,     3,
       3,     1,     3,     3,     3,     1,     2,     2,     1,     4,
       1,     1,     1,     1,     1,     4,     4,     3,     2,     3,
       1,     1,     1,     1,     1,     0,     1,     1,     3,     0,
       1,     1,     3,     1,     3
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

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


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


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
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
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp);
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
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
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]));
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule); \
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


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_INT_LIT: /* "integer literal"  */
#line 136 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1575 "parser.tab.c"
        break;

    case YYSYMBOL_FLOAT_LIT: /* "float literal"  */
#line 136 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1581 "parser.tab.c"
        break;

    case YYSYMBOL_STRING_LIT: /* "string literal"  */
#line 136 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1587 "parser.tab.c"
        break;

    case YYSYMBOL_BOOL_LIT: /* "boolean literal"  */
#line 136 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1593 "parser.tab.c"
        break;

    case YYSYMBOL_IDENTIFIER: /* "identifier"  */
#line 136 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1599 "parser.tab.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Location data for the lookahead symbol.  */
YYLTYPE yylloc
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
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

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
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
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
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
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

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
      yyerror_range[1] = yylloc;
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
  *++yylsp = yylloc;

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

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* program: stmt_list  */
#line 151 "parser.y"
                                    { program_root = (yyvsp[0].node); }
#line 1897 "parser.tab.c"
    break;

  case 3: /* stmt_list: %empty  */
#line 155 "parser.y"
                                    { (yyval.node) = NULL; }
#line 1903 "parser.tab.c"
    break;

  case 4: /* stmt_list: stmt_list stmt  */
#line 156 "parser.y"
                                    { (yyval.node) = list_append((yyvsp[-1].node), (yyvsp[0].node)); }
#line 1909 "parser.tab.c"
    break;

  case 18: /* stmt: error ';'  */
#line 174 "parser.y"
                                    { flush_pending(); yyerrok; (yyval.node) = NULL; }
#line 1915 "parser.tab.c"
    break;

  case 19: /* stmt: error block  */
#line 175 "parser.y"
                                    { flush_pending(); yyerrok; (yyval.node) = NULL; }
#line 1921 "parser.tab.c"
    break;

  case 20: /* block: '{' stmt_list '}'  */
#line 179 "parser.y"
                                    { (yyval.node) = mk(N_BLOCK, (yylsp[-2]).first_line, NULL, (yyvsp[-1].node), NULL, NULL, NULL); }
#line 1927 "parser.tab.c"
    break;

  case 21: /* basic_type: "int"  */
#line 185 "parser.y"
                                    { (yyval.type) = type_of(TY_INT, TY_ERROR); }
#line 1933 "parser.tab.c"
    break;

  case 22: /* basic_type: "float"  */
#line 186 "parser.y"
                                    { (yyval.type) = type_of(TY_FLOAT, TY_ERROR); }
#line 1939 "parser.tab.c"
    break;

  case 23: /* basic_type: "bool"  */
#line 187 "parser.y"
                                    { (yyval.type) = type_of(TY_BOOL, TY_ERROR); }
#line 1945 "parser.tab.c"
    break;

  case 24: /* basic_type: "string"  */
#line 188 "parser.y"
                                    { (yyval.type) = type_of(TY_STRING, TY_ERROR); }
#line 1951 "parser.tab.c"
    break;

  case 26: /* type: "vector" '<' basic_type '>'  */
#line 193 "parser.y"
                                    { (yyval.type) = type_of(TY_VECTOR, (yyvsp[-1].type).kind); }
#line 1957 "parser.tab.c"
    break;

  case 27: /* type: "matrix" '<' basic_type '>'  */
#line 194 "parser.y"
                                    { (yyval.type) = type_of(TY_MATRIX, (yyvsp[-1].type).kind); }
#line 1963 "parser.tab.c"
    break;

  case 28: /* var_decl: type "identifier" '=' expr ';'  */
#line 199 "parser.y"
        { (yyval.node) = mk(N_VARDECL, (yylsp[-3]).first_line, (yyvsp[-3].str), (yyvsp[-1].node), NULL, NULL, NULL); (yyval.node)->type = (yyvsp[-4].type); }
#line 1969 "parser.tab.c"
    break;

  case 29: /* var_decl: type "identifier" ';'  */
#line 202 "parser.y"
        { invalid_name((yylsp[-1]).first_line, "variable '%s' must be initialized", (yyvsp[-1].str)); free((yyvsp[-1].str)); (yyval.node) = NULL; }
#line 1975 "parser.tab.c"
    break;

  case 30: /* var_decl: "vector" "identifier" '=' expr ';'  */
#line 204 "parser.y"
        { invalid_at((yylsp[-4]).first_line, "'vector' requires an element type, e.g. vector<int>"); free((yyvsp[-3].str)); (yyval.node) = NULL; }
#line 1981 "parser.tab.c"
    break;

  case 31: /* var_decl: "matrix" "identifier" '=' expr ';'  */
#line 206 "parser.y"
        { invalid_at((yylsp[-4]).first_line, "'matrix' requires an element type, e.g. matrix<int>"); free((yyvsp[-3].str)); (yyval.node) = NULL; }
#line 1987 "parser.tab.c"
    break;

  case 32: /* simple_stmt: expr  */
#line 212 "parser.y"
        { (yyval.node) = mk(N_EXPRSTMT, (yylsp[0]).first_line, NULL, (yyvsp[0].node), NULL, NULL, NULL); }
#line 1993 "parser.tab.c"
    break;

  case 33: /* simple_stmt: expr '=' expr  */
#line 214 "parser.y"
        { if (!is_lvalue((yyvsp[-2].node))) invalid_at((yylsp[-2]).first_line, "invalid assignment target");
          (yyval.node) = mk(N_ASSIGN, (yylsp[-2]).first_line, NULL, (yyvsp[-2].node), (yyvsp[0].node), NULL, NULL); }
#line 2000 "parser.tab.c"
    break;

  case 34: /* condition: '(' expr ')'  */
#line 221 "parser.y"
                                    { (yyval.node) = (yyvsp[-1].node); }
#line 2006 "parser.tab.c"
    break;

  case 35: /* condition: '(' expr '=' expr ')'  */
#line 224 "parser.y"
        { invalid_at((yylsp[-2]).first_line, "assignment '=' used as a condition; use '==' to compare"); (yyval.node) = NULL; }
#line 2012 "parser.tab.c"
    break;

  case 36: /* condition: '(' error ')'  */
#line 226 "parser.y"
        { invalid_from_error("invalid condition"); yyerrok; (yyval.node) = NULL; }
#line 2018 "parser.tab.c"
    break;

  case 37: /* if_stmt: "if" condition block  */
#line 231 "parser.y"
        { (yyval.node) = mk(N_IF, (yylsp[-2]).first_line, NULL, (yyvsp[-1].node), (yyvsp[0].node), NULL, NULL); }
#line 2024 "parser.tab.c"
    break;

  case 38: /* if_stmt: "if" condition block "else" block  */
#line 233 "parser.y"
        { (yyval.node) = mk(N_IF, (yylsp[-4]).first_line, NULL, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[0].node), NULL); }
#line 2030 "parser.tab.c"
    break;

  case 39: /* if_stmt: "if" condition block "else" if_stmt  */
#line 235 "parser.y"
        { (yyval.node) = mk(N_IF, (yylsp[-4]).first_line, NULL, (yyvsp[-3].node), (yyvsp[-2].node), (yyvsp[0].node), NULL); }
#line 2036 "parser.tab.c"
    break;

  case 40: /* $@1: %empty  */
#line 238 "parser.y"
        { invalid_from_error("condition must be enclosed in parentheses"); }
#line 2042 "parser.tab.c"
    break;

  case 41: /* if_stmt: "if" error $@1 block  */
#line 240 "parser.y"
        { yyerrok; (yyval.node) = NULL; }
#line 2048 "parser.tab.c"
    break;

  case 42: /* if_stmt: "if" condition error ';'  */
#line 242 "parser.y"
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; (yyval.node) = NULL; }
#line 2054 "parser.tab.c"
    break;

  case 43: /* if_stmt: "if" condition block "else" error ';'  */
#line 244 "parser.y"
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; (yyval.node) = NULL; }
#line 2060 "parser.tab.c"
    break;

  case 44: /* while_stmt: "while" condition block  */
#line 249 "parser.y"
        { (yyval.node) = mk(N_WHILE, (yylsp[-2]).first_line, NULL, (yyvsp[-1].node), (yyvsp[0].node), NULL, NULL); }
#line 2066 "parser.tab.c"
    break;

  case 45: /* $@2: %empty  */
#line 252 "parser.y"
        { invalid_from_error("condition must be enclosed in parentheses"); }
#line 2072 "parser.tab.c"
    break;

  case 46: /* while_stmt: "while" error $@2 block  */
#line 254 "parser.y"
        { yyerrok; (yyval.node) = NULL; }
#line 2078 "parser.tab.c"
    break;

  case 47: /* while_stmt: "while" condition error ';'  */
#line 256 "parser.y"
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; (yyval.node) = NULL; }
#line 2084 "parser.tab.c"
    break;

  case 48: /* for_stmt: "for" '(' for_init ';' expr_opt ';' for_step ')' block  */
#line 262 "parser.y"
        { (yyval.node) = mk(N_FOR, (yylsp[-8]).first_line, NULL, (yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[0].node)); }
#line 2090 "parser.tab.c"
    break;

  case 49: /* $@3: %empty  */
#line 265 "parser.y"
        { invalid_from_error("malformed 'for' header, expected for (init; condition; step)"); yyerrok; }
#line 2096 "parser.tab.c"
    break;

  case 50: /* for_stmt: "for" '(' error ')' $@3 block  */
#line 267 "parser.y"
        { (yyval.node) = NULL; }
#line 2102 "parser.tab.c"
    break;

  case 51: /* for_stmt: "for" '(' for_init ';' expr_opt ';' for_step ')' error ';'  */
#line 269 "parser.y"
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; (yyval.node) = NULL; }
#line 2108 "parser.tab.c"
    break;

  case 52: /* for_init: %empty  */
#line 273 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2114 "parser.tab.c"
    break;

  case 53: /* for_init: type "identifier" '=' expr  */
#line 275 "parser.y"
        { (yyval.node) = mk(N_VARDECL, (yylsp[-2]).first_line, (yyvsp[-2].str), (yyvsp[0].node), NULL, NULL, NULL); (yyval.node)->type = (yyvsp[-3].type); }
#line 2120 "parser.tab.c"
    break;

  case 55: /* for_step: %empty  */
#line 280 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2126 "parser.tab.c"
    break;

  case 57: /* expr_opt: %empty  */
#line 285 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2132 "parser.tab.c"
    break;

  case 59: /* switch_stmt: "switch" condition '{' case_list default_opt '}'  */
#line 292 "parser.y"
        { (yyval.node) = mk(N_SWITCH, (yylsp[-5]).first_line, NULL, (yyvsp[-4].node), list_append((yyvsp[-2].node), (yyvsp[-1].node)), NULL, NULL); }
#line 2138 "parser.tab.c"
    break;

  case 60: /* switch_stmt: "switch" condition '{' case_list default_opt switch_error '}'  */
#line 295 "parser.y"
        { yyerrok; (yyval.node) = NULL; }
#line 2144 "parser.tab.c"
    break;

  case 61: /* switch_error: error block  */
#line 299 "parser.y"
                                            { flush_pending(); }
#line 2150 "parser.tab.c"
    break;

  case 62: /* switch_error: error ';'  */
#line 300 "parser.y"
                                            { flush_pending(); }
#line 2156 "parser.tab.c"
    break;

  case 65: /* case_list: %empty  */
#line 306 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2162 "parser.tab.c"
    break;

  case 66: /* case_list: case_list "case" expr ':' block  */
#line 308 "parser.y"
        { (yyval.node) = list_append((yyvsp[-4].node), mk(N_CASE, (yylsp[-3]).first_line, NULL, (yyvsp[-2].node), (yyvsp[0].node), NULL, NULL)); }
#line 2168 "parser.tab.c"
    break;

  case 67: /* default_opt: %empty  */
#line 312 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2174 "parser.tab.c"
    break;

  case 68: /* default_opt: "default" ':' block  */
#line 314 "parser.y"
        { (yyval.node) = mk(N_CASE, (yylsp[-2]).first_line, NULL, NULL, (yyvsp[0].node), NULL, NULL); }
#line 2180 "parser.tab.c"
    break;

  case 69: /* func_def: "func" return_type "identifier" '(' params_opt ')' block  */
#line 321 "parser.y"
        { (yyval.node) = mk(N_FUNC, (yylsp[-4]).first_line, (yyvsp[-4].str), (yyvsp[-2].node), (yyvsp[0].node), NULL, NULL); (yyval.node)->type = (yyvsp[-5].type); }
#line 2186 "parser.tab.c"
    break;

  case 70: /* func_def: "func" return_type "identifier" '(' params_opt ')' ';'  */
#line 324 "parser.y"
        { invalid_name((yylsp[-4]).first_line, "function '%s' has no body; use { }", (yyvsp[-4].str)); free((yyvsp[-4].str)); (yyval.node) = NULL; }
#line 2192 "parser.tab.c"
    break;

  case 72: /* return_type: "void"  */
#line 329 "parser.y"
                                    { (yyval.type) = type_of(TY_VOID, TY_ERROR); }
#line 2198 "parser.tab.c"
    break;

  case 73: /* params_opt: %empty  */
#line 333 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2204 "parser.tab.c"
    break;

  case 76: /* param_list: param_list ',' param  */
#line 339 "parser.y"
                                    { (yyval.node) = list_append((yyvsp[-2].node), (yyvsp[0].node)); }
#line 2210 "parser.tab.c"
    break;

  case 77: /* param: type "identifier"  */
#line 344 "parser.y"
        { (yyval.node) = mk(N_PARAM, (yylsp[0]).first_line, (yyvsp[0].str), NULL, NULL, NULL, NULL); (yyval.node)->type = (yyvsp[-1].type); }
#line 2216 "parser.tab.c"
    break;

  case 78: /* param: type "identifier" '=' expr  */
#line 346 "parser.y"
        { (yyval.node) = mk(N_PARAM, (yylsp[-2]).first_line, (yyvsp[-2].str), (yyvsp[0].node), NULL, NULL, NULL); (yyval.node)->type = (yyvsp[-3].type); }
#line 2222 "parser.tab.c"
    break;

  case 79: /* try_stmt: "try" block "catch" '(' "identifier" ')' block  */
#line 353 "parser.y"
        { (yyval.node) = mk(N_TRY, (yylsp[-6]).first_line, (yyvsp[-2].str), (yyvsp[-5].node), (yyvsp[0].node), NULL, NULL); }
#line 2228 "parser.tab.c"
    break;

  case 80: /* try_stmt: "try" block  */
#line 356 "parser.y"
        { invalid_at((yylsp[-1]).first_line, "'try' without 'catch'"); (yyval.node) = NULL; }
#line 2234 "parser.tab.c"
    break;

  case 81: /* assert_stmt: "assert" '(' expr ')' ';'  */
#line 361 "parser.y"
        { (yyval.node) = mk(N_ASSERT, (yylsp[-4]).first_line, NULL, (yyvsp[-2].node), NULL, NULL, NULL); }
#line 2240 "parser.tab.c"
    break;

  case 82: /* assert_stmt: "assert" '(' expr ',' expr ')' ';'  */
#line 363 "parser.y"
        { (yyval.node) = mk(N_ASSERT, (yylsp[-6]).first_line, NULL, (yyvsp[-4].node), (yyvsp[-2].node), NULL, NULL); }
#line 2246 "parser.tab.c"
    break;

  case 83: /* assert_stmt: "assert" '(' ')' ';'  */
#line 366 "parser.y"
        { invalid_at((yylsp[-3]).first_line, "'assert' requires a condition"); (yyval.node) = NULL; }
#line 2252 "parser.tab.c"
    break;

  case 84: /* jump_stmt: "return" ';'  */
#line 370 "parser.y"
                            { (yyval.node) = mk(N_RETURN, (yylsp[-1]).first_line, NULL, NULL, NULL, NULL, NULL); }
#line 2258 "parser.tab.c"
    break;

  case 85: /* jump_stmt: "return" expr ';'  */
#line 371 "parser.y"
                            { (yyval.node) = mk(N_RETURN, (yylsp[-2]).first_line, NULL, (yyvsp[-1].node), NULL, NULL, NULL); }
#line 2264 "parser.tab.c"
    break;

  case 86: /* jump_stmt: "break" ';'  */
#line 372 "parser.y"
                            { (yyval.node) = mk(N_BREAK, (yylsp[-1]).first_line, NULL, NULL, NULL, NULL, NULL); }
#line 2270 "parser.tab.c"
    break;

  case 87: /* jump_stmt: "continue" ';'  */
#line 373 "parser.y"
                            { (yyval.node) = mk(N_CONTINUE, (yylsp[-1]).first_line, NULL, NULL, NULL, NULL, NULL); }
#line 2276 "parser.tab.c"
    break;

  case 88: /* jump_stmt: "throw" expr ';'  */
#line 374 "parser.y"
                            { (yyval.node) = mk(N_THROW, (yylsp[-2]).first_line, NULL, (yyvsp[-1].node), NULL, NULL, NULL); }
#line 2282 "parser.tab.c"
    break;

  case 89: /* print_stmt: "print" '(' expr_list_opt ')' ';'  */
#line 379 "parser.y"
        { (yyval.node) = mk(N_PRINT, (yylsp[-4]).first_line, NULL, (yyvsp[-2].node), NULL, NULL, NULL); }
#line 2288 "parser.tab.c"
    break;

  case 90: /* invalid_stmt: "else" block  */
#line 385 "parser.y"
        { invalid_at((yylsp[-1]).first_line, "'else' without a matching 'if'"); (yyval.node) = NULL; }
#line 2294 "parser.tab.c"
    break;

  case 91: /* invalid_stmt: "else" if_stmt  */
#line 387 "parser.y"
        { invalid_at((yylsp[-1]).first_line, "'else' without a matching 'if'"); (yyval.node) = NULL; }
#line 2300 "parser.tab.c"
    break;

  case 92: /* invalid_stmt: "case" expr ':' block  */
#line 389 "parser.y"
        { invalid_at((yylsp[-3]).first_line, "'case' outside of a 'switch'"); (yyval.node) = NULL; }
#line 2306 "parser.tab.c"
    break;

  case 93: /* invalid_stmt: "default" ':' block  */
#line 391 "parser.y"
        { invalid_at((yylsp[-2]).first_line, "'default' outside of a 'switch'"); (yyval.node) = NULL; }
#line 2312 "parser.tab.c"
    break;

  case 94: /* invalid_stmt: "catch" '(' "identifier" ')' block  */
#line 393 "parser.y"
        { invalid_at((yylsp[-4]).first_line, "'catch' without a preceding 'try'"); free((yyvsp[-2].str)); (yyval.node) = NULL; }
#line 2318 "parser.tab.c"
    break;

  case 97: /* or_expr: or_expr "||" and_expr  */
#line 407 "parser.y"
                                    { (yyval.node) = binary(OP_OR, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2324 "parser.tab.c"
    break;

  case 99: /* and_expr: and_expr "&&" eq_expr  */
#line 412 "parser.y"
                                    { (yyval.node) = binary(OP_AND, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2330 "parser.tab.c"
    break;

  case 101: /* eq_expr: eq_expr "==" rel_expr  */
#line 417 "parser.y"
                                    { (yyval.node) = binary(OP_EQ, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2336 "parser.tab.c"
    break;

  case 102: /* eq_expr: eq_expr "!=" rel_expr  */
#line 418 "parser.y"
                                    { (yyval.node) = binary(OP_NE, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2342 "parser.tab.c"
    break;

  case 104: /* rel_expr: rel_expr '<' add_expr  */
#line 423 "parser.y"
                                    { (yyval.node) = binary(OP_LT, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2348 "parser.tab.c"
    break;

  case 105: /* rel_expr: rel_expr '>' add_expr  */
#line 424 "parser.y"
                                    { (yyval.node) = binary(OP_GT, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2354 "parser.tab.c"
    break;

  case 106: /* rel_expr: rel_expr "<=" add_expr  */
#line 425 "parser.y"
                                    { (yyval.node) = binary(OP_LE, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2360 "parser.tab.c"
    break;

  case 107: /* rel_expr: rel_expr ">=" add_expr  */
#line 426 "parser.y"
                                    { (yyval.node) = binary(OP_GE, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2366 "parser.tab.c"
    break;

  case 109: /* add_expr: add_expr '+' mul_expr  */
#line 431 "parser.y"
                                    { (yyval.node) = binary(OP_ADD, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2372 "parser.tab.c"
    break;

  case 110: /* add_expr: add_expr '-' mul_expr  */
#line 432 "parser.y"
                                    { (yyval.node) = binary(OP_SUB, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2378 "parser.tab.c"
    break;

  case 112: /* mul_expr: mul_expr '*' unary  */
#line 437 "parser.y"
                                    { (yyval.node) = binary(OP_MUL, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2384 "parser.tab.c"
    break;

  case 113: /* mul_expr: mul_expr '/' unary  */
#line 438 "parser.y"
                                    { (yyval.node) = binary(OP_DIV, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2390 "parser.tab.c"
    break;

  case 114: /* mul_expr: mul_expr '%' unary  */
#line 439 "parser.y"
                                    { (yyval.node) = binary(OP_MOD, (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2396 "parser.tab.c"
    break;

  case 116: /* unary: '-' unary  */
#line 444 "parser.y"
                                    { (yyval.node) = unary(OP_NEG, (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2402 "parser.tab.c"
    break;

  case 117: /* unary: '!' unary  */
#line 445 "parser.y"
                                    { (yyval.node) = unary(OP_NOT, (yyvsp[0].node), (yylsp[-1]).first_line); }
#line 2408 "parser.tab.c"
    break;

  case 119: /* postfix: postfix '[' expr ']'  */
#line 450 "parser.y"
                                    { (yyval.node) = mk(N_INDEX, (yylsp[-2]).first_line, NULL, (yyvsp[-3].node), (yyvsp[-1].node), NULL, NULL); }
#line 2414 "parser.tab.c"
    break;

  case 120: /* primary: "integer literal"  */
#line 454 "parser.y"
                                    { (yyval.node) = mk(N_INT, (yylsp[0]).first_line, (yyvsp[0].str), NULL, NULL, NULL, NULL); }
#line 2420 "parser.tab.c"
    break;

  case 121: /* primary: "float literal"  */
#line 455 "parser.y"
                                    { (yyval.node) = mk(N_FLOAT, (yylsp[0]).first_line, (yyvsp[0].str), NULL, NULL, NULL, NULL); }
#line 2426 "parser.tab.c"
    break;

  case 122: /* primary: "string literal"  */
#line 456 "parser.y"
                                    { (yyval.node) = mk(N_STRING, (yylsp[0]).first_line, (yyvsp[0].str), NULL, NULL, NULL, NULL); }
#line 2432 "parser.tab.c"
    break;

  case 123: /* primary: "boolean literal"  */
#line 457 "parser.y"
                                    { (yyval.node) = mk(N_BOOL, (yylsp[0]).first_line, (yyvsp[0].str), NULL, NULL, NULL, NULL); }
#line 2438 "parser.tab.c"
    break;

  case 124: /* primary: "identifier"  */
#line 458 "parser.y"
                                    { (yyval.node) = mk(N_VAR, (yylsp[0]).first_line, (yyvsp[0].str), NULL, NULL, NULL, NULL); }
#line 2444 "parser.tab.c"
    break;

  case 125: /* primary: "identifier" '(' args_opt ')'  */
#line 459 "parser.y"
                                    { (yyval.node) = mk(N_CALL, (yylsp[-3]).first_line, (yyvsp[-3].str), (yyvsp[-1].node), NULL, NULL, NULL); }
#line 2450 "parser.tab.c"
    break;

  case 126: /* primary: reduction '(' expr ')'  */
#line 461 "parser.y"
        { (yyval.node) = mk(N_REDUCE, (yylsp[-3]).first_line, NULL, (yyvsp[-1].node), NULL, NULL, NULL); (yyval.node)->op = (yyvsp[-3].op); }
#line 2456 "parser.tab.c"
    break;

  case 127: /* primary: '(' expr ')'  */
#line 462 "parser.y"
                                    { (yyval.node) = (yyvsp[-1].node); }
#line 2462 "parser.tab.c"
    break;

  case 128: /* primary: '[' ']'  */
#line 463 "parser.y"
                                    { (yyval.node) = mk(N_ARRAY, (yylsp[-1]).first_line, NULL, NULL, NULL, NULL, NULL); }
#line 2468 "parser.tab.c"
    break;

  case 129: /* primary: '[' expr_list ']'  */
#line 464 "parser.y"
                                    { (yyval.node) = mk(N_ARRAY, (yylsp[-2]).first_line, NULL, (yyvsp[-1].node), NULL, NULL, NULL); }
#line 2474 "parser.tab.c"
    break;

  case 130: /* reduction: "sum"  */
#line 468 "parser.y"
                { (yyval.op) = OP_SUM; }
#line 2480 "parser.tab.c"
    break;

  case 131: /* reduction: "prod"  */
#line 469 "parser.y"
                { (yyval.op) = OP_PROD; }
#line 2486 "parser.tab.c"
    break;

  case 132: /* reduction: "max"  */
#line 470 "parser.y"
                { (yyval.op) = OP_MAX; }
#line 2492 "parser.tab.c"
    break;

  case 133: /* reduction: "min"  */
#line 471 "parser.y"
                { (yyval.op) = OP_MIN; }
#line 2498 "parser.tab.c"
    break;

  case 134: /* reduction: "len"  */
#line 472 "parser.y"
                { (yyval.op) = OP_LEN; }
#line 2504 "parser.tab.c"
    break;

  case 135: /* expr_list_opt: %empty  */
#line 476 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2510 "parser.tab.c"
    break;

  case 138: /* expr_list: expr_list ',' expr  */
#line 482 "parser.y"
                                    { (yyval.node) = list_append((yyvsp[-2].node), (yyvsp[0].node)); }
#line 2516 "parser.tab.c"
    break;

  case 139: /* args_opt: %empty  */
#line 487 "parser.y"
                                    { (yyval.node) = NULL; }
#line 2522 "parser.tab.c"
    break;

  case 142: /* arg_list: arg_list ',' arg  */
#line 494 "parser.y"
        { if (has_named((yyvsp[-2].node)) && (yyvsp[0].node)->kind != N_NAMED)
              invalid_at((yylsp[0]).first_line, "positional argument after a named argument");
          (yyval.node) = list_append((yyvsp[-2].node), (yyvsp[0].node)); }
#line 2530 "parser.tab.c"
    break;

  case 144: /* arg: "identifier" '=' expr  */
#line 502 "parser.y"
        { (yyval.node) = mk(N_NAMED, (yylsp[-2]).first_line, (yyvsp[-2].str), (yyvsp[0].node), NULL, NULL, NULL); }
#line 2536 "parser.tab.c"
    break;


#line 2540 "parser.tab.c"

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
  *++yylsp = yyloc;

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
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
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
                      yytoken, &yylval, &yylloc);
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

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

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
                  yytoken, &yylval, &yylloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 505 "parser.y"


/* Bison la llama al detectar un error de sintaxis. Solo guarda el mensaje y
 * la línea; lo imprime la regla que recupere el análisis. */
void yyerror(const char *s) {
    flush_pending();
    snprintf(pending_msg, sizeof pending_msg, "%s", s);
    pending_line = yylloc.first_line;
    has_pending = 1;
}
