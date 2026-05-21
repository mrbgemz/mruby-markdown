/*
 * md4c.h — md4c public API
 * https://github.com/mity/md4c
 *
 * md4c is released under the MIT license.
 * See the file LICENSE for details.
 */
#ifndef MD4C_H__
#define MD4C_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Block types */
#define MD_BLOCK_DOC             0
#define MD_BLOCK_QUOTE           1
#define MD_BLOCK_UL              2
#define MD_BLOCK_OL              3
#define MD_BLOCK_LI              4
#define MD_BLOCK_HR              5
#define MD_BLOCK_H               6
#define MD_BLOCK_CODE            7
#define MD_BLOCK_HTML            8
#define MD_BLOCK_P               9
#define MD_BLOCK_TABLE           10
#define MD_BLOCK_THEAD           11
#define MD_BLOCK_TBODY           12
#define MD_BLOCK_TR              13
#define MD_BLOCK_TH              14
#define MD_BLOCK_TD              15

/* Span types */
#define MD_SPAN_EM               1
#define MD_SPAN_STRONG           2
#define MD_SPAN_A                3
#define MD_SPAN_IMG              4
#define MD_SPAN_CODE             5
#define MD_SPAN_DEL              6
#define MD_SPAN_LATEXMATH        7
#define MD_SPAN_LATEXMATH_DISPLAY 8
#define MD_SPAN_WIKILINK         9
#define MD_SPAN_U                10

/* Text types */
#define MD_TEXT_NORMAL           0
#define MD_TEXT_NULLCHAR         1
#define MD_TEXT_BR              2
#define MD_TEXT_SOFTBR           3
#define MD_TEXT_ENTITY           4
#define MD_TEXT_CODE             5

/* Flags */
#define MD_FLAG_COLLAPSEWHITESPACE  0x0001
#define MD_FLAG_PERMISSIVEATXHEADERS 0x0002
#define MD_FLAG_PERMISSIVEURLAUTOLINKS 0x0004
#define MD_FLAG_PERMISSIVEEMAILAUTOLINKS 0x0008
#define MD_FLAG_NOINDENTEDCODEBLOCKS 0x0010
#define MD_FLAG_NOHTMLBLOCKS      0x0020
#define MD_FLAG_NOHTMLSPANS       0x0040
#define MD_FLAG_TABLES            0x0100
#define MD_FLAG_STRIKETHROUGH     0x0200
#define MD_FLAG_PERMISSIVEWWWAUTOLINKS 0x0400
#define MD_FLAG_TASKLISTS         0x0800
#define MD_FLAG_LATEXMATH         0x1000
#define MD_FLAG_WIKILINKS         0x2000
#define MD_FLAG_UNDERLINE         0x4000

/* Dialect */
#define MD_DIALECT_COMMONMARK     0

typedef struct MD_BLOCK_TAG {
    int type;
    unsigned long long align;
    int n_cols;
    int level;
} MD_BLOCK;

typedef struct MD_SPAN_TAG {
    int type;
    int size;
    const char* href;
    const char* title;
    const char* label;
    const char* text;
    int child_count;
    unsigned long long flags;
} MD_SPAN;

typedef struct MD_TEXTTAG {
    int type;
    const char* text;
    size_t size;
} MD_TEXT;

typedef void (*MD_ENTER_BLOCK)(const MD_BLOCK* block, void* userdata);
typedef void (*MD_LEAVE_BLOCK)(const MD_BLOCK* block, void* userdata);
typedef void (*MD_ENTER_SPAN)(const MD_SPAN* span, void* userdata);
typedef void (*MD_LEAVE_SPAN)(const MD_SPAN* span, void* userdata);
typedef void (*MD_TEXT_CB)(const MD_TEXT* text, void* userdata);

typedef struct MD_PARSER {
    int flags;
    int dialect;
    MD_ENTER_BLOCK enter_block;
    MD_LEAVE_BLOCK leave_block;
    MD_ENTER_SPAN enter_span;
    MD_LEAVE_SPAN leave_span;
    MD_TEXT_CB text;
    void* userdata;
    size_t reserved;
} MD_PARSER;

int md_parse(const char* text, size_t size, const MD_PARSER* parser, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* MD4C_H__ */
