/*
 * Copyright (c) 2017 Jordan Vaughan
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SIMPLELEXER_H
#define __SIMPLELEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*
 * This represents a position (line and column) within a stream of text.
 * Both fields are one-based.
 */
typedef struct TextPosition {
    size_t line;
    size_t column;
} TextPosition;

/*
 * This represents a span of text within a file or stream.
 *
 * SimpleLexer guarantees that `start` <= `end`:
 * Either start.line < end.line
 * OR they're the same and start.column <= end.column.
 */
typedef struct TextSpan {
    TextPosition start;
    TextPosition end;
} TextSpan;

/*
 * a lexed token, including its starting and ending positions within its stream
 */
typedef struct SimpleToken {
    /* the NUL-terminated token text */
    char* text;

    /* the string length of text (does not include the terminating NUL) */
    size_t length;

    /* where the token is located in its text stream */
    TextSpan span;

    /* nonzero if the token was quoted */
    char quoted;

    /* nonzero if the token started with an escaped character */
    char startedEscaped;
} SimpleToken;

/*
 * Copy `source` into `dest`.
 * This allocates memory for `dest`'s text buffer
 * using the standard library's malloc().
 * Pass dest to SimpleToken_Destroy() to free dest's text buffer.
 *
 * This returns zero if the copy succeeded and nonzero if malloc() failed.
 */
extern int SimpleToken_Copy(
    const SimpleToken *restrict source,
    SimpleToken *restrict dest);

/*
 * Deallocate a SimpleToken's text buffer but not the SimpleToken itself.
 * This frees the token's text buffer using the standard library's free().
 */
extern void SimpleToken_Destroy(SimpleToken *token);

/*
 * Allocate a new SimpleToken that is a copy of the given token.
 * Tokens returned by this function should be destroyed via SimpleToken_Free().
 * This function uses the standard library's malloc().
 *
 * This returns the new token or NULL if malloc() failed.
 */
extern SimpleToken *SimpleToken_Duplicate(const SimpleToken *source);

/*
 * Deallocate a SimpleToken created by SimpleToken_Duplicate().
 * This frees both the token structure itself and its text buffer
 * using the standard library's free().
 */
extern void SimpleToken_Free(SimpleToken *token);

/*
 * This is a simple lexer that produces SimpleTokens.  A token is a sequence of
 * characters delimited by whitespace (characters that cause isspace() to return
 * nonzero values).  Backslashes escape the characters that follow them,
 * causing them to be included in their tokens literally.  (Single-character
 * C escapes are handled as in C strings.) Double quotation marks ('"')
 * can enclose tokens, which is one way to add whitespace to tokens.
 * Single-line comments begin with unescaped, unquoted '#' characters.
 *
 * New lexers should be initialized via SimpleLexer_Init().
 *
 * All of this structure's fields should be considered read-only.
 */
typedef struct SimpleLexer
{
    /* the lexer's current position */
    TextPosition currentPosition;

    /* the number of columns in the previous line */
    size_t numColumnsInPreviousLine;

    /* the start of the current token */
    TextPosition tokenStart;

    char escaping;              /* set if lexer is escaping a character */
    char inToken;               /* set if lexer is inside a token */
    char finishedQuotedToken;   /* set if lexer just finished a
                                   quoted token */
    char inComment;             /* set if lexer is inside a comment */
    char tokenIsQuoted;         /* set if the lexed token was quoted */
    char startedEscaped;        /* set if the lexed token was started
                                   with an escaped character */
    char finished;              /* set if lexer finished its stream */

    char* buffer;           /* token text buffer
                               (not owned by the lexer) */
    size_t bufferLength;    /* current token length */
    size_t bufferCapacity;  /* token text buffer's byte size */
} SimpleLexer;

/*
 * SimpleLexer functions return these error codes.
 */
