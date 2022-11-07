#include "helper.h"
#include "quickjs.h"
#include "lre_exec_backtrack.h"

typedef uintptr_t StackInt;

// == realloc fp == //
typedef int (*wasm_js_realloc)(int func_offset, int state, int ptr, int size, char* closure);
wasm_js_realloc jr;
void* wasm_js_realloc_closure;
void register_wasm_js_realloc(wasm_js_realloc func, void* closure) {
    jr = func;
    wasm_js_realloc_closure = closure;
}
// == end == //
// == realloc fp == //
typedef int (*wasm_js_realloc_def)(int state, int ptr, int size, char* closure);
wasm_js_realloc_def jrd;
void* wasm_js_realloc_def_closure;
void register_wasm_js_realloc_def(wasm_js_realloc_def func, void* closure) {
    jrd = func;
    wasm_js_realloc_def_closure = closure;
}
// == end == //

int my_lre_realloc(int m, int s, int ptr, size_t size) {
    //JSMallocFunctions *mf = transfer_i32_to_ptr(m);
    //return jr(((JSMallocFunctions *)transfer_i32_to_ptr(m))->js_realloc, s, ptr, size, wasm_js_realloc_closure);
    return jrd(s, ptr, size, wasm_js_realloc_def_closure);
    //return mf.js_realloc(s, ptr, size);
}

int push_state(
    int malloc_function_wasm,
    int malloc_state_wasm,
    int s_wasm,
    int capture_wasm,
    int stack_wasm,
    size_t stack_len,
    int pc_wasm, 
    int cptr_wasm,
    REExecStateEnum type, 
    size_t count)
{
    REExecContext *s = transfer_i32_to_ptr(s_wasm);

    REExecState *rs;
    uint8_t *new_stack;
    size_t new_size, i, n;
    StackInt *stack_buf;
    //printf("len: %d size: %d\n", s->state_stack_len, s->state_stack_size);
    if (unlikely((s->state_stack_len + 1) > s->state_stack_size)) {
        /* reallocate the stack */
        new_size = s->state_stack_size * 3 / 2;
        if (new_size < 8)
            new_size = 8;
        // int my_lre_realloc(int m, int s, int ptr, size_t size)
        int new_stack_offset = my_lre_realloc(malloc_function_wasm, malloc_state_wasm, s->state_stack, new_size * s->state_size);
        //new_stack = transfer_i32_to_ptr(new_stack_offset);
        if (!new_stack_offset)
            //printf("return -1\n");
            return -1;
        s->state_stack_size = new_size;
        s->state_stack = new_stack_offset;
    }
    rs = (REExecState *)((uint8_t *)transfer_i32_to_ptr(s->state_stack) + s->state_stack_len * s->state_size);

    s->state_stack_len++;
    rs->type = type;
    rs->count = count;
    rs->stack_len = stack_len;
    rs->cptr = cptr_wasm;
    rs->pc = pc_wasm;
    n = 2 * s->capture_count;
    int* capture = transfer_i32_to_ptr(capture_wasm);
    for(i = 0; i < n; i++)
        rs->buf[i] = capture[i];
    stack_buf = (StackInt *)(rs->buf + n);
    StackInt *stack = transfer_i32_to_ptr(stack_wasm);
    for(i = 0; i < stack_len; i++)
        stack_buf[i] = stack[i];
    return 0;
}

typedef int (*wasm_lre_case_conv)(int, uint32_t, int, void*);
wasm_lre_case_conv lre_case_conv_func;
void* lre_case_conv_cl;
void register_wasm_lre_case_conv(wasm_lre_case_conv func, void* mc) {
    lre_case_conv_func = func;
    lre_case_conv_cl = mc;
}

