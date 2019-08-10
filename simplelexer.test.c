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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SimpleLexer lexer;
static SimpleToken token;
static size_t idx;
static char defaultBuffer[1024];

#define TEST_ASSERT(x) \
    do { \
        if (!(x)) { \
            (void) fprintf(stderr, "assertion failed: " #x "\n"); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL(x, y) \
    do { \
        if ((x) != (y)) { \
            (void) fprintf(stderr, "assertion failed: expected " #x " to equal " #y "\n"); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_STREQ(x, y) \
    do { \
        if (strcmp((x), (y)) != 0) { \
            (void) fprintf(stderr, "assertion failed: expected " #x " to equal " #y "\n"); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_SPAN_EQUAL(span, startline, startcol, endline, endcol) \
    do { \
        if ((span).start.line != (startline) \
            || (span).start.column != (startcol) \
            || (span).end.line != (endline) \
            || (span).end.column != (endcol)) { \
            (void) fprintf(stderr, "assertion failed: expected token span %zu:%zu-%zu:%zu " \
                "but got %zu:%zu-%zu:%zu\n", (size_t)startline, (size_t)startcol, (size_t)endline, \
                (size_t)endcol, (span).start.line, (span).start.column, (span).end.line, \
                (span).end.column); \
            return 1; \
        } \
    } while (0)

#define TEST_GET_TOKEN(expectedResult) TEST_ASSERT_EQUAL(SimpleLexer_GetNextToken(&lexer, &token), expectedResult)
#define TEST_FINISH(expectedResult) TEST_ASSERT_EQUAL(SimpleLexer_Finish(&lexer, &token), expectedResult)
#define TEST_SPAN(startline, startcol, endline, endcol) TEST_ASSERT_SPAN_EQUAL(token.span, startline, startcol, endline, endcol)

static int EmptyInputYieldsEofAndNoTokens()
{
    SimpleLexer_SetInput(&lexer, "", 0);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_ASSERT_EQUAL(token.text, NULL);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_EQUAL(token.text, NULL);
    TEST_SPAN(0, 0, 0, 0);

    return 0;
}

static int OneUnquotedToken()
{
    const char *input = "token";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_ASSERT_EQUAL(token.text, NULL);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, input);
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 5);

    return 0;
}

static int LexAfterFinishReturnsEof()
{
    const char *input = "token";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);

    return 0;
}

static int SettingNewInputAfterFinishMakesLexReturnEof()
{
    const char *input = "token";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);

    return 0;
}

static int FinishAfterFinishReturnsEof()
{
    const char *input = "token";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_FINISH(SIMPLE_LEXER_EOF);

    return 0;
}

static int SettingTwoDifferentInputsUsesTheSecondInput()
{
    const char *input1 = "token1";
    const char *input2 = "token2";
    const size_t len = strlen(input1);

    SimpleLexer_SetInput(&lexer, input1, len);
    SimpleLexer_SetInput(&lexer, input2, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);

    TEST_ASSERT_STREQ(token.text, input2);
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    return 0;
}

static int SettingInputAfterParsingPartOfAnotherDiscardsUnusedInput()
{
    const char *input1 = "token11 token12";
    const char *input2 = "token21 token22";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    SimpleLexer_SetInput(&lexer, input2, len2);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);

    TEST_ASSERT_STREQ(token.text, "token21");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    return 0;
}

static int ParserStaysAtCurrentLineAndColumnAfterSettingInput()
{
    const char *input1 = "token11 token12";
    const char *input2 = "token21 token22";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);
    (void) SimpleLexer_GetNextToken(&lexer, &token);

    TEST_SPAN(1, 9, 1, 15);
    return 0;
}

static int UnquotedTextAtEofPrefixesTextOfNextInput()
{
    const char* input1 = "prefix";
    const char* input2 = "suffix token";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);

    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "prefixsuffix");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 12);
    return 0;
}

static int QuotedTextAtEofPrefixesTextOfNextInput()
{
    const char* input1 = "\"prefix ";
    const char* input2 = " suffix\" token";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);

    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "prefix  suffix");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 16);
    return 0;
}

