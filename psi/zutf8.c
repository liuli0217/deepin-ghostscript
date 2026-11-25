/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* The .locale_to_utf8 operator, for converting text from the locale
 * charset to UTF-8.  This code is not used on Windows; there's a
 * separate Windows implementation in zwinutf8.c. */

#include "ghost.h"
#include "oper.h"
#include "iutil.h"
#include "ialloc.h"
#include "malloc_.h"
#include "errno_.h"
#include "string_.h"
#include "store.h"
#include <idn2.h>
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <stdlib.h>

/* Convert a string from the current locale's character set to UTF-8.
 * Uses iconv (POSIX standard, part of glibc) to perform the conversion,
 * replacing the former stringprep_locale_to_utf8() call that was lost
 * when migrating from libidn to libidn2.
 * <string> .locale_to_utf8 <string> */
static int
zlocale_to_utf8(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    char *input;
    char *output = NULL;
    int code;

    check_op(1);
    check_read_type(*op, t_string);
    input = ref_to_string(op, imemory, "locale_to_utf8 input");
    if (input == 0)
        return_error(gs_error_VMerror);

    {
        iconv_t cd;
        const char *codeset;
        char *inbuf, *outbuf;
        size_t inbytes, outbytes, outlen;
        size_t input_len = strlen(input);

        setlocale(LC_CTYPE, "");
#ifdef CODESET
        codeset = nl_langinfo(CODESET);
#else
        codeset = "UTF-8";
#endif

        /* If the locale is already UTF-8 (or "C"), just return the input. */
        if (codeset == NULL || strcmp(codeset, "UTF-8") == 0 ||
            strcmp(codeset, "C") == 0 ||
            strcmp(codeset, "ANSI_X3.4-1968") == 0) {
            code = string_to_ref(input, op, iimemory, "locale_to_utf8 output");
            ifree_string((byte *)input, r_size(op) + 1, "locale_to_utf8 input");
            return code;
        }

        cd = iconv_open("UTF-8", codeset);
        if (cd == (iconv_t)-1) {
            /* Cannot open converter; fall back to returning input unchanged. */
            code = string_to_ref(input, op, iimemory, "locale_to_utf8 output");
            ifree_string((byte *)input, r_size(op) + 1, "locale_to_utf8 input");
            return code;
        }

        outlen = input_len * 4 + 1;
        output = (char *)malloc(outlen);
        if (output == NULL) {
            iconv_close(cd);
            ifree_string((byte *)input, r_size(op) + 1, "locale_to_utf8 input");
            return_error(gs_error_VMerror);
        }

        inbuf = input;
        inbytes = input_len;
        outbuf = output;
        outbytes = outlen;

        if (iconv(cd, &inbuf, &inbytes, &outbuf, &outbytes) == (size_t)-1) {
            /* Conversion error; fall back to returning input unchanged,
             * matching the original stringprep_locale_to_utf8 behaviour
             * on EILSEQ/EINVAL. */
            free(output);
            output = NULL;
            iconv_close(cd);
        } else {
            iconv_close(cd);
            output[outlen - outbytes] = '\0';
        }
    }

    if (output != NULL) {
        code = string_to_ref(output, op, iimemory, "locale_to_utf8 output");
        free(output);
    } else {
        code = string_to_ref(input, op, iimemory, "locale_to_utf8 output");
    }
    ifree_string((byte *)input, r_size(op) + 1, "locale_to_utf8 input");
    if (code < 0)
        return code;

    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zutf8_op_defs[] =
{
    {"1.locale_to_utf8", zlocale_to_utf8},
    op_def_end(0)
};
