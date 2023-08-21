/*
 * tests.h
 *
 * Author:    Ryan M. Lederman <lederman@gmail.com>
 * Copyright: Copyright (c) 2004-2023
 * Version:   0.2.0
 * License:   The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _BAL_TESTS_H_INCLUDED
# define _BAL_TESTS_H_INCLUDED
#include "bal.h"

/******************************************************************************\
 *                               Types & Macros                               *
\******************************************************************************/

/** Test function pointer type. */
typedef bool (*bal_test_func)(void);

/** Test metadata container type. */
typedef struct {
    const char* const name;
    bal_test_func func;
} bal_test_data;

/******************************************************************************\
 *                              Test Definitions                              *
\******************************************************************************/

/**
 * @test baltest_init_cleanup_sanity
 * Ensures that libbal behaves correctly under various circumstances with regards
 * to order and number of init/cleanup operations.
 */
bool baltest_init_cleanup_sanity(void);

/**
 * @test baltest_error_sanity
 * Ensures that libbal properly handles reported errors and returns the expected
 * error codes and messages for each known error code.
 */
bool baltest_error_sanity(void);

/******************************************************************************\
 *                                Test Harness                                *
\******************************************************************************/

void _bal_print(int text_attr, int fg_color, int bg_color, const char* format, ...);
void _bal_infomsg(const char* format, ...);

# define _ESC "\x1b[" /**< Begins an ANSI escape sequence. */
# define _ESC_M "m"   /**< Marks the end of a sequence. */

# define _ESC_SEQ(codes, s) _ESC codes _ESC_M s
# define _ESC_SEQE(codes)   _ESC_SEQ(codes, "")

/** Resets all previously applied colors and effects/attributes. */
# define _ESC_RST _ESC_SEQE("0")

/** Allows for setting of the attributes, foreground, and background colors
 * simultaneously. */
# undef COLOR
# define COLOR(attr, fg, bg, s) \
    _ESC_SEQ(#attr ";38;5;" #fg ";48;5;" #bg, s) _ESC_RST

/** Sets only the foreground color, setting background to its default. */
# undef FG_COLOR
# define FG_COLOR(attr, fg, s) \
    _ESC_SEQ(#attr ";38;5;" #fg ";49;5;49", s) _ESC_RST

/** Sets only the background color, setting foreground to its default. */
# undef BG_COLOR
# define BG_COLOR(attr, bg, s) \
    _ESC_SEQ(#attr ";39;5;39;48;5;" #bg, s) _ESC_RST

# undef ULINE
# define ULINE(s) _ESC_SEQ("4", s) _ESC_SEQE("24") /**< Underlined. */
# undef EMPH
# define EMPH(s)  _ESC_SEQ("3", s) _ESC_SEQE("23") /**< Emphasis/italic. */
# undef BOLD
# define BOLD(s)  _ESC_SEQ("1", s) _ESC_SEQE("22") /**< Bold. */

# define BLACK(s)     COLOR(0, 0, 49, s) /**< Black foreground text. */
# define BLACKB(s)    COLOR(1, 0, 49, s) /**< Bold black foreground text. */

# define RED(s)       COLOR(0, 1, 49, s) /**< Red foreground text. */
# define REDB(s)      COLOR(1, 1, 49, s) /**< Bold red foreground text. */

# define BRED(s)      COLOR(0, 9, 49, s) /**< Bright red foreground text. */
# define BREDB(s)     COLOR(1, 9, 49, s) /**< Bold bright red foreground text. */

# define GREEN(s)     COLOR(0, 2, 49, s) /**< Green foreground text. */
# define GREENB(s)    COLOR(1, 2, 49, s) /**< Bold green foreground text. */

# define BGREEN(s)    COLOR(0, 10, 49, s) /**< Bright green foreground text. */
# define BGREENB(s)   COLOR(1, 10, 49, s) /**< Bold bright green foreground text. */

# define YELLOW(s)    COLOR(0, 3, 49, s) /**< Yellow foreground text. */
# define YELLOWB(s)   COLOR(1, 3, 49, s) /**< Bold yellow foreground text. */

# define BYELLOW(s)   COLOR(0, 11, 49, s) /**< Bright yellow foreground text. */
# define BYELLOWB(s)  COLOR(1, 11, 49, s) /**< Bold bright yellow foreground text. */

# define BLUE(s)      COLOR(0, 4, 49, s) /**< Blue foreground text. */
# define BLUEB(s)     COLOR(1, 4, 49, s) /**< Bold blue foreground text. */

# define BBLUE(s)     COLOR(0, 12, 49, s) /**< Bright blue foreground text. */
# define BBLUEB(s)    COLOR(1, 12, 49, s) /**< Bold bright blue foreground text. */

# define MAGENTA(s)   COLOR(0, 5, 49, s) /**< Magenta foreground text. */
# define MAGENTAB(s)  COLOR(1, 5, 49, s) /**< Bold magenta foreground text. */

# define BMAGENTA(s)  COLOR(0, 13, 49, s) /**< Bright magenta foreground text. */
# define BMAGENTAB(s) COLOR(1, 13, 49, s) /**< Bold bright magenta foreground text. */

# define CYAN(s)      COLOR(0, 6, 49, s) /**< Cyan foreground text. */
# define CYANB(s)     COLOR(1, 6, 49, s) /**< Bold cyan foreground text. */

# define BCYAN(s)     COLOR(0, 13, 49, s) /**< Bright cyan foreground text. */
# define BCYANB(s)    COLOR(1, 13, 49, s) /**< Bold bright cyan foreground text. */

# define BGRAY(s)     COLOR(0, 7, 49, s) /**< Bright gray foreground text. */
# define BGRAYB(s)    COLOR(1, 7, 49, s) /**< Bold bright gray foreground text. */

# define DGRAY(s)     COLOR(0, 8, 49, s) /**< Dark gray foreground text. */
# define DGRAYB(s)    COLOR(1, 8, 49, s) /**< Bold dark gray foreground text. */

# define WHITE(s)     COLOR(0, 15, 49, s) /**< White foreground text. */
# define WHITEB(s)    COLOR(1, 15, 49, s) /**< Bold white foreground text. */

#endif /* !_BAL_TESTS_H_INCLUDED */
