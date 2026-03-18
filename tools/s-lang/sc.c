/* sc.c — S Language Transpiler (S → C)
 *
 * S is a simplified, expressive dialect of C for PhoenixOS development.
 * It reduces boilerplate while compiling directly to portable C99/C11.
 *
 * Usage:
 *   sc source.s              transpile source.s → source.c then compile
 *   sc source.s -o output    transpile and compile to binary 'output'
 *   sc source.s --emit-c     transpile only, print C to stdout
 *   sc source.s -C out.c     transpile only, write C to out.c
 *   sc -h | --help           show this help
 *   sc --version             show version
 *
 * S language features over C:
 *   use <header>             → #include <header>
 *   use "header"             → #include "header"
 *   fn name(params) -> T {}  → T name(params) {}
 *   fn name(params) {}       → void name(params) {}
 *   let T name = expr;       → const T name = expr;
 *   var T name = expr;       → T name = expr;
 *   null                     → NULL
 *   true / false             → true / false  (via <stdbool.h>)
 *   loop { }                 → while (1) { }
 *   for i in a..b { }        → for (int i = a; i < b; i++) { }
 *   for i in a..=b { }       → for (int i = a; i <= b; i++) { }
 *   Type aliases:
 *     str  → char*
 *     byte → uint8_t
 *     i8  i16  i32  i64  → int8_t  int16_t  int32_t  int64_t
 *     u8  u16  u32  u64  → uint8_t uint16_t uint32_t uint64_t
 *     f32 f64             → float   double
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

/* ── Version ──────────────────────────────────────────────────────────────── */

#define SC_VERSION "0.1.0"

/* ── Token types ──────────────────────────────────────────────────────────── */

typedef enum {
    TOK_EOF = 0,
    TOK_WORD,           /* identifier / keyword */
    TOK_NUMBER,         /* integer or float literal */
    TOK_STRING,         /* "..." */
    TOK_CHAR_LIT,       /* '...' */
    TOK_PUNCT,          /* single char: { } ( ) [ ] ; , . + - * / % & | ^ ~ ! = < > ? : */
    TOK_NEWLINE,        /* \n */
    TOK_SPACE,          /* whitespace (non-newline) */
    TOK_LINE_COMMENT,   /* // ... */
    TOK_BLOCK_COMMENT,  /* block comment */
    TOK_DOTDOT,         /* .. */
    TOK_DOTDOTEQ,       /* ..= */
    TOK_ARROW,          /* -> */
    TOK_EQ2,            /* == */
    TOK_NEQ,            /* != */
    TOK_LEQ,            /* <= */
    TOK_GEQ,            /* >= */
    TOK_AND2,           /* && */
    TOK_OR2,            /* || */
    TOK_SHL,            /* << */
    TOK_SHR,            /* >> */
    TOK_PLUSEQ,         /* += */
    TOK_MINUSEQ,        /* -= */
    TOK_STAREQ,         /* *= */
    TOK_SLASHEQ,        /* /= */
    TOK_PERCENTEQ,      /* %= */
    TOK_ANDEQ,          /* &= */
    TOK_OREQ,           /* |= */
    TOK_XOREQ,          /* ^= */
    TOK_INC,            /* ++ */
    TOK_DEC,            /* -- */
} TokType;

/* ── Token ────────────────────────────────────────────────────────────────── */

typedef struct {
    TokType  type;
    char    *text;      /* heap-allocated token text */
    int      line;
    int      col;
} Token;

/* ── Lexer state ──────────────────────────────────────────────────────────── */

typedef struct {
    const char *src;
    int         pos;
    int         len;
    int         line;
    int         col;
    const char *filename;
} Lexer;

/* ── Translator state ─────────────────────────────────────────────────────── */

typedef struct {
    Token  *tokens;
    int     count;
    int     cap;
    int     pos;        /* current position in tokens[] */
} Translator;

/* ── Output buffer ────────────────────────────────────────────────────────── */

typedef struct {
    char *buf;
    int   len;
    int   cap;
} OutBuf;

/* ══════════════════════════════════════════════════════════════════════════ */
/* Utilities                                                                   */
/* ══════════════════════════════════════════════════════════════════════════ */

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "sc: error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static char *xstrdup(const char *s) {
    size_t len = strlen(s);
    char *p = malloc(len + 1);
    if (!p) die("out of memory");
    memcpy(p, s, len + 1);
    return p;
}