int lre_case_conv(uint32_t *res, uint32_t c, int conv_type)
{
    if (c < 128) {
        if (conv_type) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        } else {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
        }
    } else {
        uint32_t v, code, data, type, len, a, is_lower;
        int idx, idx_min, idx_max;
        
        is_lower = (conv_type != 0);
        idx_min = 0;
        idx_max = countof(case_conv_table1) - 1;
        while (idx_min <= idx_max) {
            idx = (unsigned)(idx_max + idx_min) / 2;
            v = case_conv_table1[idx];
            code = v >> (32 - 17);
            len = (v >> (32 - 17 - 7)) & 0x7f;
            if (c < code) {
                idx_max = idx - 1;
            } else if (c >= code + len) {
                idx_min = idx + 1;
            } else {
                type = (v >> (32 - 17 - 7 - 4)) & 0xf;
                data = ((v & 0xf) << 8) | case_conv_table2[idx];
                switch(type) {
                case RUN_TYPE_U:
                case RUN_TYPE_L:
                case RUN_TYPE_UF:
                case RUN_TYPE_LF:
                    if (conv_type == (type & 1) ||
                        (type >= RUN_TYPE_UF && conv_type == 2)) {
                        c = c - code + (case_conv_table1[data] >> (32 - 17));
                    }
                    break;
                case RUN_TYPE_UL:
                    a = c - code;
                    if ((a & 1) != (1 - is_lower))
                        break;
                    c = (a ^ 1) + code;
                    break;
                case RUN_TYPE_LSU:
                    a = c - code;
                    if (a == 1) {
                        c += 2 * is_lower - 1;
                    } else if (a == (1 - is_lower) * 2) {
                        c += (2 * is_lower - 1) * 2;
                    }
                    break;
                case RUN_TYPE_U2L_399_EXT2:
                    if (!is_lower) {
                        res[0] = c - code + case_conv_ext[data >> 6];
                        res[1] = 0x399;
                        return 2;
                    } else {
                        c = c - code + case_conv_ext[data & 0x3f];
                    }
                    break;
                case RUN_TYPE_UF_D20:
                    if (conv_type == 1)
                        break;
                    c = data + (conv_type == 2) * 0x20;
                    break;
                case RUN_TYPE_UF_D1_EXT:
                    if (conv_type == 1)
                        break;
                    c = case_conv_ext[data] + (conv_type == 2);
                    break;
                case RUN_TYPE_U_EXT:
                case RUN_TYPE_LF_EXT:
                    if (is_lower != (type - RUN_TYPE_U_EXT))
                        break;
                    c = case_conv_ext[data];
                    break;
                case RUN_TYPE_U_EXT2:
                case RUN_TYPE_L_EXT2:
                    if (conv_type != (type - RUN_TYPE_U_EXT2))
                        break;
                    res[0] = c - code + case_conv_ext[data >> 6];
                    res[1] = case_conv_ext[data & 0x3f];
                    return 2;
                default:
                case RUN_TYPE_U_EXT3:
                    if (conv_type != 0)
                        break;
                    res[0] = case_conv_ext[data >> 8];
                    res[1] = case_conv_ext[(data >> 4) & 0xf];
                    res[2] = case_conv_ext[data & 0xf];
                    return 3;
                }
                break;
            }
        }
    }
    res[0] = c;
    return 1;
}


uint32_t lre_canonicalize(uint32_t c, BOOL is_utf16) {
    uint32_t res[3];
    int len;
    if (is_utf16) {
        if (likely(c < 128)) {
            if (c >= 'A' && c <= 'Z')
                c = c - 'A' + 'a';
        } else {
            lre_case_conv(res, c, 2);
            c = res[0];
        }
    } else {
        if (likely(c < 128)) {
            if (c >= 'a' && c <= 'z')
                c = c - 'a' + 'A';
        } else {
            /* legacy regexp: to upper case if single char >= 128 */
            len = lre_case_conv(res, c, FALSE);
            if (len == 1 && res[0] >= 128)
                c = res[0];
        }
    }
    return c;
}