static int TextAtEofThatStartedEscapedPrefixesTextOfNextInput()
{
    const char* input1 = "\\nprefix";
    const char* input2 = "suffix token";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);

    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "\nprefixsuffix");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 1);
    TEST_SPAN(1, 1, 1, 14);
    return 0;
}

static int TextAtEofBecomesNextTokenWhenNextInputStartsWithSpace()
{
    const char* input1 = "token1";
    const char* input2 = " token2";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);

    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    return 0;
}

static int TextAtEofBecomesNextTokenWhenNextInputStartsWithTab()
{
    const char* input1 = "token1";
    const char* input2 = "\ttoken2";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);

    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    return 0;
}

static int TextAtEofBecomesNextTokenWhenNextInputStartsWithNewline()
{
    const char* input1 = "token1";
    const char* input2 = "\ntoken2";
    const size_t len1 = strlen(input1);
    const size_t len2 = strlen(input2);

    SimpleLexer_SetInput(&lexer, input1, len1);
    (void) SimpleLexer_GetNextToken(&lexer, &token);
    SimpleLexer_SetInput(&lexer, input2, len2);

    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    return 0;
}

static int TwoUnquotedTokens()
{
    const char *input = "token1 token2";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 8, 1, 13);

    return 0;
}

static int TwoTokens_FirstQuoted()
{
    const char *input = "\"token1\" token2";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 10, 1, 15);

    return 0;
}

static int TwoTokens_SecondQuoted()
{
    const char *input = "token1 \"token2\"";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 8, 1, 15);

    return 0;
}

static int TwoQuotedTokens_WithSpace()
{
    const char *input = "\"token1\" \"token2\"";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 10, 1, 17);

    return 0;
}

static int TwoQuotedTokens_Adjacent()
{
    const char *input = "\"token1\"\"token2\"";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 9, 1, 16);

    return 0;
}

static int TwoTokens_FirstQuoted_Adjacent()
{
    const char *input = "\"token1\"token2";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);

    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 9, 1, 14);

    return 0;
}

static int TwoTokens_SecondQuoted_Adjacent()
{
    const char *input = "token1\"token2\"";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 7, 1, 14);

    return 0;
}

static int UnquotedTokenWithEscapedInnerQuotationMark()
{
    const char *input = "token\\\"token";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token\"token");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 12);

    return 0;
}

static int QuotedTokenWithEscapedInnerQuotationMark()
{
    const char *input = "\"token\\\"token\"";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token\"token");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 14);

    return 0;
}

static int QuotedTokenWithEmbeddedNewline()
{
    const char *input = "\"token\ntoken\"";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token\ntoken");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 2, 6);

    return 0;
}

static int TokenSequenceWithQuotedTokenWithEmbeddedNewline()
{
    const char *input = "token1 \"token2\ntoken2\" token3";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2\ntoken2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 8, 2, 7);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token3");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(2, 9, 2, 14);

    return 0;
}

static int LeadingWhitespace()
{
    const char *input = " \t\n token";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(2, 2, 2, 6);

    return 0;
}

static int TrailingWhitespace()
{
    const char *input = "token \t\n ";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 5);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);

    return 0;
}

static int MiddleWhitespace()
{
    const char *input = "token1 \t\n\t\n\t token2";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(3, 3, 3, 8);

    return 0;
}

static int LeadingTrailingAndMiddleWhitespace()
{
    const char *input = " \t\n token1 \t\n\t\n\t token2 \t\n\t ";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(2, 2, 2, 7);
    TEST_GET_TOKEN(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(4, 3, 4, 8);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);

    return 0;
}

static int TokenStartedEscaped()
{
    const char *input = "\\mytoken";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "mytoken");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 1);
    TEST_SPAN(1, 1, 1, 8);

    return 0;
}

