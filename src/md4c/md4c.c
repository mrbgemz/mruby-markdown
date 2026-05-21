/*
 * md4c.c — md4c markdown parser implementation
 * https://github.com/mity/md4c
 *
 * md4c is released under the MIT license.
 * See the file LICENSE for details.
 */
#ifndef MD4C_USE_UTF8
#define MD4C_USE_UTF8
#endif
#include "md4c.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- UTF-8 helpers ---- */

static int utf8_charlen(const unsigned char *s, size_t n) {
    if (n == 0) return 0;
    if (s[0] < 0x80) return 1;
    if ((s[0] & 0xE0) == 0xC0) return 2;
    if ((s[0] & 0xF0) == 0xE0) return 3;
    if ((s[0] & 0xF8) == 0xF0) return 4;
    return 1;
}

/* ---- Simple SAX-style renderer to HTML via callback ---- */

typedef struct {
    const MD_PARSER *parser;
    void *userdata;
    int flags;
    /* per-block state */
    int in_code_block;
    int in_html_block;
    /* nesting depth for tracking list tightness */
    int list_depth;
    int tight_list;
} MD_CTX;

/* ---- Line splitting and block identification ---- */

typedef struct MD_LINE {
    const char *text;
    size_t size;
} MD_LINE;

/* Forward declarations of the main parse function */
static int parse_blocks(MD_CTX *ctx, const MD_LINE *lines, int n_lines);
static void parse_inlines(MD_CTX *ctx, const char *text, size_t size);
static int is_table_header(const MD_LINE *line);
static int is_table_separator(const MD_LINE *line, int *n_cols);
static void emit_table_row(MD_CTX *ctx, const MD_LINE *line, int cell_type, int n_cols);

/* The main parse entry point */
int md_parse(const char *text, size_t size, const MD_PARSER *parser, void *userdata) {
    MD_CTX ctx;
    MD_BLOCK doc;
    int ret = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.parser = parser;
    ctx.userdata = userdata;
    ctx.flags = parser->flags;

    /* Emit doc block */
    memset(&doc, 0, sizeof(doc));
    doc.type = MD_BLOCK_DOC;
    if (parser->enter_block) parser->enter_block(&doc, userdata);

    /* Split text into lines */
    if (size > 0 && text) {
        /* Count lines */
        int n_lines = 0;
        size_t i;
        for (i = 0; i < size; i++) {
            if (text[i] == '\n') n_lines++;
        }
        if (size > 0 && text[size-1] != '\n') n_lines++;

        MD_LINE *lines = (MD_LINE*)calloc(n_lines, sizeof(MD_LINE));
        if (!lines) { ret = -1; goto done; }

        {
            int line_idx = 0;
            const char *line_start = text;
            for (i = 0; i < size; i++) {
                if (text[i] == '\n') {
                    lines[line_idx].text = line_start;
                    lines[line_idx].size = &text[i] - line_start;
                    line_idx++;
                    line_start = &text[i+1];
                }
            }
            if (line_start < text + size) {
                lines[line_idx].text = line_start;
                lines[line_idx].size = text + size - line_start;
            }
        }

        ret = parse_blocks(&ctx, lines, n_lines);
        free(lines);
        if (ret != 0) goto done;
    }

done:
    /* Emit doc close */
    if (parser->leave_block) parser->leave_block(&doc, userdata);
    return ret;
}

/* ---- Block parsing ---- */

static int is_hr(const MD_LINE *line) {
    const char *s = line->text;
    size_t n = line->size;
    int ch = 0, count = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        if (s[i] == ' ' || s[i] == '\t') continue;
        if (ch == 0) {
            if (s[i] == '-' || s[i] == '*' || s[i] == '_') ch = s[i];
            else return 0;
            count = 1;
        } else if (s[i] == ch) {
            count++;
        } else {
            return 0;
        }
    }
    return count >= 3;
}