static char *xstrndup(const char *s, int n) {
    char *p = malloc(n + 1);
    if (!p) die("out of memory");
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* OutBuf                                                                       */
/* ══════════════════════════════════════════════════════════════════════════ */

static void ob_init(OutBuf *ob) {
    ob->cap = 4096;
    ob->buf = malloc(ob->cap);
    if (!ob->buf) die("out of memory");
    ob->len = 0;
}

static void ob_grow(OutBuf *ob, int need) {
    while (ob->len + need >= ob->cap) {
        ob->cap *= 2;
        ob->buf = realloc(ob->buf, ob->cap);
        if (!ob->buf) die("out of memory");
    }
}

static void ob_append(OutBuf *ob, const char *s, int n) {
    ob_grow(ob, n + 1);
    memcpy(ob->buf + ob->len, s, n);
    ob->len += n;
    ob->buf[ob->len] = '\0';
}

static void ob_str(OutBuf *ob, const char *s) {
    ob_append(ob, s, (int)strlen(s));
}

static void ob_char(OutBuf *ob, char c) {
    ob_grow(ob, 2);
    ob->buf[ob->len++] = c;
    ob->buf[ob->len] = '\0';
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Lexer                                                                        */
/* ══════════════════════════════════════════════════════════════════════════ */

static char lexer_peek(Lexer *l) {
    if (l->pos >= l->len) return '\0';
    return l->src[l->pos];
}

static char lexer_peek2(Lexer *l) {
    if (l->pos + 1 >= l->len) return '\0';
    return l->src[l->pos + 1];
}

static char lexer_advance(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') { l->line++; l->col = 1; }
    else           { l->col++; }
    return c;
}

static Token make_tok(TokType t, const char *text, int line, int col) {
    Token tok;
    tok.type = t;
    tok.text = xstrdup(text);
    tok.line = line;
    tok.col  = col;
    return tok;
}

/* Lex one token; returns 0 at EOF */
static int lexer_next(Lexer *l, Token *out) {
    if (l->pos >= l->len) {
        *out = make_tok(TOK_EOF, "", l->line, l->col);
        return 0;
    }

    int line = l->line, col = l->col;
    char c = lexer_peek(l);

    /* ── Whitespace (non-newline) ────────────────────────────────────────── */
    if (c == ' ' || c == '\t' || c == '\r') {
        int start = l->pos;
        while (l->pos < l->len) {
            char ch = lexer_peek(l);
            if (ch != ' ' && ch != '\t' && ch != '\r') break;
            lexer_advance(l);
        }
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_SPACE, text, line, col);
        free(text);
        return 1;
    }

    /* ── Newline ─────────────────────────────────────────────────────────── */
    if (c == '\n') {
        lexer_advance(l);
        *out = make_tok(TOK_NEWLINE, "\n", line, col);
        return 1;
    }

    /* ── Line comment ────────────────────────────────────────────────────── */
    if (c == '/' && lexer_peek2(l) == '/') {
        int start = l->pos;
        while (l->pos < l->len && lexer_peek(l) != '\n')
            lexer_advance(l);
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_LINE_COMMENT, text, line, col);
        free(text);
        return 1;
    }

    /* ── Block comment ───────────────────────────────────────────────────── */
    if (c == '/' && lexer_peek2(l) == '*') {
        int start = l->pos;
        lexer_advance(l); lexer_advance(l); /* consume slash-star */
        while (l->pos + 1 < l->len) {
            if (lexer_peek(l) == '*' && lexer_peek2(l) == '/') {
                lexer_advance(l); lexer_advance(l);
                break;
            }
            lexer_advance(l);
        }
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_BLOCK_COMMENT, text, line, col);
        free(text);
        return 1;
    }

    /* ── String literal ──────────────────────────────────────────────────── */
    if (c == '"') {
        int start = l->pos;
        lexer_advance(l); /* opening " */
        while (l->pos < l->len) {
            char ch = lexer_advance(l);
            if (ch == '\\') { if (l->pos < l->len) lexer_advance(l); }
            else if (ch == '"') break;
        }
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_STRING, text, line, col);
        free(text);
        return 1;
    }

    /* ── Char literal ────────────────────────────────────────────────────── */
    if (c == '\'') {
        int start = l->pos;
        lexer_advance(l);
        while (l->pos < l->len) {
            char ch = lexer_advance(l);
            if (ch == '\\') { if (l->pos < l->len) lexer_advance(l); }
            else if (ch == '\'') break;
        }
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_CHAR_LIT, text, line, col);
        free(text);
        return 1;
    }

    /* ── Number ──────────────────────────────────────────────────────────── */
    if (isdigit((unsigned char)c) ||
        (c == '0' && (lexer_peek2(l) == 'x' || lexer_peek2(l) == 'X' ||
                      lexer_peek2(l) == 'b' || lexer_peek2(l) == 'B'))) {
        int start = l->pos;
        /* hex */
        if (c == '0' && (lexer_peek2(l) == 'x' || lexer_peek2(l) == 'X')) {
            lexer_advance(l); lexer_advance(l);
            while (l->pos < l->len && isxdigit((unsigned char)lexer_peek(l)))
                lexer_advance(l);
        } else {
            /* Consume digits; allow at most ONE '.' for float literals.
             * Stop at '..' so range operators (.. and ..=) tokenise correctly. */
            int seen_dot = 0;
            while (l->pos < l->len) {
                char ch = lexer_peek(l);
                if (isdigit((unsigned char)ch)) {
                    lexer_advance(l);
                } else if (ch == '.' && !seen_dot && lexer_peek2(l) != '.') {
                    /* Single dot followed by a digit → float literal */
                    if (l->pos + 1 < l->len && isdigit((unsigned char)l->src[l->pos + 1])) {
                        seen_dot = 1;
                        lexer_advance(l);
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            /* suffix: u, l, ul, ull, f */
            while (l->pos < l->len &&
                   (lexer_peek(l) == 'u' || lexer_peek(l) == 'U' ||
                    lexer_peek(l) == 'l' || lexer_peek(l) == 'L' ||
                    lexer_peek(l) == 'f' || lexer_peek(l) == 'F'))
                lexer_advance(l);
        }
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_NUMBER, text, line, col);
        free(text);
        return 1;
    }

    /* ── Identifier / keyword ────────────────────────────────────────────── */
    if (isalpha((unsigned char)c) || c == '_') {
        int start = l->pos;
        while (l->pos < l->len &&
               (isalnum((unsigned char)lexer_peek(l)) || lexer_peek(l) == '_'))
            lexer_advance(l);
        char *text = xstrndup(l->src + start, l->pos - start);
        *out = make_tok(TOK_WORD, text, line, col);
        free(text);
        return 1;
    }

    /* ── Multi-char operators ────────────────────────────────────────────── */
    lexer_advance(l); /* consume first char */
    char c2 = lexer_peek(l);

    /* ..= and .. */
    if (c == '.' && c2 == '.') {
        lexer_advance(l);
        if (lexer_peek(l) == '=') {
            lexer_advance(l);
            *out = make_tok(TOK_DOTDOTEQ, "..=", line, col);
        } else {
            *out = make_tok(TOK_DOTDOT, "..", line, col);
        }
        return 1;
    }
    /* -> */
    if (c == '-' && c2 == '>') {
        lexer_advance(l);
        *out = make_tok(TOK_ARROW, "->", line, col);
        return 1;
    }
    /* == */
    if (c == '=' && c2 == '=') {
        lexer_advance(l);
        *out = make_tok(TOK_EQ2, "==", line, col);
        return 1;
    }
    /* != */
    if (c == '!' && c2 == '=') {
        lexer_advance(l);
        *out = make_tok(TOK_NEQ, "!=", line, col);
        return 1;
    }
    /* <= */
    if (c == '<' && c2 == '=') {
        lexer_advance(l);
        *out = make_tok(TOK_LEQ, "<=", line, col);
        return 1;
    }
    /* >= */
    if (c == '>' && c2 == '=') {
        lexer_advance(l);
        *out = make_tok(TOK_GEQ, ">=", line, col);
        return 1;
    }
    /* && */
    if (c == '&' && c2 == '&') {
        lexer_advance(l);
        *out = make_tok(TOK_AND2, "&&", line, col);
        return 1;
    }
    /* || */
    if (c == '|' && c2 == '|') {
        lexer_advance(l);
        *out = make_tok(TOK_OR2, "||", line, col);
        return 1;
    }
    /* << */
    if (c == '<' && c2 == '<') {
        lexer_advance(l);
        *out = make_tok(TOK_SHL, "<<", line, col);
        return 1;
    }
    /* >> */
    if (c == '>' && c2 == '>') {
        lexer_advance(l);
        *out = make_tok(TOK_SHR, ">>", line, col);
        return 1;
    }
    /* += -= *= /= %= &= |= ^= */
    if (c2 == '=') {
        char buf[3] = { c, '=', '\0' };
        TokType t2 = TOK_PUNCT;
        switch (c) {
            case '+': t2 = TOK_PLUSEQ;    break;
            case '-': t2 = TOK_MINUSEQ;   break;
            case '*': t2 = TOK_STAREQ;    break;
            case '/': t2 = TOK_SLASHEQ;   break;
            case '%': t2 = TOK_PERCENTEQ; break;
            case '&': t2 = TOK_ANDEQ;     break;
            case '|': t2 = TOK_OREQ;      break;
            case '^': t2 = TOK_XOREQ;     break;
            default: break;
        }
        if (t2 != TOK_PUNCT) {
            lexer_advance(l);
            *out = make_tok(t2, buf, line, col);
            return 1;
        }
    }
    /* ++ -- */
    if (c == '+' && c2 == '+') { lexer_advance(l); *out = make_tok(TOK_INC, "++", line, col); return 1; }
    if (c == '-' && c2 == '-') { lexer_advance(l); *out = make_tok(TOK_DEC, "--", line, col); return 1; }

    /* ── Single-char punct ───────────────────────────────────────────────── */
    char buf2[2] = { c, '\0' };
    *out = make_tok(TOK_PUNCT, buf2, line, col);
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Token array helpers                                                          */
/* ══════════════════════════════════════════════════════════════════════════ */

static void tr_push(Translator *tr, Token tok) {
    if (tr->count >= tr->cap) {
        tr->cap = tr->cap ? tr->cap * 2 : 256;
        tr->tokens = realloc(tr->tokens, tr->cap * sizeof(Token));
        if (!tr->tokens) die("out of memory");
    }
    tr->tokens[tr->count++] = tok;
}

/* Peek at current token (skip whitespace/newlines optionally) */
static Token tr_peek(Translator *tr) {
    if (tr->pos >= tr->count) return make_tok(TOK_EOF, "", 0, 0);
    return tr->tokens[tr->pos];
}

static Token tr_consume(Translator *tr) {
    if (tr->pos >= tr->count) return make_tok(TOK_EOF, "", 0, 0);
    return tr->tokens[tr->pos++];
}

/* ── Check if a word token matches string ──────────────────────────────── */
static int tok_is(Token t, const char *s) {
    return t.type == TOK_WORD && strcmp(t.text, s) == 0;
}
static int tok_is_punct(Token t, char c) {
    return t.type == TOK_PUNCT && t.text[0] == c;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Type alias translation                                                       */
/* ══════════════════════════════════════════════════════════════════════════ */

static const char *translate_type(const char *word) {
    if (strcmp(word, "str")  == 0) return "char*";
    if (strcmp(word, "byte") == 0) return "uint8_t";
    if (strcmp(word, "i8")   == 0) return "int8_t";
    if (strcmp(word, "i16")  == 0) return "int16_t";
    if (strcmp(word, "i32")  == 0) return "int32_t";
    if (strcmp(word, "i64")  == 0) return "int64_t";
    if (strcmp(word, "u8")   == 0) return "uint8_t";
    if (strcmp(word, "u16")  == 0) return "uint16_t";
    if (strcmp(word, "u32")  == 0) return "uint32_t";
    if (strcmp(word, "u64")  == 0) return "uint64_t";
    if (strcmp(word, "f32")  == 0) return "float";
    if (strcmp(word, "f64")  == 0) return "double";
    if (strcmp(word, "usize")== 0) return "size_t";
    if (strcmp(word, "isize")== 0) return "ssize_t";
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Main translation pass                                                        */
/* ══════════════════════════════════════════════════════════════════════════ */

/* Forward declaration */
static void translate_tokens(Translator *tr, OutBuf *ob);

/* Translate a 'fn' declaration:
 *   fn name ( params ) -> RetType { body }
 *   fn name ( params )            { body }
 * Emits:
 *   RetType name ( params ) { ... }  (or void if no return type)              */
static void translate_fn(Translator *tr, OutBuf *ob) {
    /* consume 'fn' */
    tr->pos++;

    /* skip whitespace */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) {
        tr->pos++;
    }

    /* function name */
    Token name_tok = tr_consume(tr);
    if (name_tok.type != TOK_WORD) {
        /* malformed, emit as-is */
        ob_str(ob, "fn ");
        ob_str(ob, name_tok.text);
        return;
    }

    /* skip whitespace */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* params */
    if (!tok_is_punct(tr_peek(tr), '(')) {
        /* no parens — emit fallback */
        ob_str(ob, "void ");
        ob_str(ob, name_tok.text);
        return;
    }

    /* Collect params as raw string for now — we re-translate types inside */
    /* Save position before params to re-process */
    OutBuf params_ob;
    ob_init(&params_ob);

    ob_char(&params_ob, '(');
    tr->pos++; /* consume '(' */
    int depth = 1;
    while (tr->pos < tr->count && depth > 0) {
        Token t = tr_peek(tr);
        if (t.type == TOK_EOF) break;
        if (tok_is_punct(t, '(')) { depth++; ob_str(&params_ob, t.text); tr->pos++; continue; }
        if (tok_is_punct(t, ')')) {
            depth--;
            if (depth == 0) { ob_char(&params_ob, ')'); tr->pos++; break; }
            ob_str(&params_ob, t.text); tr->pos++; continue;
        }
        if (t.type == TOK_WORD) {
            const char *mapped = translate_type(t.text);
            if (mapped) {
                ob_str(&params_ob, mapped);
                tr->pos++;
                continue;
            }
        }
        ob_str(&params_ob, t.text);
        tr->pos++;
    }

    /* skip whitespace */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* check for '->' return type */
    const char *ret_type = "void";
    char *ret_buf = NULL;

    if (tr_peek(tr).type == TOK_ARROW) {
        tr->pos++; /* consume '->' */
        /* skip whitespace */
        while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

        /* collect return type tokens until '{' or newline (ignoring ws) */
        OutBuf rtob;
        ob_init(&rtob);
        while (tr->pos < tr->count) {
            Token t = tr_peek(tr);
            if (t.type == TOK_EOF || tok_is_punct(t, '{') || tok_is_punct(t, ';'))
                break;
            if (t.type == TOK_NEWLINE) break;
            if (t.type == TOK_WORD) {
                const char *mapped = translate_type(t.text);
                if (mapped) {
                    ob_str(&rtob, mapped);
                    tr->pos++;
                    continue;
                }
            }
            ob_str(&rtob, t.text);
            tr->pos++;
        }
        /* trim trailing whitespace from return type */
        while (rtob.len > 0 && (rtob.buf[rtob.len-1] == ' ' || rtob.buf[rtob.len-1] == '\t'))
            rtob.buf[--rtob.len] = '\0';
        ret_buf = rtob.buf;
        ret_type = rtob.buf;
    }

    /* Emit: RetType name(params) */
    ob_str(ob, ret_type);
    ob_char(ob, ' ');
    ob_str(ob, name_tok.text);
    ob_str(ob, params_ob.buf);

    free(params_ob.buf);
    if (ret_buf) free(ret_buf);
}

/* Translate a 'use' directive:
 *   use <header>   → #include <header>
 *   use "header"   → #include "header"                                        */
static void translate_use(Translator *tr, OutBuf *ob) {
    tr->pos++; /* consume 'use' */

    /* skip spaces (not newlines) */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    Token t = tr_peek(tr);

    if (t.type == TOK_STRING) {
        ob_str(ob, "#include ");
        ob_str(ob, t.text);
        tr->pos++;
    } else if (tok_is_punct(t, '<')) {
        /* collect until '>' */
        ob_str(ob, "#include ");
        int start = tr->pos;
        (void)start;
        ob_str(ob, t.text); /* '<' */
        tr->pos++;
        while (tr->pos < tr->count) {
            Token tt = tr_peek(tr);
            ob_str(ob, tt.text);
            tr->pos++;
            if (tok_is_punct(tt, '>')) break;
            if (tt.type == TOK_NEWLINE || tt.type == TOK_EOF) break;
        }
    } else {
        /* fallback */
        ob_str(ob, "/* use ");
        ob_str(ob, t.text);
        ob_str(ob, " */");
        tr->pos++;
    }
}

/* Translate 'let T name = expr;' or 'let name: T = expr;' → 'const T name = expr;' */
static void translate_let(Translator *tr, OutBuf *ob) {
    tr->pos++; /* consume 'let' */
    /* skip spaces */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* Check if next is a type or a name followed by ':' */
    /* Strategy: collect tokens, detect pattern */
    Token t1 = tr_peek(tr);

    /* If t1 is a known type alias, emit "const <type>" */
    if (t1.type == TOK_WORD) {
        const char *mapped = translate_type(t1.text);
        if (mapped) {
            ob_str(ob, "const ");
            ob_str(ob, mapped);
            tr->pos++;
            return; /* rest is emitted normally */
        }
        /* Check if next-after-spaces is ':' (name: type syntax) */
        int saved = tr->pos;
        tr->pos++; /* skip name */
        while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;
        Token colon = tr_peek(tr);
        if (tok_is_punct(colon, ':')) {
            tr->pos++; /* consume ':' */
            while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;
            Token type_tok = tr_peek(tr);
            const char *mapped2 = (type_tok.type == TOK_WORD) ? translate_type(type_tok.text) : NULL;
            if (mapped2) {
                ob_str(ob, "const ");
                ob_str(ob, mapped2);
                ob_char(ob, ' ');
                ob_str(ob, t1.text); /* variable name */
                tr->pos++; /* consume type token */
                return;
            } else {
                /* unknown type — emit as-is with const prefix */
                ob_str(ob, "const ");
                if (type_tok.type != TOK_EOF) {
                    ob_str(ob, type_tok.text);
                    tr->pos++;
                }
                ob_char(ob, ' ');
                ob_str(ob, t1.text);
                return;
            }
        }
        /* No colon — treat name as the type specifier */
        tr->pos = saved;
        ob_str(ob, "const ");
        /* emit original word as type (might be a custom type like 'int', 'char', etc.) */
        ob_str(ob, t1.text);
        tr->pos++;
    } else {
        ob_str(ob, "const ");
    }
}

/* Translate 'var T name = expr;' or 'var name: T = expr;' → 'T name = expr;' */
static void translate_var(Translator *tr, OutBuf *ob) {
    tr->pos++; /* consume 'var' */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    Token t1 = tr_peek(tr);
    if (t1.type == TOK_WORD) {
        const char *mapped = translate_type(t1.text);
        if (mapped) {
            ob_str(ob, mapped);
            tr->pos++;
            return;
        }
        /* name: type syntax */
        int saved = tr->pos;
        tr->pos++;
        while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;
        Token colon = tr_peek(tr);
        if (tok_is_punct(colon, ':')) {
            tr->pos++;
            while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;
            Token type_tok = tr_peek(tr);
            const char *mapped2 = (type_tok.type == TOK_WORD) ? translate_type(type_tok.text) : NULL;
            if (mapped2) {
                ob_str(ob, mapped2);
                ob_char(ob, ' ');
                ob_str(ob, t1.text);
                tr->pos++;
                return;
            } else {
                if (type_tok.type != TOK_EOF) { ob_str(ob, type_tok.text); tr->pos++; }
                ob_char(ob, ' ');
                ob_str(ob, t1.text);
                return;
            }
        }
        tr->pos = saved;
        /* emit as plain type */
        ob_str(ob, t1.text);
        tr->pos++;
    }
}

/* Translate 'for i in start..end { }' or 'for i in start..=end { }'         */
static int try_translate_for_range(Translator *tr, OutBuf *ob) {
    /* We've already seen 'for'. Try to detect pattern:
     *   for WORD in expr .. [=] expr {
     * Save position in case this isn't a range-for.                           */
    int saved = tr->pos;
    tr->pos++; /* consume 'for' */

    /* skip spaces */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* loop variable */
    Token var_tok = tr_peek(tr);
    if (var_tok.type != TOK_WORD) { tr->pos = saved; return 0; }
    tr->pos++;

    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* 'in' keyword */
    Token in_tok = tr_peek(tr);
    if (!tok_is(in_tok, "in")) { tr->pos = saved; return 0; }
    tr->pos++;

    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* start expression — collect until '..' or '..=' */
    OutBuf start_ob;
    ob_init(&start_ob);
    int found_range = 0;
    int inclusive   = 0;

    while (tr->pos < tr->count) {
        Token t = tr_peek(tr);
        if (t.type == TOK_DOTDOTEQ) { found_range = 1; inclusive = 1; tr->pos++; break; }
        if (t.type == TOK_DOTDOT)   { found_range = 1; inclusive = 0; tr->pos++; break; }
        if (t.type == TOK_NEWLINE || t.type == TOK_EOF) break;
        ob_str(&start_ob, t.text);
        tr->pos++;
    }

    if (!found_range) { free(start_ob.buf); tr->pos = saved; return 0; }

    /* skip spaces after range op */
    while (tr->pos < tr->count && tr_peek(tr).type == TOK_SPACE) tr->pos++;

    /* end expression — collect until '{' or newline */
    OutBuf end_ob;
    ob_init(&end_ob);
    while (tr->pos < tr->count) {
        Token t = tr_peek(tr);
        if (tok_is_punct(t, '{') || t.type == TOK_NEWLINE || t.type == TOK_EOF) break;
        if (t.type == TOK_SPACE) { ob_str(&end_ob, t.text); tr->pos++; continue; }
        ob_str(&end_ob, t.text);
        tr->pos++;
    }

    /* trim trailing whitespace from end expression */
    while (end_ob.len > 0 && (end_ob.buf[end_ob.len-1] == ' ' ||
                               end_ob.buf[end_ob.len-1] == '\t'))
        end_ob.buf[--end_ob.len] = '\0';

    /* emit: for (int VAR = START; VAR < END; VAR++) */
    ob_str(ob, "for (int ");
    ob_str(ob, var_tok.text);
    ob_str(ob, " = ");
    ob_str(ob, start_ob.buf);
    ob_str(ob, "; ");
    ob_str(ob, var_tok.text);
    ob_str(ob, inclusive ? " <= " : " < ");
    ob_str(ob, end_ob.buf);
    ob_str(ob, "; ");
    ob_str(ob, var_tok.text);
    ob_str(ob, "++)");

    free(start_ob.buf);
    free(end_ob.buf);
    return 1;
}

/* ── Main token translation loop ──────────────────────────────────────────── */

static void translate_tokens(Translator *tr, OutBuf *ob) {
    while (tr->pos < tr->count) {
        Token t = tr_peek(tr);

        if (t.type == TOK_EOF) break;

        /* ── Pass-through: comments, strings, char literals, numbers ─────── */
        if (t.type == TOK_LINE_COMMENT || t.type == TOK_BLOCK_COMMENT ||
            t.type == TOK_STRING || t.type == TOK_CHAR_LIT ||
            t.type == TOK_NUMBER || t.type == TOK_SPACE || t.type == TOK_NEWLINE) {
            ob_str(ob, t.text);
            tr->pos++;
            continue;
        }

        /* ── Multi-char operators: pass through ──────────────────────────── */
        if (t.type != TOK_WORD && t.type != TOK_PUNCT) {
            ob_str(ob, t.text);
            tr->pos++;
            continue;
        }

        /* ── Single-char punct: pass through ─────────────────────────────── */
        if (t.type == TOK_PUNCT) {
            ob_str(ob, t.text);
            tr->pos++;
            continue;
        }

        /* ── Keywords and identifiers ─────────────────────────────────────── */

        /* use → #include */
        if (tok_is(t, "use")) {
            translate_use(tr, ob);
            continue;
        }

        /* fn → function declaration */
        if (tok_is(t, "fn")) {
            translate_fn(tr, ob);
            continue;
        }

        /* let → const declaration */
        if (tok_is(t, "let")) {
            translate_let(tr, ob);
            continue;
        }

        /* var → mutable declaration */
        if (tok_is(t, "var")) {
            translate_var(tr, ob);
            continue;
        }

        /* null → NULL */
        if (tok_is(t, "null")) {
            ob_str(ob, "NULL");
            tr->pos++;
            continue;
        }

        /* loop { → while (1) { */
        if (tok_is(t, "loop")) {
            ob_str(ob, "while (1)");
            tr->pos++;
            continue;
        }

        /* for ... in ... .. → range-for */
        if (tok_is(t, "for")) {
            if (!try_translate_for_range(tr, ob)) {
                /* not a range-for — emit as plain 'for' */
                ob_str(ob, t.text);
                tr->pos++;
            }
            continue;
        }

        /* Type aliases in expression/type contexts */
        if (t.type == TOK_WORD) {
            const char *mapped = translate_type(t.text);
            if (mapped) {
                ob_str(ob, mapped);
                tr->pos++;
                continue;
            }
        }

        ob_str(ob, t.text);
        tr->pos++;
    }
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* File I/O helpers                                                             */
/* ══════════════════════════════════════════════════════════════════════════ */

static char *read_file(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "sc: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); die("out of memory"); }
    long rd = (long)fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = (int)rd;
    return buf;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Transpile: S source → C source                                               */
/* ══════════════════════════════════════════════════════════════════════════ */

static char *transpile(const char *src, int src_len, const char *filename) {
    /* ── Lex ──────────────────────────────────────────────────────────────── */
    Lexer lex;
    lex.src      = src;
    lex.pos      = 0;
    lex.len      = src_len;
    lex.line     = 1;
    lex.col      = 1;
    lex.filename = filename;

    Translator tr;
    tr.tokens = NULL;
    tr.count  = 0;
    tr.cap    = 0;
    tr.pos    = 0;

    Token tok;
    while (lexer_next(&lex, &tok)) {
        tr_push(&tr, tok);
    }
    tr_push(&tr, make_tok(TOK_EOF, "", lex.line, lex.col));

    /* ── Detect if 'use <s.h>' or 'use "s.h"' is already present ─────────── */
    int has_s_include = 0;
    for (int i = 0; i < tr.count; i++) {
        if (tr.tokens[i].type == TOK_WORD && strcmp(tr.tokens[i].text, "use") == 0) {
            /* peek ahead for s.h */
            int j = i + 1;
            while (j < tr.count && tr.tokens[j].type == TOK_SPACE) j++;
            if (j < tr.count) {
                const char *txt = tr.tokens[j].text;
                if (strstr(txt, "s.h")) { has_s_include = 1; break; }
            }
        }
    }

    /* ── Translate ────────────────────────────────────────────────────────── */
    /* Reset flags */

    OutBuf body;
    ob_init(&body);
    translate_tokens(&tr, &body);

    /* ── Build output with header ─────────────────────────────────────────── */
    OutBuf out;
    ob_init(&out);

    ob_str(&out, "/* Generated by sc (S language compiler) v" SC_VERSION " — DO NOT EDIT */\n");
    ob_str(&out, "/* Source: ");
    ob_str(&out, filename);
    ob_str(&out, " */\n\n");

    if (!has_s_include) {
        ob_str(&out, "#include <s.h>\n");
    }

    ob_str(&out, body.buf);
    free(body.buf);

    /* free tokens */
    for (int i = 0; i < tr.count; i++) free(tr.tokens[i].text);
    free(tr.tokens);

    return out.buf;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Main                                                                         */
/* ══════════════════════════════════════════════════════════════════════════ */

static void usage(const char *prog) {
    fprintf(stderr,
        "S Language Compiler v" SC_VERSION "\n"
        "\n"
        "Usage: %s [options] <source.s>\n"
        "\n"
        "Options:\n"
        "  -o <output>    Compile to binary (default: strip .s extension)\n"
        "  -C <out.c>     Transpile only, write C to file\n"
        "  --emit-c       Transpile only, print C to stdout\n"
        "  --version      Print version and exit\n"
        "  -h, --help     Show this help\n"
        "\n"
        "Examples:\n"
        "  %s hello.s             Compile hello.s to ./hello\n"
        "  %s hello.s -o hello    Same, explicit output\n"
        "  %s hello.s --emit-c    Print generated C code\n"
        "  %s hello.s -C hello.c  Write generated C to hello.c\n",
        prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *input_path = NULL;
    const char *output_bin = NULL;
    const char *output_c   = NULL;
    int         emit_c     = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("sc " SC_VERSION "\n");
            return 0;
        }
        if (strcmp(argv[i], "--emit-c") == 0) {
            emit_c = 1;
            continue;
        }
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) die("-o requires an argument");
            output_bin = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-C") == 0) {
            if (++i >= argc) die("-C requires an argument");
            output_c = argv[i];
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "sc: unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
        input_path = argv[i];
    }

    if (!input_path) {
        fprintf(stderr, "sc: no input file\n");
        usage(argv[0]);
        return 1;
    }

    /* ── Read input ─────────────────────────────────────────────────────── */
    int src_len;
    char *src = read_file(input_path, &src_len);
    if (!src) return 1;

    /* ── Transpile ──────────────────────────────────────────────────────── */
    char *c_src = transpile(src, src_len, input_path);
    free(src);

    /* ── Output ─────────────────────────────────────────────────────────── */
    if (emit_c) {
        fputs(c_src, stdout);
        free(c_src);
        return 0;
    }

    if (output_c) {
        FILE *f = fopen(output_c, "w");
        if (!f) die("cannot write '%s': %s", output_c, strerror(errno));
        fputs(c_src, f);
        fclose(f);
        free(c_src);
        return 0;
    }

    /* ── Compile via gcc ────────────────────────────────────────────────── */
    /* Write to a temp .c file, then compile */
    char tmp_c[512];
    snprintf(tmp_c, sizeof(tmp_c), "/tmp/sc_%d_XXXXXX.c", (int)getpid());
    /* Use mkstemp-compatible approach */
    int tmp_fd = -1;
    {
        /* simple temp file: /tmp/sc_<pid>.c */
        snprintf(tmp_c, sizeof(tmp_c), "/tmp/sc_%d.c", (int)getpid());
        FILE *tf = fopen(tmp_c, "w");
        if (!tf) die("cannot create temp file '%s': %s", tmp_c, strerror(errno));
        fputs(c_src, tf);
        fclose(tf);
        (void)tmp_fd;
    }
    free(c_src);

    /* Determine output binary name */
    char bin_name[512];
    if (output_bin) {
        snprintf(bin_name, sizeof(bin_name), "%s", output_bin);
    } else {
        /* Strip .s extension */
        snprintf(bin_name, sizeof(bin_name), "%s", input_path);
        char *dot = strrchr(bin_name, '.');
        if (dot && strcmp(dot, ".s") == 0) *dot = '\0';
    }

    /* Build gcc command */
    /* Look for s.h in the same dir as sc, or in standard include paths */
    char sc_dir[512];
    snprintf(sc_dir, sizeof(sc_dir), "%s", argv[0]);
    char *slash = strrchr(sc_dir, '/');
    if (slash) { *slash = '\0'; } else { sc_dir[0] = '.'; sc_dir[1] = '\0'; }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "gcc -O2 -Wall -std=c11 -I\"%s/include\" -o \"%s\" \"%s\"",
             sc_dir, bin_name, tmp_c);

    int rc = system(cmd);
    remove(tmp_c);

    if (rc != 0) {
        fprintf(stderr, "sc: compilation failed\n");
        return 1;
    }

    return 0;
}