static int CEscapeCharactersProduceAsciiEquivalents()
{
    const char *input = "\\a\\b\\f\\n\\r\\t\\v";
    const size_t len = strlen(input);

    SimpleLexer_SetInput(&lexer, input, len);
    TEST_GET_TOKEN(SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "\a\b\f\n\r\t\v");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 1);
    TEST_SPAN(1, 1, 1, 14);

    return 0;
}

typedef struct Test
{
    const char *name;
    int (*test)();
} Test;

#define REGISTER_TEST(name) { #name, name }

static Test tests[] = {
    REGISTER_TEST(EmptyInputYieldsEofAndNoTokens),
    REGISTER_TEST(OneUnquotedToken),
    REGISTER_TEST(LexAfterFinishReturnsEof),
    REGISTER_TEST(SettingNewInputAfterFinishMakesLexReturnEof),
    REGISTER_TEST(FinishAfterFinishReturnsEof),
    REGISTER_TEST(SettingTwoDifferentInputsUsesTheSecondInput),
    REGISTER_TEST(SettingInputAfterParsingPartOfAnotherDiscardsUnusedInput),
    REGISTER_TEST(ParserStaysAtCurrentLineAndColumnAfterSettingInput),
    REGISTER_TEST(UnquotedTextAtEofPrefixesTextOfNextInput),
    REGISTER_TEST(QuotedTextAtEofPrefixesTextOfNextInput),
    REGISTER_TEST(TextAtEofThatStartedEscapedPrefixesTextOfNextInput),
    REGISTER_TEST(TextAtEofBecomesNextTokenWhenNextInputStartsWithSpace),
    REGISTER_TEST(TextAtEofBecomesNextTokenWhenNextInputStartsWithTab),
    REGISTER_TEST(TextAtEofBecomesNextTokenWhenNextInputStartsWithNewline),
    REGISTER_TEST(TwoUnquotedTokens),
    REGISTER_TEST(TwoTokens_FirstQuoted),
    REGISTER_TEST(TwoTokens_SecondQuoted),
    REGISTER_TEST(TwoQuotedTokens_WithSpace),
    REGISTER_TEST(TwoQuotedTokens_Adjacent),
    REGISTER_TEST(TwoTokens_FirstQuoted_Adjacent),
    REGISTER_TEST(TwoTokens_SecondQuoted_Adjacent),
    REGISTER_TEST(UnquotedTokenWithEscapedInnerQuotationMark),
    REGISTER_TEST(QuotedTokenWithEscapedInnerQuotationMark),
    REGISTER_TEST(QuotedTokenWithEmbeddedNewline),
    REGISTER_TEST(TokenSequenceWithQuotedTokenWithEmbeddedNewline),
    REGISTER_TEST(LeadingWhitespace),
    REGISTER_TEST(TrailingWhitespace),
    REGISTER_TEST(MiddleWhitespace),
    REGISTER_TEST(LeadingTrailingAndMiddleWhitespace),
    REGISTER_TEST(TokenStartedEscaped),
    REGISTER_TEST(CEscapeCharactersProduceAsciiEquivalents),
    { NULL, NULL },
};

int main(int argc, char **argv)
{
    size_t numPassed;
    size_t numFailed;
    Test *currentTest;

    (void) fprintf(stdout, "Running tests...\n\n");

    numPassed = 0;
    numFailed = 0;
    for (currentTest = tests; currentTest->name != NULL; ++currentTest)
    {
        (void) fprintf(stdout, "> %s RUN\n", currentTest->name);
        SimpleLexer_Init(&lexer, defaultBuffer, sizeof(defaultBuffer)); 
        (void) memset(&token, 0, sizeof(token));
        idx = 0;
        (void) memset(defaultBuffer, 0, sizeof(defaultBuffer));
        if (currentTest->test() == 0)
        {
            (void) fprintf(stdout, "> %s PASS\n", currentTest->name);
            ++numPassed;
        }
        else
        {
            (void) fprintf(stdout, "> %s FAIL\n", currentTest->name);
            ++numFailed;
        }
    }

    (void) fprintf(stdout, "\nPassed: %zu\nFailed: %zu\n\n",
        numPassed, numFailed);
    return numFailed ? 1 : 0;
}