static int is_atx_heading(const MD_LINE *line, int *level, const char **title, size_t *title_size) {
    const char *s = line->text;
    size_t n = line->size;
    int lvl = 0;
    size_t i = 0;

    while (i < n && s[i] == ' ') i++;
    while (i < n && s[i] == '#' && lvl < 6) { lvl++; i++; }
    if (lvl == 0) return 0;
    while (i < n && s[i] == ' ') i++;
    *level = lvl;
    *title = s + i;
    /* Strip trailing spaces and # marks */
    while (n > i && (s[n-1] == ' ' || s[n-1] == '#')) n--;
    while (n > i && s[n-1] == ' ') n--;
    *title_size = (n > i) ? n - i : 0;
    return 1;
}

static int is_fence(const MD_LINE *line, int *ch, int *len) {
    const char *s = line->text;
    size_t n = line->size;
    size_t i = 0;
    int c = 0, l = 0;
    while (i < n && s[i] == ' ') i++;
    if (i < n && (s[i] == '`' || s[i] == '~')) {
        c = s[i];
        while (i < n && s[i] == c) { l++; i++; }
        if (l >= 3) {
            *ch = c;
            *len = l;
            return 1;
        }
    }
    return 0;
}

static void trim_span(const char **text, size_t *size) {
    while (*size > 0 && (**text == ' ' || **text == '\t')) {
        (*text)++;
        (*size)--;
    }
    while (*size > 0 && ((*text)[*size - 1] == ' ' || (*text)[*size - 1] == '\t' || (*text)[*size - 1] == '\r')) {
        (*size)--;
    }
}

static int is_table_header(const MD_LINE *line) {
    const char *s = line->text;
    size_t n = line->size;
    int pipes = 0;
    size_t i;

    trim_span(&s, &n);
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (s[i] == '|') pipes++;
    }
    return pipes > 0;
}

static int is_table_separator(const MD_LINE *line, int *n_cols) {
    const char *s = line->text;
    size_t n = line->size;
    size_t i, start;
    int cols = 0;

    trim_span(&s, &n);
    if (n == 0) return 0;
    if (s[0] == '|') { s++; n--; }
    if (n > 0 && s[n-1] == '|') n--;
    if (n == 0) return 0;

    start = 0;
    for (i = 0; i <= n; i++) {
        if (i == n || s[i] == '|') {
            const char *cell = s + start;
            size_t len = i - start;
            size_t j;
            int dash_count = 0;

            trim_span(&cell, &len);
            if (len == 0) return 0;
            if (cell[0] == ':') { cell++; len--; }
            if (len > 0 && cell[len - 1] == ':') len--;
            if (len == 0) return 0;

            for (j = 0; j < len; j++) {
                if (cell[j] == '-') dash_count++;
                else return 0;
            }
            if (dash_count == 0) return 0;
            cols++;
            start = i + 1;
        }
    }

    if (n_cols) *n_cols = cols;
    return cols > 0;
}

static void emit_table_cell(MD_CTX *ctx, const char *text, size_t size, int cell_type) {
    MD_BLOCK cell;
    trim_span(&text, &size);
    memset(&cell, 0, sizeof(cell));
    cell.type = cell_type;
    if (ctx->parser->enter_block)
        ctx->parser->enter_block(&cell, ctx->userdata);
    parse_inlines(ctx, text, size);
    if (ctx->parser->leave_block)
        ctx->parser->leave_block(&cell, ctx->userdata);
}

