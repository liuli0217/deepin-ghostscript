/* Copyright (C) 2020-2023 Artifex Software, Inc.
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

/* platform-specific string to UTF8 conversion routines */

/* This code is in its own file because it needs to be compiled with
 * a different set of complier flags to normal when built for MS Windows.
 * We can't use the /Za flag with a file whcih includes windows.h, disabling
 * the language extensions breaks the compilation. See pdf/pdf.mak for the
 * build-specifics.
 */

#include "ghostpdf.h"
#include "pdf_utf8.h"
#include "pdf_types.h"
#include "pdf_stack.h"

#ifdef HAVE_LIBIDN2
#  include <idn2.h>
#  include <errno.h>
#  include <iconv.h>
#  include <langinfo.h>
#  include <locale.h>
#  include <stdlib.h>
/* Convert a string from the current locale's character set to UTF-8.
 * Uses iconv (POSIX standard, part of glibc) to perform the conversion,
 * replacing the former stringprep_locale_to_utf8() call that was lost
 * when migrating from libidn to libidn2.
 * <string> .locale_to_utf8 <string> */
int
locale_to_utf8(pdf_context *ctx, pdf_string *input, pdf_string **output)
{
    iconv_t cd;
    const char *codeset;
    char *inbuf, *outbuf;
    size_t inbytes, outbytes, outlen;
    char *utf8buf = NULL;
    int code;

    *output = NULL;

    setlocale(LC_CTYPE, "");
#ifdef CODESET
    codeset = nl_langinfo(CODESET);
#else
    codeset = "UTF-8";
#endif

    /* If the locale is already UTF-8 (or "C"), just copy the input. */
    if (codeset == NULL || strcmp(codeset, "UTF-8") == 0 ||
        strcmp(codeset, "C") == 0 || strcmp(codeset, "ANSI_X3.4-1968") == 0) {
        code = pdfi_object_alloc(ctx, PDF_STRING, input->length, (pdf_obj **)output);
        if (code < 0)
            return code;
        pdfi_countup(*output);
        memcpy((*output)->data, input->data, input->length);
        return 0;
    }

    cd = iconv_open("UTF-8", codeset);
    if (cd == (iconv_t)-1) {
        /* Cannot open converter; fall back to copying the input unchanged. */
        code = pdfi_object_alloc(ctx, PDF_STRING, input->length, (pdf_obj **)output);
        if (code < 0)
            return code;
        pdfi_countup(*output);
        memcpy((*output)->data, input->data, input->length);
        return 0;
    }

    /* Allocate output buffer: worst case is 4x expansion for UTF-8. */
    outlen = input->length * 4 + 1;
    utf8buf = (char *)malloc(outlen);
    if (utf8buf == NULL) {
        iconv_close(cd);
        return_error(gs_error_VMerror);
    }

    inbuf = (char *)input->data;
    inbytes = input->length;
    outbuf = utf8buf;
    outbytes = outlen;

    if (iconv(cd, &inbuf, &inbytes, &outbuf, &outbytes) == (size_t)-1) {
        /* Conversion error (e.g. invalid sequence); fall back to
         * copying the input unchanged, matching the original
         * stringprep_locale_to_utf8 behaviour on EILSEQ/EINVAL. */
        free(utf8buf);
        iconv_close(cd);
        code = pdfi_object_alloc(ctx, PDF_STRING, input->length, (pdf_obj **)output);
        if (code < 0)
            return code;
        pdfi_countup(*output);
        memcpy((*output)->data, input->data, input->length);
        return 0;
    }

    iconv_close(cd);
    outlen = outlen - outbytes;  /* actual bytes written */

    code = pdfi_object_alloc(ctx, PDF_STRING, outlen, (pdf_obj **)output);
    if (code < 0) {
        free(utf8buf);
        return code;
    }
    pdfi_countup(*output);
    memcpy((*output)->data, utf8buf, outlen);
    free(utf8buf);
    return 0;
}
#else
#ifdef _MSC_VER
#include "windows_.h"
/* Convert a string from the current locale's character set to UTF-8.
 * Unfortunately, "current locale" can mean a few different things on
 * Windows -- we use the default ANSI code page, which does the right
 * thing for command-line arguments (like "-sPDFPassword=foo") and
 * for strings typed as input to gswin32.exe.  It doesn't work for
 * strings typed as input to gswin32c.exe, which are normally in the
 * default OEM code page instead.
 * <string> .locale_to_utf8 <string> */
int
locale_to_utf8(pdf_context *ctx, pdf_string *input, pdf_string **output)
{
#define LOCALE_TO_UTF8_BUFFER_SIZE 1024
    WCHAR wide_buffer[LOCALE_TO_UTF8_BUFFER_SIZE];
    char utf8_buffer[LOCALE_TO_UTF8_BUFFER_SIZE];
    int code, BytesWritten;

    *output = NULL;

    BytesWritten = MultiByteToWideChar(CP_ACP, 0, input->data, input->length,
        wide_buffer, LOCALE_TO_UTF8_BUFFER_SIZE);
    if (BytesWritten == 0)
        return_error(gs_error_ioerror);

    BytesWritten = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, BytesWritten,
        utf8_buffer, LOCALE_TO_UTF8_BUFFER_SIZE, NULL, NULL);
    if (BytesWritten == 0)
        return_error(gs_error_ioerror);

    code = pdfi_object_alloc(ctx, PDF_STRING, BytesWritten, (pdf_obj **)output);
    if (code < 0)
        return code;
    pdfi_countup(*output);
    memcpy((*output)->data, utf8_buffer, BytesWritten);

    return 0;
#undef LOCALE_TO_UTF8_BUFFER_SIZE
}
#else
/* We have no known method to create a UTF-8 string. Just copy the input and pretend.
 */
int
locale_to_utf8(pdf_context *ctx, pdf_string *input, pdf_string **output)
{
    int code = 0;

    code = pdfi_object_alloc(ctx, PDF_STRING, input->length, (pdf_obj **)output);
    if (code < 0)
        return code;
    pdfi_countup(*output);
    memcpy((*output)->data, input->data, input->length);

    return 0;
}
#endif /* _WINDOWS_ */
#endif /* HAVE_LIBIDN2 */
