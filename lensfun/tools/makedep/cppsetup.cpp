/* $XConsortium: cppsetup.c /main/17 1996/09/28 16:15:03 rws $ */
/*

   Copyright (c) 1993, 1994  X Consortium

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
   X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
   AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the X Consortium shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the X Consortium.

*/
/* $XFree86: xc/config/makedepend/cppsetup.c,v 3.2 1996/12/30 13:57:53 dawes Exp $ */

#include "def.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef	CPP
/*
 * This file is strictly for the sake of cpy.y and yylex.c (if
 * you indeed have the source for cpp).
 */
#define IB 1
#define SB 2
#define NB 4
#define CB 8
#define QB 16
#define WB 32
#define SALT '#'
#if defined(pdp11) || defined(vax) || defined(ns16000) || defined(mc68000) || defined(ibm032)
#define COFF 128
#else
#define COFF 0
#endif
/*
 * These variables used by cpy.y and yylex.c
 */
extern char *outp, *inp, *newp, *pend;
extern char *ptrtab;
extern char fastab[];
extern char slotab[];

/*
 * cppsetup
 */
struct filepointer *currentfile;
struct inclist *currentinc;

int cppsetup (char *line, struct filepointer *filep, struct inclist *inc)
{
    register char *p, savec;
    static bool setupdone = false;
    bool value;

    if (!setupdone)
    {
        cpp_varsetup ();
        setupdone = true;
    }

    currentfile = filep;
    currentinc = inc;
    inp = newp = line;
    for (p = newp; *p; p++)
        ;

    /* put a newline back on the end, and set up pend, etc. */
    *p++ = '\n';
    savec = *p;
    *p = '\0';
    pend = p;

    ptrtab = slotab + COFF;
    *--inp = SALT;
    outp = inp;
    value = yyparse ();
    *p = savec;
    return (value);
}

struct symtab **lookup (symbol)
char *symbol;
{
    static struct symtab *undefined;
    struct symtab **sp;

    sp = isdefined (symbol, currentinc, NULL);
    if (sp == NULL)
    {
        sp = &undefined;
        (*sp)->s_value = NULL;
    }
    return (sp);
}

pperror (int tag, int x0, int x1, int x2, int x3, int x4)
{
    warning ("\"%s\", line %d: ", currentinc->i_file, currentfile->f_line);
    warning (x0, x1, x2, x3, x4);
}

yyerror (char *s)
{
    fatalerr ("Fatal error: %s\n", s);
}

#else /* not CPP */

#include "ifparser.h"

struct _parse_data
{
    struct filepointer *filep;
    struct inclist *inc;
    const char *line;
};

static char *my_if_errors (IfParser *ip, const char *cp, const char *expecting)
{
    struct _parse_data *pd = (struct _parse_data *) ip->data;
    int lineno = pd->filep->f_line;
    char *filename = pd->inc->i_file;
    char prefix[300];
    int prefixlen;
    int i;

    sprintf (prefix, "\"%s\":%d", filename, lineno);
    prefixlen = strlen (prefix);
    fprintf (stderr, "%s:  %s", prefix, pd->line);
    i = cp - pd->line;
    if (i > 0 && pd->line[i - 1] != '\n')
        putc ('\n', stderr);
    for (i += prefixlen + 3; i > 0; i--)
        putc (' ', stderr);
    fprintf (stderr, "^--- expecting %s\n", expecting);
    return NULL;
}

#define MAXNAMELEN 256

static struct symtab **lookup_variable (IfParser *ip, const char *var, int len)
{
    char tmpbuf[MAXNAMELEN + 1];
    struct _parse_data *pd = (struct _parse_data *) ip->data;

    if (len > MAXNAMELEN)
        return 0;

    strncpy (tmpbuf, var, len);
    tmpbuf[len] = '\0';
    return isdefined (tmpbuf, pd->inc, NULL);
}

static int my_eval_defined (IfParser *ip, const char *var, int len)
{
    if (lookup_variable (ip, var, len))
        return 1;
    else
        return 0;
}

#define isvarfirstletter(ccc) (isalpha(ccc) || (ccc) == '_')

static long my_eval_variable (IfParser *ip, char *var, int len)
{
    struct symtab **s;
    char *eol;

    s = lookup_variable (ip, var, len);
    if (!s)
        return 0;
    do
    {
        var = const_cast <char *> ((*s)->s_value);
        if (!isvarfirstletter (*var))
            break;
        s = lookup_variable (ip, var, strlen (var));
    }
    while (s);

    eol = strchr (var, 0);

    // Skip parentheses which Microsoft likes so much
    if (*var == '(')
    {
        var += 1 + strspn (var + 1, " \t");
        while (eol > var && isspace (eol [-1]))
            eol--;
        if (eol <= var || eol [-1] != ')')
        {
            struct _parse_data *pd = (struct _parse_data *) ip->data;
            fprintf (stderr, "\"%s\":%ld:  Invalid value used as number: `%s'\n",
                     pd->inc->i_incstring, pd->filep->f_line, var);
            return 0;
        }
        eol--;
    }

    while (eol > var && isspace (eol [-1]))
        eol--;
    *eol = 0;

    return strtol (var, NULL, 0);
}

int cppsetup (char *line, struct filepointer *filep, struct inclist *inc)
{
    IfParser ip;
    struct _parse_data pd;
    long val = 0;

    pd.filep = filep;
    pd.inc = inc;
    pd.line = line;
    ip.funcs.handle_error = my_if_errors;
    ip.funcs.eval_defined = my_eval_defined;
    ip.funcs.eval_variable = my_eval_variable;
    ip.data = (char *) &pd;

    (void) ParseIfExpression (&ip, line, &val);
    if (val)
        return IF;
    else
        return IFFALSE;
}

#endif /* CPP */