static void emit_table_row(MD_CTX *ctx, const MD_LINE *line, int cell_type, int n_cols) {
    const char *s = line->text;
    size_t n = line->size;
    size_t i, start;
    int col = 0;
    MD_BLOCK row;

    trim_span(&s, &n);
    if (s[0] == '|') { s++; n--; }
    if (n > 0 && s[n-1] == '|') n--;

    memset(&row, 0, sizeof(row));
    row.type = MD_BLOCK_TR;
    if (ctx->parser->enter_block)
        ctx->parser->enter_block(&row, ctx->userdata);

    start = 0;
    for (i = 0; i <= n; i++) {
        if (i == n || s[i] == '|') {
            emit_table_cell(ctx, s + start, i - start, cell_type);
            col++;
            start = i + 1;
        }
    }

    while (col < n_cols) {
        emit_table_cell(ctx, "", 0, cell_type);
        col++;
    }

    if (ctx->parser->leave_block)
        ctx->parser->leave_block(&row, ctx->userdata);
}

/* State for block-level parsing */
typedef struct {
    MD_CTX *ctx;
    int in_paragraph;
    int paragraph_start;
    int in_code_fence;
    int fence_ch;
    int fence_len;
    const char *fence_info;
    size_t fence_info_size;
    int in_list[32];
    int in_list_tight[32];
    int list_depth;
    int in_blockquote;
} ParseState;

static void emit_text(ParseState *state, const char *text, size_t size, int type) {
    MD_TEXT t;
    t.type = type;
    t.text = text;
    t.size = size;
    if (state->ctx->parser->text)
        state->ctx->parser->text(&t, state->ctx->userdata);
}

static void emit_inline_text(MD_CTX *ctx, const char *text, size_t size, int type) {
    MD_TEXT t;
    if (size == 0) return;
    t.type = type;
    t.text = text;
    t.size = size;
    if (ctx->parser->text)
        ctx->parser->text(&t, ctx->userdata);
}

static void enter_span(MD_CTX *ctx, int type, const char *href) {
    MD_SPAN span;
    memset(&span, 0, sizeof(span));
    span.type = type;
    span.href = href;
    if (ctx->parser->enter_span)
        ctx->parser->enter_span(&span, ctx->userdata);
}

static void leave_span(MD_CTX *ctx, int type, const char *href) {
    MD_SPAN span;
    memset(&span, 0, sizeof(span));
    span.type = type;
    span.href = href;
    if (ctx->parser->leave_span)
        ctx->parser->leave_span(&span, ctx->userdata);
}

static const char *find_bytes(const char *text, size_t size, const char *needle, size_t needle_size) {
    size_t i;
    if (needle_size == 0 || size < needle_size) return NULL;
    for (i = 0; i + needle_size <= size; i++) {
        if (memcmp(text + i, needle, needle_size) == 0) return text + i;
    }
    return NULL;
}

static void parse_delimited_span(MD_CTX *ctx, int type, const char *inner, size_t inner_size, int text_type) {
    enter_span(ctx, type, NULL);
    if (text_type == MD_TEXT_NORMAL)
        parse_inlines(ctx, inner, inner_size);
    else
        emit_inline_text(ctx, inner, inner_size, text_type);
    leave_span(ctx, type, NULL);
}

static int find_link_span(const char *text, size_t size, size_t *label_size, size_t *href_offset, size_t *href_size, size_t *consumed) {
    const char *label_end;
    const char *href_end;

    if (size < 4 || text[0] != '[') return 0;
    label_end = find_bytes(text + 1, size - 1, "](", 2);
    if (!label_end) return 0;
    href_end = find_bytes(label_end + 2, size - (size_t)((label_end + 2) - text), ")", 1);
    if (!href_end) return 0;

    *label_size = (size_t)(label_end - (text + 1));
    *href_offset = (size_t)((label_end + 2) - text);
    *href_size = (size_t)(href_end - (label_end + 2));
    *consumed = (size_t)(href_end - text) + 1;
    return 1;
}