typedef enum SimpleLexerError {
    /* no errors occurred */
    SIMPLE_LEXER_OK = 0,

    /* the lexer reached the end of its stream */
    SIMPLE_LEXER_EOF,

    /* the current token is too large for the lexer's buffer */
    SIMPLE_LEXER_TOKEN_TOO_LARGE,

    /* the lexer reached the end of its stream while parsing a quoted token
       but hasn't found closing quotation marks */
    SIMPLE_LEXER_UNCLOSED_QUOTED_TOKEN,

    /* the stream ended with an unescaped backslash */
    SIMPLE_LEXER_ESCAPING_EOF,

    /* not a real error code: just number of error codes */
    SIMPLE_LEXER_NUMERRORCODES
} SimpleLexerError;

/*
 * Initialize or reset the specified SimpleLexer.
 * The caller must specify a byte buffer for token text.
 * Note that the caller is responsible for freeing this buffer (if necessary)
 * once lexing is done.  The length of the buffer determines the maximum
 * token size.  The SimpleLexer functions do NOT reallocate the buffer
 * while lexing large tokens.
 *
 * `tokenBufferSize` is the size of `tokenBuffer` in bytes, including the
 * terminating NUL byte.  For example, if `tokenBuffer` is 1024 bytes,
 * permitting a maximum token size of 1023 bytes,
 * `tokenBufferSize` should be 1024.
 */
extern void SimpleLexer_Init(
    SimpleLexer *restrict lexer,
    char *restrict tokenBuffer,
    size_t tokenBufferSize);

/*
 * Get the next token from the specified string
 * starting at the specified `index`.
 *
 * This function updates the value stored at `index` as it scans the string.
 * Pass the `source` string and this `index` value to the function repeatedly
 * to get subsequent tokens.  You can modify the value stored at `index`
 * (to reset the stream, for example).
 *
 * This function returns SIMPLE_LEXER_OK if it successfully lexed a token,
 * which is stored in the `token` parameter.
 * Note that this token should NOT be passed to
 * SimpleToken_Free() or SimpleToken_Destroy().
 * The token's text buffer is the same buffer that was given to the lexer
 * when the lexer was initialized with SimpleLexer_Init().
 *
 * This function returns SIMPLE_LEXER_EOF when the lexer reaches the end of
 * the string (that is, when `index` >= `sourceSize`) even when there is
 * a token at the end of the string.  This makes it possible for callers to lex
 * tokens from streams of text.  When the stream is finished, call
 * SimpleLexer_Finish() to get the final token, if any.
 *
 * Other return codes:
 *
 *    o  SIMPLE_LEXER_TOKEN_TOO_LARGE: The token being lexed is too large
 *       for the lexer's text buffer.
 */
extern SimpleLexerError SimpleLexer_GetNextToken(
    SimpleLexer *restrict lexer,
    const char *restrict source,
    size_t sourceSize,
    size_t *restrict index,
    SimpleToken *restrict token);

/*
 * Get the final token, if any, and shut down the lexer,
 * preventing its use in future SimpleLexer_GetNextToken()
 * and SimpleLexer_Finish() calls.
 *
 * Lexers passed to this function can be reset by passing them
 * to SimpleLexer_Init().
 *
 * This function returns SIMPLE_LEXER_OK if there is a final token,
 * which is stored in the `finalToken` parameter.
 * Note that this token should NOT be passed to
 * SimpleToken_Free() or SimpleToken_Destroy().
 * The token's text buffer is the same buffer that was given to the lexer
 * when the lexer was initialized with SimpleLexer_Init().
 *
 * This function returns SIMPLE_LEXER_EOF when there is no final token.
 * In this case, `finalToken` is untouched.
 *
 * Other return codes:
 *
 *    o  SIMPLE_LEXER_ESCAPING_EOF: The stream ended with an escape character
 *       ('\'), but it is incomplete because no character follows it.
 *
 *    o  SIMPLE_LEXER_UNCLOSED_QUOTED_TOKEN: The stream ended in a quoted token
 *       that wasn't closed with quotation marks ('"').
 */
extern SimpleLexerError SimpleLexer_Finish(
    SimpleLexer *restrict lexer,
    SimpleToken *restrict finalToken);

#ifdef __cplusplus
}
#endif

#endif  /* __SIMPLELEXER_H */

