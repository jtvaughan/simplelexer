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

#define TEST_GET_TOKEN(input, inputSize, expectedResult) TEST_ASSERT_EQUAL(SimpleLexer_GetNextToken(&lexer, input, inputSize, &idx, &token), expectedResult)
#define TEST_FINISH(expectedResult) TEST_ASSERT_EQUAL(SimpleLexer_Finish(&lexer, &token), expectedResult)
#define TEST_SPAN(startline, startcol, endline, endcol) TEST_ASSERT_SPAN_EQUAL(token.span, startline, startcol, endline, endcol)

static int TestSimpleLexer_Empty()
{
    TEST_GET_TOKEN("", 0, SIMPLE_LEXER_EOF);
    TEST_ASSERT_EQUAL(token.text, NULL);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_EQUAL(token.text, NULL);
    TEST_SPAN(0, 0, 0, 0);

    return 0;
}

static int TestSimpleLexer_OneUnquotedToken()
{
    const char *input = "token";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_ASSERT_EQUAL(token.text, NULL);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, input);
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 5);

    return 0;
}

static int TestSimpleLexer_LexAfterFinishReturnsEof()
{
    const char *input = "token";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    idx = 0;
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);

    return 0;
}

static int TestSimpleLexer_FinishAfterFinishReturnsEof()
{
    const char *input = "token";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_FINISH(SIMPLE_LEXER_EOF);

    return 0;
}

static int TestSimpleLexer_TwoUnquotedTokens()
{
    const char *input = "token1 token2";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 8, 1, 13);

    return 0;
}

static int TestSimpleLexer_TwoTokens_FirstQuoted()
{
    const char *input = "\"token1\" token2";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 10, 1, 15);

    return 0;
}

static int TestSimpleLexer_TwoTokens_SecondQuoted()
{
    const char *input = "token1 \"token2\"";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 8, 1, 15);

    return 0;
}

static int TestSimpleLexer_TwoQuotedTokens_WithSpace()
{
    const char *input = "\"token1\" \"token2\"";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 10, 1, 17);

    return 0;
}

static int TestSimpleLexer_TwoQuotedTokens_Adjacent()
{
    const char *input = "\"token1\"\"token2\"";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 9, 1, 16);

    return 0;
}

static int TestSimpleLexer_TwoTokens_FirstQuoted_Adjacent()
{
    const char *input = "\"token1\"token2";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 8);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 9, 1, 14);

    return 0;
}

static int TestSimpleLexer_TwoTokens_SecondQuoted_Adjacent()
{
    const char *input = "token1\"token2\"";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 7, 1, 14);

    return 0;
}

static int TestSimpleLexer_UnquotedTokenWithEscapedInnerQuotationMark()
{
    const char *input = "token\\\"token";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token\"token");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 12);

    return 0;
}

static int TestSimpleLexer_QuotedTokenWithEscapedInnerQuotationMark()
{
    const char *input = "\"token\\\"token\"";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token\"token");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 14);

    return 0;
}

static int TestSimpleLexer_QuotedTokenWithEmbeddedNewline()
{
    const char *input = "\"token\ntoken\"";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);
    TEST_ASSERT_STREQ(token.text, "token\ntoken");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 2, 6);

    return 0;
}

static int TestSimpleLexer_TokenSequenceWithQuotedTokenWithEmbeddedNewline()
{
    const char *input = "token1 \"token2\ntoken2\" token3";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2\ntoken2");
    TEST_ASSERT_EQUAL(token.quoted, 1);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 8, 2, 7);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token3");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(2, 9, 2, 14);

    return 0;
}

static int TestSimpleLexer_LeadingWhitespace()
{
    const char *input = " \t\n token";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(2, 2, 2, 6);

    return 0;
}

static int TestSimpleLexer_TrailingWhitespace()
{
    const char *input = "token \t\n ";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 5);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);

    return 0;
}

static int TestSimpleLexer_MiddleWhitespace()
{
    const char *input = "token1 \t\n\t\n\t token2";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(1, 1, 1, 6);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(3, 3, 3, 8);

    return 0;
}

static int TestSimpleLexer_LeadingTrailingAndMiddleWhitespace()
{
    const char *input = " \t\n token1 \t\n\t\n\t token2 \t\n\t ";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token1");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(2, 2, 2, 7);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "token2");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 0);
    TEST_SPAN(4, 3, 4, 8);
    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_EOF);

    return 0;
}

static int TestSimpleLexer_TokenStartedEscaped()
{
    const char *input = "\\mytoken";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
    TEST_FINISH(SIMPLE_LEXER_OK);
    TEST_ASSERT_STREQ(token.text, "mytoken");
    TEST_ASSERT_EQUAL(token.quoted, 0);
    TEST_ASSERT_EQUAL(token.startedEscaped, 1);
    TEST_SPAN(1, 1, 1, 8);

    return 0;
}

static int TestSimpleLexer_CEscapeCharactersProduceAsciiEquivalents()
{
    const char *input = "\\a\\b\\f\\n\\r\\t\\v";
    const size_t len = strlen(input);

    TEST_GET_TOKEN(input, len, SIMPLE_LEXER_EOF);
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
    REGISTER_TEST(TestSimpleLexer_Empty),
    REGISTER_TEST(TestSimpleLexer_OneUnquotedToken),
    REGISTER_TEST(TestSimpleLexer_LexAfterFinishReturnsEof),
    REGISTER_TEST(TestSimpleLexer_FinishAfterFinishReturnsEof),
    REGISTER_TEST(TestSimpleLexer_TwoUnquotedTokens),
    REGISTER_TEST(TestSimpleLexer_TwoTokens_FirstQuoted),
    REGISTER_TEST(TestSimpleLexer_TwoTokens_SecondQuoted),
    REGISTER_TEST(TestSimpleLexer_TwoQuotedTokens_WithSpace),
    REGISTER_TEST(TestSimpleLexer_TwoQuotedTokens_Adjacent),
    REGISTER_TEST(TestSimpleLexer_TwoTokens_FirstQuoted_Adjacent),
    REGISTER_TEST(TestSimpleLexer_TwoTokens_SecondQuoted_Adjacent),
    REGISTER_TEST(TestSimpleLexer_UnquotedTokenWithEscapedInnerQuotationMark),
    REGISTER_TEST(TestSimpleLexer_QuotedTokenWithEscapedInnerQuotationMark),
    REGISTER_TEST(TestSimpleLexer_QuotedTokenWithEmbeddedNewline),
    REGISTER_TEST(TestSimpleLexer_TokenSequenceWithQuotedTokenWithEmbeddedNewline),
    REGISTER_TEST(TestSimpleLexer_LeadingWhitespace),
    REGISTER_TEST(TestSimpleLexer_TrailingWhitespace),
    REGISTER_TEST(TestSimpleLexer_MiddleWhitespace),
    REGISTER_TEST(TestSimpleLexer_LeadingTrailingAndMiddleWhitespace),
    REGISTER_TEST(TestSimpleLexer_TokenStartedEscaped),
    REGISTER_TEST(TestSimpleLexer_CEscapeCharactersProduceAsciiEquivalents),
    { NULL, NULL }
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