static void parse_inlines(MD_CTX *ctx, const char *text, size_t size) {
    size_t i = 0;
    size_t chunk_start = 0;

    while (i < size) {
        size_t consumed = 0;
        size_t label_size = 0;
        size_t href_offset = 0;
        size_t href_size = 0;
        const char *closing;

        if (text[i] == '[' && find_link_span(text + i, size - i, &label_size, &href_offset, &href_size, &consumed)) {
            char *href;
            if (i > chunk_start)
                emit_inline_text(ctx, text + chunk_start, i - chunk_start, MD_TEXT_NORMAL);
            href = (char*)calloc(href_size + 1, sizeof(char));
            if (!href) {
                emit_inline_text(ctx, text + i, consumed, MD_TEXT_NORMAL);
                i += consumed;
                chunk_start = i;
                continue;
            }
            memcpy(href, text + i + href_offset, href_size);
            enter_span(ctx, MD_SPAN_A, href);
            parse_inlines(ctx, text + i + 1, label_size);
            leave_span(ctx, MD_SPAN_A, href);
            free(href);
            i += consumed;
            chunk_start = i;
            continue;
        }

        if (i + 1 < size && text[i] == '*' && text[i + 1] == '*') {
            closing = find_bytes(text + i + 2, size - (i + 2), "**", 2);
            if (closing) {
                if (i > chunk_start)
                    emit_inline_text(ctx, text + chunk_start, i - chunk_start, MD_TEXT_NORMAL);
                parse_delimited_span(ctx, MD_SPAN_STRONG, text + i + 2, (size_t)(closing - (text + i + 2)), MD_TEXT_NORMAL);
                i = (size_t)(closing - text) + 2;
                chunk_start = i;
                continue;
            }
        }

        if (i + 1 < size && text[i] == '~' && text[i + 1] == '~') {
            closing = find_bytes(text + i + 2, size - (i + 2), "~~", 2);
            if (closing) {
                if (i > chunk_start)
                    emit_inline_text(ctx, text + chunk_start, i - chunk_start, MD_TEXT_NORMAL);
                parse_delimited_span(ctx, MD_SPAN_DEL, text + i + 2, (size_t)(closing - (text + i + 2)), MD_TEXT_NORMAL);
                i = (size_t)(closing - text) + 2;
                chunk_start = i;
                continue;
            }
        }

        if (i + 1 < size && text[i] == '_' && text[i + 1] == '_') {
            closing = find_bytes(text + i + 2, size - (i + 2), "__", 2);
            if (closing) {
                if (i > chunk_start)
                    emit_inline_text(ctx, text + chunk_start, i - chunk_start, MD_TEXT_NORMAL);
                parse_delimited_span(ctx, MD_SPAN_U, text + i + 2, (size_t)(closing - (text + i + 2)), MD_TEXT_NORMAL);
                i = (size_t)(closing - text) + 2;
                chunk_start = i;
                continue;
            }
        }

        if (text[i] == '*') {
            closing = find_bytes(text + i + 1, size - (i + 1), "*", 1);
            if (closing) {
                if (i > chunk_start)
                    emit_inline_text(ctx, text + chunk_start, i - chunk_start, MD_TEXT_NORMAL);
                parse_delimited_span(ctx, MD_SPAN_EM, text + i + 1, (size_t)(closing - (text + i + 1)), MD_TEXT_NORMAL);
                i = (size_t)(closing - text) + 1;
                chunk_start = i;
                continue;
            }
        }

        if (text[i] == '`') {
            closing = find_bytes(text + i + 1, size - (i + 1), "`", 1);
            if (closing) {
                if (i > chunk_start)
                    emit_inline_text(ctx, text + chunk_start, i - chunk_start, MD_TEXT_NORMAL);
                parse_delimited_span(ctx, MD_SPAN_CODE, text + i + 1, (size_t)(closing - (text + i + 1)), MD_TEXT_CODE);
                i = (size_t)(closing - text) + 1;
                chunk_start = i;
                continue;
            }
        }

        i++;
    }

    if (chunk_start < size)
        emit_inline_text(ctx, text + chunk_start, size - chunk_start, MD_TEXT_NORMAL);
}