intptr_t lre_exec_backtrack(
    int malloc_function_wasm,
    int malloc_state_wasm,
    int s_wasm,
    int capture_wasm, // uint8_t **capture,
    int stack_wasm, // StackInt *stack, 
    int stack_len,
    int pc_wasm, // const uint8_t *pc, 
    int cptr_wasm, // const uint8_t *cptr,
    BOOL no_recurse
) {
    REExecContext *s = transfer_i32_to_ptr(s_wasm);
    int opcode, ret;
    uint32_t val, c;
    
    int cbuf_type = s->cbuf_type;
    const uint8_t *cbuf_end = transfer_i32_to_ptr(s->cbuf_end);
    const uint8_t *pc = transfer_i32_to_ptr(pc_wasm);
    const uint8_t *cptr = transfer_i32_to_ptr(cptr_wasm);
    StackInt *stack = transfer_i32_to_ptr(stack_wasm);

    for(;;) {
        opcode = *pc++; // *(pc++)
        switch(opcode) {
        case REOP_match:
            {
                REExecState *rs;
                if (no_recurse)
                    return (intptr_t)cptr;
                ret = 1;
                goto recurse;
            no_match:
                if (no_recurse)
                    return 0;
                ret = 0;
            recurse:
                for(;;) {
                    if (s->state_stack_len == 0)
                        return ret;
                    rs = (REExecState *)((uint8_t *)transfer_i32_to_ptr(s->state_stack) +
                                         (s->state_stack_len - 1) * s->state_size);
                    if (rs->type == RE_EXEC_STATE_SPLIT) {
                        if (!ret) {
                        pop_state:
                            //memcpy(transfer_i32_to_ptr(capture_wasm), rs->buf, sizeof(int) * 2 * s->capture_count);
                            //int* capture = transfer_i32_to_ptr(capture_wasm);
                            for(int i = 0; i < 2 * s->capture_count; i++) {
                                ((int*)transfer_i32_to_ptr(capture_wasm))[i] = rs->buf[i];
                            }
                        pop_state1:
                            pc = transfer_i32_to_ptr(rs->pc);
                            cptr = transfer_i32_to_ptr(rs->cptr);
                            stack_len = rs->stack_len;
                            // memcpy(stack, rs->buf + 2 * s->capture_count, stack_len * sizeof(stack[0]));
                            // todo: need handle rs->buf?
                            for (int i = 0; i < stack_len; i++) {
                                stack[i] = ((void **)transfer_i32_to_ptr(rs->buf + 2 * s->capture_count))[i];
                            }
                            s->state_stack_len--;
                            break;
                        }
                    } else if (rs->type == RE_EXEC_STATE_GREEDY_QUANT) {
                        if (!ret) {
                            uint32_t char_count, i;
                            int* capture = transfer_i32_to_ptr(capture_wasm);
                            memcpy(capture, rs->buf, sizeof(capture[0]) * 2 * s->capture_count);
                            stack_len = rs->stack_len;
                            memcpy(stack, rs->buf + 2 * s->capture_count, stack_len * sizeof(stack[0]));
                            // for (int i = 0; i < stack_len; i++) {
                            //     stack[i] = ((void **)transfer_i32_to_ptr(rs->buf + 2 * s->capture_count))[i];
                            // }
                            pc = transfer_i32_to_ptr(rs->pc);
                            cptr = transfer_i32_to_ptr(rs->cptr);
                            /* go backward */
                            char_count = get_u32(pc + 12);
                            for(i = 0; i < char_count; i++) {
                                PREV_CHAR(cptr, transfer_i32_to_ptr(s->cbuf));
                            }
                            pc = (pc + 16) + (int)get_u32(pc);
                            rs->cptr = transfer_ptr_to_i32(cptr);
                            rs->count--;
                            if (rs->count == 0) {
                                s->state_stack_len--;
                            }
                            break;
                        }
                    } else {
                        ret = ((rs->type == RE_EXEC_STATE_LOOKAHEAD && ret) ||
                               (rs->type == RE_EXEC_STATE_NEGATIVE_LOOKAHEAD && !ret));
                        if (ret) {
                            /* keep the capture in case of positive lookahead */
                            if (rs->type == RE_EXEC_STATE_LOOKAHEAD)
                                goto pop_state1;
                            else
                                goto pop_state;
                        }
                    }
                    s->state_stack_len--;
                }
            }
            break;
        case REOP_char32:
            val = get_u32(pc);
            pc += 4;
            goto test_char;
        case REOP_char:
            val = get_u16(pc);
            pc += 2;
        test_char:
            if (cptr >= cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end);
            if (s->ignore_case) {
                c = lre_canonicalize(c, s->is_utf16);
            }
            if (val != c)
                goto no_match;
            break;
        case REOP_split_goto_first:
        case REOP_split_next_first:
            {
                const uint8_t *pc1;
                
                val = get_u32(pc);
                pc += 4;
                if (opcode == REOP_split_next_first) {
                    pc1 = pc + (int)val;
                } else {
                    pc1 = pc;
                    pc = pc + (int)val;
                }
                int *capture = transfer_i32_to_ptr(capture_wasm);
                ret = push_state(malloc_function_wasm, malloc_state_wasm, 
                                 transfer_ptr_to_i32(s), capture_wasm, 
                                 transfer_ptr_to_i32(stack), stack_len,
                                 transfer_ptr_to_i32(pc1), transfer_ptr_to_i32(cptr), 
                                 RE_EXEC_STATE_SPLIT, 0);
                if (ret < 0)
                    return -1;
                break;
            }
        case REOP_lookahead:
        case REOP_negative_lookahead:
            val = get_u32(pc);
            pc += 4;
            ret = push_state(malloc_function_wasm, malloc_state_wasm, transfer_ptr_to_i32(s), 
                             capture_wasm, 
                             transfer_ptr_to_i32(stack), stack_len,
                             transfer_ptr_to_i32(pc + (int)val), transfer_ptr_to_i32(cptr),
                             RE_EXEC_STATE_LOOKAHEAD + opcode - REOP_lookahead,
                             0);
            if (ret < 0)
                return -1;
            break;
            
        case REOP_goto:
            val = get_u32(pc);
            pc += 4 + (int)val;
            break;
        case REOP_line_start:
            if (cptr == transfer_i32_to_ptr(s->cbuf))
                break;
            if (!s->multi_line)
                goto no_match;
            PEEK_PREV_CHAR(c, cptr, transfer_i32_to_ptr(s->cbuf));
            if (!is_line_terminator(c))
                goto no_match;
            break;
        case REOP_line_end:
            if (cptr == cbuf_end)
                break;
            if (!s->multi_line)
                goto no_match;
            PEEK_CHAR(c, cptr, cbuf_end);
            if (!is_line_terminator(c))
                goto no_match;
            break;
        case REOP_dot:
            if (cptr == cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end);
            if (is_line_terminator(c))
                goto no_match;
            break;
        case REOP_any:
            if (cptr == cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end);
            break;
        case REOP_save_start:
        case REOP_save_end:
            val = *pc++;
            assert(val < s->capture_count);
            int *capture = transfer_i32_to_ptr(capture_wasm);
            capture[2 * val + opcode - REOP_save_start] = transfer_ptr_to_i32(cptr);
            break;
        case REOP_save_reset:
            {
                uint32_t val2;
                val = pc[0];
                val2 = pc[1];
                pc += 2;
                assert(val2 < s->capture_count);
                while (val <= val2) {
                    capture[2 * val] = NULL;
                    capture[2 * val + 1] = NULL;
                    val++;
                }
            }
            break;
        case REOP_push_i32:
            val = get_u32(pc);
            pc += 4;
            stack[stack_len++] = val;
            break;
        case REOP_drop:
            stack_len--;
            break;
        case REOP_loop:
            val = get_u32(pc);
            pc += 4;
            if (--stack[stack_len - 1] != 0) {
                pc += (int)val;
            }
            break;
        case REOP_push_char_pos:
            stack[stack_len++] = (uintptr_t)cptr;
            break;
        case REOP_bne_char_pos:
            val = get_u32(pc);
            pc += 4;
            if (stack[--stack_len] != (uintptr_t)cptr)
                pc += (int)val;
            break;
        case REOP_word_boundary:
        case REOP_not_word_boundary:
            {
                BOOL v1, v2;
                /* char before */
                if (cptr == transfer_i32_to_ptr(s->cbuf)) {
                    v1 = FALSE;
                } else {
                    PEEK_PREV_CHAR(c, cptr, transfer_i32_to_ptr(s->cbuf));
                    v1 = is_word_char(c);
                }
                /* current char */
                if (cptr >= cbuf_end) {
                    v2 = FALSE;
                } else {
                    PEEK_CHAR(c, cptr, cbuf_end);
                    v2 = is_word_char(c);
                }
                if (v1 ^ v2 ^ (REOP_not_word_boundary - opcode))
                    goto no_match;
            }
            break;
        case REOP_back_reference:
        case REOP_backward_back_reference:
            {
                const uint8_t *cptr1, *cptr1_end, *cptr1_start;
                uint32_t c1, c2;
                
                val = *pc++;
                if (val >= s->capture_count)
                    goto no_match;
                cptr1_start = transfer_i32_to_ptr(capture[2 * val]);
                cptr1_end = transfer_i32_to_ptr(capture[2 * val + 1]);
                if (!cptr1_start || !cptr1_end)
                    break;
                if (opcode == REOP_back_reference) {
                    cptr1 = cptr1_start;
                    while (cptr1 < cptr1_end) {
                        if (cptr >= cbuf_end)
                            goto no_match;
                        GET_CHAR(c1, cptr1, cptr1_end);
                        GET_CHAR(c2, cptr, cbuf_end);
                        if (s->ignore_case) {
                            c1 = lre_canonicalize(c1, s->is_utf16);
                            c2 = lre_canonicalize(c2, s->is_utf16);
                        }
                        if (c1 != c2)
                            goto no_match;
                    }
                } else {
                    cptr1 = cptr1_end;
                    while (cptr1 > cptr1_start) {
                        if (cptr == transfer_i32_to_ptr(s->cbuf))
                            goto no_match;
                        GET_PREV_CHAR(c1, cptr1, cptr1_start);
                        GET_PREV_CHAR(c2, cptr, transfer_i32_to_ptr(s->cbuf));
                        if (s->ignore_case) {
                            c1 = lre_canonicalize(c1, s->is_utf16);
                            c2 = lre_canonicalize(c2, s->is_utf16);
                        }
                        if (c1 != c2)
                            goto no_match;
                    }
                }
            }
            break;
        case REOP_range:
            {
                int n;
                uint32_t low, high, idx_min, idx_max, idx;
                
                n = get_u16(pc); /* n must be >= 1 */
                pc += 2;
                if (cptr >= cbuf_end)
                    goto no_match;
                GET_CHAR(c, cptr, cbuf_end);
                if (s->ignore_case) {
                    c = lre_canonicalize(c, s->is_utf16);
                }
                idx_min = 0;
                low = get_u16(pc + 0 * 4);
                if (c < low)
                    goto no_match;
                idx_max = n - 1;
                high = get_u16(pc + idx_max * 4 + 2);
                /* 0xffff in for last value means +infinity */
                if (unlikely(c >= 0xffff) && high == 0xffff)
                    goto range_match;
                if (c > high)
                    goto no_match;
                while (idx_min <= idx_max) {
                    idx = (idx_min + idx_max) / 2;
                    low = get_u16(pc + idx * 4);
                    high = get_u16(pc + idx * 4 + 2);
                    if (c < low)
                        idx_max = idx - 1;
                    else if (c > high)
                        idx_min = idx + 1;
                    else
                        goto range_match;
                }
                goto no_match;
            range_match:
                pc += 4 * n;
            }
            break;
        case REOP_range32:
            {
                int n;
                uint32_t low, high, idx_min, idx_max, idx;
                
                n = get_u16(pc); /* n must be >= 1 */
                pc += 2;
                if (cptr >= cbuf_end)
                    goto no_match;
                GET_CHAR(c, cptr, cbuf_end);
                if (s->ignore_case) {
                    c = lre_canonicalize(c, s->is_utf16);
                }
                idx_min = 0;
                low = get_u32(pc + 0 * 8);
                if (c < low)
                    goto no_match;
                idx_max = n - 1;
                high = get_u32(pc + idx_max * 8 + 4);
                if (c > high)
                    goto no_match;
                while (idx_min <= idx_max) {
                    idx = (idx_min + idx_max) / 2;
                    low = get_u32(pc + idx * 8);
                    high = get_u32(pc + idx * 8 + 4);
                    if (c < low)
                        idx_max = idx - 1;
                    else if (c > high)
                        idx_min = idx + 1;
                    else
                        goto range32_match;
                }
                goto no_match;
            range32_match:
                pc += 8 * n;
            }
            break;
        case REOP_prev:
            /* go to the previous char */
            if (cptr == transfer_i32_to_ptr(s->cbuf))
                goto no_match;
            PREV_CHAR(cptr, transfer_i32_to_ptr(s->cbuf));
            break;
        case REOP_simple_greedy_quant:
            {
                uint32_t next_pos, quant_min, quant_max;
                size_t q;
                intptr_t res;
                const uint8_t *pc1;
                
                next_pos = get_u32(pc);
                quant_min = get_u32(pc + 4);
                quant_max = get_u32(pc + 8);
                pc += 16;
                pc1 = pc;
                pc += (int)next_pos;
                
                q = 0;
                for(;;) {
                    res = lre_exec_backtrack(malloc_function_wasm, malloc_state_wasm, 
                                             transfer_ptr_to_i32(s), transfer_ptr_to_i32(capture), 
                                             transfer_ptr_to_i32(stack), stack_len,
                                             transfer_ptr_to_i32(pc1), transfer_ptr_to_i32(cptr), TRUE);
                    if (res == -1)
                        return res;
                    if (!res)
                        break;
                    cptr = (uint8_t *)res;
                    q++;
                    if (q >= quant_max && quant_max != INT32_MAX)
                        break;
                }
                if (q < quant_min)
                    goto no_match;
                if (q > quant_min) {
                    /* will examine all matches down to quant_min */
                    ret = push_state(malloc_function_wasm, malloc_state_wasm,  
                                     transfer_ptr_to_i32(s), 
                                     transfer_ptr_to_i32(capture), 
                                     transfer_ptr_to_i32(stack), stack_len,
                                     transfer_ptr_to_i32(pc1 - 16), transfer_ptr_to_i32(cptr),
                                     RE_EXEC_STATE_GREEDY_QUANT,
                                     q - quant_min);
                    if (ret < 0)
                        return -1;
                }
            }
            break;
        default:
            abort();
        }
    }
}
