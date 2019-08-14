/*
 * Copyright (c) 2019 Jordan Vaughan
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

#include "simplelexer.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void SimpleLexer_Init(
    SimpleLexer* restrict lexer,
    char* restrict tokenBuffer,
    size_t tokenBufferSize)
{
    assert(lexer != NULL);
    assert(tokenBuffer != NULL);
    assert(tokenBufferSize != 0);

    lexer->currentPosition.line = 1;
    lexer->currentPosition.column = 1;
    lexer->numColumnsInPreviousLine = 0;
    lexer->tokenStart.line = 1;
    lexer->tokenStart.column = 1;

    lexer->escaping = 0;
    lexer->inToken = 0;
    lexer->inComment = 0;
    lexer->tokenIsQuoted = 0;
    lexer->startedEscaped = 0;
    lexer->finished = 0;

    lexer->buffer = tokenBuffer;
    lexer->bufferLength = 0;
    lexer->bufferCapacity = tokenBufferSize;

    lexer->input = NULL;
    lexer->inputSize = 0;
    lexer->inputIndex = 0;
}

static int SimpleLexer_AppendToBuffer(SimpleLexer* lexer, char c)
{
    assert(lexer != NULL);
    assert(lexer->buffer != NULL);

    if (lexer->bufferLength < lexer->bufferCapacity - 1)
    {
        lexer->buffer[lexer->bufferLength] = c;
        ++lexer->bufferLength;
        return 0;
    }
    return 1;
}

static void SimpleLexer_StartToken(
    SimpleLexer* restrict lexer,
    int_fast8_t quoted,
    int_fast8_t startedEscaped)
{
    assert(lexer != NULL);
    assert(lexer->buffer != NULL);
    assert(lexer->inToken == 0);
    assert(lexer->inComment == 0);
    assert(lexer->finished == 0);

    lexer->tokenStart = lexer->currentPosition;

    lexer->inToken = 1;
    lexer->tokenIsQuoted = quoted;
    lexer->startedEscaped = startedEscaped;
}

static void SimpleLexer_FinishToken(
    SimpleLexer* restrict lexer,
    SimpleToken* restrict outToken,
    char recordCurrentPositionAsEnd)
{
    assert(lexer != NULL);
    assert(lexer->buffer != NULL);
    assert(outToken != NULL);

    lexer->buffer[lexer->bufferLength] = '\0';

    outToken->text = lexer->buffer;
    outToken->length = lexer->bufferLength;
    outToken->span.start = lexer->tokenStart;
    outToken->quoted = lexer->tokenIsQuoted;
    outToken->startedEscaped = lexer->startedEscaped;

    lexer->bufferLength = 0;
    lexer->inToken = 0;

    if (recordCurrentPositionAsEnd)
    {
        outToken->span.end = lexer->currentPosition;
    }
    else if (lexer->currentPosition.column != 1)
    {
        outToken->span.end.line = lexer->currentPosition.line;
        outToken->span.end.column = lexer->currentPosition.column - 1;
    }
    else if (lexer->currentPosition.line > 1)
    {
        outToken->span.end.line = lexer->currentPosition.line - 1;
        outToken->span.end.column = lexer->numColumnsInPreviousLine;
    }
    else
    {
        outToken->span.end.line = 1;
        outToken->span.end.column = 1;
    }
}

int SimpleToken_Copy(
    const SimpleToken* restrict source,
    SimpleToken* restrict dest)
{
    assert(source != NULL);
    assert(dest != NULL);
    assert(source != dest);

    dest->text = malloc(source->length + 1);
    if (dest->text == NULL)
    {
        return 1;
    }

    (void) memcpy(dest->text, source->text, source->length + 1);
    dest->length = source->length;
    dest->span = source->span;
    dest->quoted = source->quoted;
    dest->startedEscaped = source->startedEscaped;

    return 0;
}

void SimpleToken_Destroy(SimpleToken* token)
{
    assert(token != NULL);

    free(token->text);
}

SimpleToken* SimpleToken_Duplicate(const SimpleToken* source)
{
    SimpleToken* copy;

    assert(source != NULL);
    assert(source->text != NULL);

    copy = malloc(sizeof(*source));
    if (!copy)
    {
        return NULL;
    }

    if (SimpleToken_Copy(source, copy))
    {
        free(copy);
        return NULL;
    }
    return copy;
}

void SimpleToken_Free(SimpleToken* token)
{
    assert(token != NULL);

    SimpleToken_Destroy(token);
    free(token);
}

static inline void SimpleLexer_AdvanceLine(SimpleLexer* lexer)
{
    ++lexer->currentPosition.line;
    lexer->numColumnsInPreviousLine = lexer->currentPosition.column;
    lexer->currentPosition.column = 1;
}

void SimpleLexer_SetInput(
    SimpleLexer* restrict lexer,
    const char* restrict text,
    size_t textSize)
{
    assert(lexer != NULL);
    assert(text != NULL);

    lexer->input = text;
    lexer->inputSize = textSize;
    lexer->inputIndex = 0;
}

SimpleLexerError SimpleLexer_GetNextToken(
    SimpleLexer* restrict lexer,
    SimpleToken* restrict outToken)
{
    char c;

    assert(lexer != NULL);
    assert(lexer->buffer != NULL);
    assert(outToken != NULL);

    if (lexer->finished || lexer->inputIndex >= lexer->inputSize) {
        return SIMPLE_LEXER_EOF;
    }
    assert(lexer->input != NULL);

    for (c = lexer->input[lexer->inputIndex];
        lexer->inputIndex < lexer->inputSize;
        c = lexer->input[++lexer->inputIndex])
    {
        /* c is at position lexer->currentPosition.
           Don't advance lexer->currentPosition until we've consumed c. */

        if (lexer->inComment)
        {
            if (c == '\n')
            {
                lexer->inComment = 0;
            }
        }
        else if (lexer->escaping)
        {
            assert(lexer->inToken);

            char decodedChar;
            switch (c)
            {
                case 'a': decodedChar = '\a'; break;
                case 'b': decodedChar = '\b'; break;
                case 'f': decodedChar = '\f'; break;
                case 'n': decodedChar = '\n'; break;
                case 'r': decodedChar = '\r'; break;
                case 't': decodedChar = '\t'; break;
                case 'v': decodedChar = '\v'; break;
                default: decodedChar = c; break;
            }

            if (SimpleLexer_AppendToBuffer(lexer, decodedChar))
            {
                return SIMPLE_LEXER_TOKEN_TOO_LARGE;
            }
            lexer->escaping = 0;
        }
        else if (c == '\n')
        {
            if (lexer->inToken)
            {
                if (lexer->tokenIsQuoted)
                {
                    if (SimpleLexer_AppendToBuffer(lexer, c))
                    {
                        return SIMPLE_LEXER_TOKEN_TOO_LARGE;
                    }
                }
                else
                {
                    SimpleLexer_FinishToken(lexer, outToken, 0);
                    SimpleLexer_AdvanceLine(lexer);
                    ++lexer->inputIndex;
                    return SIMPLE_LEXER_OK;
                }
            }
        }
        else if (isspace(c) || c == '\0')
        {
            if (lexer->inToken)
            {
                if (lexer->tokenIsQuoted)
                {
                    if (SimpleLexer_AppendToBuffer(lexer, c))
                    {
                        return SIMPLE_LEXER_TOKEN_TOO_LARGE;
                    }
                }
                else
                {
                    SimpleLexer_FinishToken(lexer, outToken, 0);
                    ++lexer->currentPosition.column;
                    ++lexer->inputIndex;
                    return SIMPLE_LEXER_OK;
                }
            }
        }
        else if (c == '"')
        {
            if (lexer->inToken)
            {
                if (lexer->tokenIsQuoted)
                {
                    SimpleLexer_FinishToken(lexer, outToken, 1);
                    ++lexer->currentPosition.column;
                    ++lexer->inputIndex;
                }
                else
                {
                    SimpleLexer_FinishToken(lexer, outToken, 0);
                }
                return SIMPLE_LEXER_OK;
            }
            else
            {
                SimpleLexer_StartToken(lexer, 1, 0);
            }
        }
        else if (c == '\\')
        {
            lexer->escaping = 1;
            if (!lexer->inToken)
            {
                SimpleLexer_StartToken(lexer, 0, 1);
            }
        }
        else if (c == '#')
        {
            if (lexer->inToken && lexer->tokenIsQuoted)
            {
                if (SimpleLexer_AppendToBuffer(lexer, c))
                {
                    return SIMPLE_LEXER_TOKEN_TOO_LARGE;
                }
            }
            else
            {
                lexer->inComment = 1;
                if (lexer->bufferLength != 0)
                {
                    SimpleLexer_FinishToken(lexer, outToken, 0);
                    ++lexer->currentPosition.column;
                    ++lexer->inputIndex;
                    return SIMPLE_LEXER_OK;
                }
            }
        }
        else
        {
            if (!lexer->inToken)
            {
                SimpleLexer_StartToken(lexer, 0, 0);
            }
            if (SimpleLexer_AppendToBuffer(lexer, c))
            {
                return SIMPLE_LEXER_TOKEN_TOO_LARGE;
            }
        }

        /* Advance the lexer's current position in the stream. */
        if (c == '\n')
        {
            SimpleLexer_AdvanceLine(lexer);
        }
        else
        {
            ++lexer->currentPosition.column;
        }
    }

    return SIMPLE_LEXER_EOF;
}

SimpleLexerError SimpleLexer_Finish(
    SimpleLexer* restrict lexer,
    SimpleToken* restrict finalToken)
{
    int error;

    assert(lexer != NULL);
    assert(finalToken != NULL);

    if (lexer->finished)
    {
        return SIMPLE_LEXER_EOF;
    }

    error = SIMPLE_LEXER_OK;

    if (lexer->escaping)
    {
        error = SIMPLE_LEXER_ESCAPING_EOF;
    }

    if (lexer->inToken && lexer->tokenIsQuoted)
    {
        error = SIMPLE_LEXER_UNCLOSED_QUOTED_TOKEN;
    }

    if (lexer->bufferLength != 0)
    {
        SimpleLexer_FinishToken(lexer, finalToken, 0);
    }
    else if (error == SIMPLE_LEXER_OK)
    {
        error = SIMPLE_LEXER_EOF;
    }

    lexer->finished = 1;

    return error;
}