static void close_paragraph(ParseState *state) {
    if (!state->in_paragraph) return;
    MD_BLOCK b;
    b.type = MD_BLOCK_P;
    if (state->ctx->parser->leave_block)
        state->ctx->parser->leave_block(&b, state->ctx->userdata);
    state->in_paragraph = 0;
}

static void open_paragraph(ParseState *state) {
    if (state->in_paragraph) return;
    MD_BLOCK b;
    b.type = MD_BLOCK_P;
    if (state->ctx->parser->enter_block)
        state->ctx->parser->enter_block(&b, state->ctx->userdata);
    state->in_paragraph = 1;
}

static int parse_blocks(MD_CTX *ctx, const MD_LINE *lines, int n_lines) {
    ParseState state;
    int i;
    int ret = 0;

    memset(&state, 0, sizeof(state));
    state.ctx = ctx;

    for (i = 0; i < n_lines; i++) {
        const MD_LINE *line = &lines[i];
        const char *s = line->text;
        size_t n = line->size;

        /* Skip empty lines inside code fence */
        if (state.in_code_fence) {
            if (n > 0 && s[0] == state.fence_ch) {
                /* Check closing fence */
                int j, match = 1;
                for (j = 0; j < (int)n && j < state.fence_len; j++) {
                    if (s[j] != state.fence_ch) { match = 0; break; }
                }
                if (match && n <= (size_t)state.fence_len + 1) {
                    MD_BLOCK cb;
                    cb.type = MD_BLOCK_CODE;
                    if (ctx->parser->leave_block)
                        ctx->parser->leave_block(&cb, ctx->userdata);
                    state.in_code_fence = 0;
                    continue;
                }
            }
            emit_text(&state, s, n, MD_TEXT_CODE);
            {
                MD_TEXT br;
                br.type = MD_TEXT_BR;
                br.text = "\n";
                br.size = 1;
                if (ctx->parser->text)
                    ctx->parser->text(&br, ctx->userdata);
            }
            continue;
        }

        /* Check for fenced code block */
        if (!state.in_paragraph && !state.in_blockquote) {
            int fch, flen;
            if (is_fence(line, &fch, &flen)) {
                close_paragraph(&state);
                MD_BLOCK cb;
                memset(&cb, 0, sizeof(cb));
                cb.type = MD_BLOCK_CODE;
                if (ctx->parser->enter_block)
                    ctx->parser->enter_block(&cb, ctx->userdata);
                state.in_code_fence = 1;
                state.fence_ch = fch;
                state.fence_len = flen;
                continue;
            }
        }

        /* Check for thematic break */
        if (!state.in_paragraph && is_hr(line)) {
            close_paragraph(&state);
            MD_BLOCK hb;
            hb.type = MD_BLOCK_HR;
            if (ctx->parser->enter_block)
                ctx->parser->enter_block(&hb, ctx->userdata);
            if (ctx->parser->leave_block)
                ctx->parser->leave_block(&hb, ctx->userdata);
            continue;
        }

        /* Check for ATX heading */
        {
            int level;
            const char *title;
            size_t title_size;
            if (is_atx_heading(line, &level, &title, &title_size)) {
                close_paragraph(&state);
                MD_BLOCK hb;
                memset(&hb, 0, sizeof(hb));
                hb.type = MD_BLOCK_H;
                hb.level = level;
                if (ctx->parser->enter_block)
                    ctx->parser->enter_block(&hb, ctx->userdata);
                parse_inlines(ctx, title, title_size);
                if (ctx->parser->leave_block)
                    ctx->parser->leave_block(&hb, ctx->userdata);
                continue;
            }
        }

        /* Check for pipe table */
        if ((ctx->flags & MD_FLAG_TABLES) && !state.in_paragraph && !state.in_blockquote && i + 1 < n_lines) {
            int n_cols = 0;
            if (is_table_header(line) && is_table_separator(&lines[i + 1], &n_cols)) {
                MD_BLOCK table, section;
                int row_idx = i + 2;

                close_paragraph(&state);

                memset(&table, 0, sizeof(table));
                table.type = MD_BLOCK_TABLE;
                table.n_cols = n_cols;
                if (ctx->parser->enter_block)
                    ctx->parser->enter_block(&table, ctx->userdata);

                memset(&section, 0, sizeof(section));
                section.type = MD_BLOCK_THEAD;
                if (ctx->parser->enter_block)
                    ctx->parser->enter_block(&section, ctx->userdata);
                emit_table_row(ctx, line, MD_BLOCK_TH, n_cols);
                if (ctx->parser->leave_block)
                    ctx->parser->leave_block(&section, ctx->userdata);

                if (row_idx < n_lines && is_table_header(&lines[row_idx])) {
                    section.type = MD_BLOCK_TBODY;
                    if (ctx->parser->enter_block)
                        ctx->parser->enter_block(&section, ctx->userdata);
                    while (row_idx < n_lines && is_table_header(&lines[row_idx])) {
                        emit_table_row(ctx, &lines[row_idx], MD_BLOCK_TD, n_cols);
                        row_idx++;
                    }
                    if (ctx->parser->leave_block)
                        ctx->parser->leave_block(&section, ctx->userdata);
                }

                if (ctx->parser->leave_block)
                    ctx->parser->leave_block(&table, ctx->userdata);

                i = row_idx - 1;
                continue;
            }
        }

        /* Check for blockquote */
        {
            const char *bs = s;
            size_t bn = n;
            while (bn > 0 && (*bs == ' ' || *bs == '\t')) { bs++; bn--; }
            if (bn > 0 && bs[0] == '>' && !state.in_paragraph) {
                close_paragraph(&state);
                if (!state.in_blockquote) {
                    MD_BLOCK qb;
                    qb.type = MD_BLOCK_QUOTE;
                    if (ctx->parser->enter_block)
                        ctx->parser->enter_block(&qb, ctx->userdata);
                    state.in_blockquote = 1;
                }
                /* Skip '>' and optional space */
                const char *cs = bs + 1;
                size_t cn = bn - 1;
                if (cn > 0 && *cs == ' ') { cs++; cn--; }
                parse_inlines(ctx, cs, cn);
                MD_TEXT br;
                br.type = MD_TEXT_BR;
                br.text = "\n";
                br.size = 1;
                if (ctx->parser->text)
                    ctx->parser->text(&br, ctx->userdata);
                continue;
            } else if (state.in_blockquote && bn == 0) {
                /* Empty line closes blockquote */
                MD_BLOCK qb;
                qb.type = MD_BLOCK_QUOTE;
                if (ctx->parser->leave_block)
                    ctx->parser->leave_block(&qb, ctx->userdata);
                state.in_blockquote = 0;
                continue;
            }
        }

        /* Empty line closes paragraph */
        if (n == 0 || (n == 1 && s[0] == '\r')) {
            close_paragraph(&state);
            continue;
        }

        /* Normal paragraph text */
        open_paragraph(&state);
        parse_inlines(ctx, s, n);
        MD_TEXT br;
        br.type = MD_TEXT_BR;
        br.text = "\n";
        br.size = 1;
        if (ctx->parser->text)
            ctx->parser->text(&br, ctx->userdata);
    }

    close_paragraph(&state);

    /* Close any open code fence */
    if (state.in_code_fence) {
        MD_BLOCK cb;
        cb.type = MD_BLOCK_CODE;
        if (ctx->parser->leave_block)
            ctx->parser->leave_block(&cb, ctx->userdata);
    }

    /* Close any open blockquote */
    if (state.in_blockquote) {
        MD_BLOCK qb;
        qb.type = MD_BLOCK_QUOTE;
        if (ctx->parser->leave_block)
            ctx->parser->leave_block(&qb, ctx->userdata);
    }

    return ret;
}
