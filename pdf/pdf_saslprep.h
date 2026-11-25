/* SASLprep (RFC 4013) password preparation.
 *
 * This implements the SASLprep stringprep profile manually,
 * replacing the former dependency on libidn's stringprep().
 * libunistring (a transitive dependency of libidn2) is used
 * for NFKC normalization, Unicode category checks, case
 * folding, and bidi class queries.
 *
 * Returns:
 *   0  — success, *output is malloc'd, *output_len set
 *   1  — soft error (prohibited chars or bidi violation),
 *        caller should fall back to original password
 *  -1  — hard error (NFKC failure or memory error)
 */
#ifndef PDF_SASLPREP_H
#define PDF_SASLPREP_H

int pdf_saslprep(const char *input, int input_len,
                 char **output, int *output_len);

#endif /* PDF_SASLPREP_H */
