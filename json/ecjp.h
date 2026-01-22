/*
BSD 3-Clause License

Copyright (c) 2025, Alfredo Montini

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef ECJP_H
#define ECJP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ecjp_limit.h"

#ifdef USE_BOOL_TYPE
#include <stdbool.h>
#else
#include <stdint.h>
typedef uint8_t bool;
#define true                        1
#define false                       0
#endif

//#define DEBUG                       1
//#define DEBUG_VERBOSE

#define ECJP_VERSION_MAJOR           1
#define ECJP_VERSION_MINOR           0
#define ECJP_VERSION_PATCH           0

#define ECJP_ARRAY_NO_INDEX         -1

typedef enum {
    ECJP_BOOL_FALSE,
    ECJP_BOOL_TRUE           
} ecjp_bool_t;

typedef enum {
    ECJP_ST_NULL,
    ECJP_ST_OBJ,
    ECJP_ST_ARRAY,
    ECJP_ST_MAX
} ecjp_struct_type_t;

typedef enum {
    ECJP_NO_ERROR = 0,
    ECJP_GENERIC_ERROR,
    ECJP_BRACKETS_MISSING,
    ECJP_SYNTAX_ERROR,
    ECJP_NULL_POINTER,
    ECJP_EMPTY_STRING,
    ECJP_NO_MORE_KEY,
    ECJP_NO_SPACE_IN_BUFFER_VALUE,
    ECJP_INDEX_OUT_OF_BOUNDS,
    ECJP_INDEX_NOT_FOUND,
    ECJP_MAX_ERROR
} ecjp_return_code_t;

typedef enum {
    ECJP_TYPE_UNDEFINED = 0,
    ECJP_TYPE_STRING,
    ECJP_TYPE_NUMBER,
    ECJP_TYPE_OBJECT,
    ECJP_TYPE_ARRAY,
    ECJP_TYPE_BOOL,
    ECJP_TYPE_NULL,
    ECJP_TYPE_KEY_VALUE_PAIR,
    ECJP_TYPE_MAX_TYPES  
} ecjp_value_type_t;

typedef enum {
    ECJP_PS_START = 0,
    ECJP_PS_IN_OBJECT,
    ECJP_PS_IN_ARRAY,
    ECJP_PS_IN_KEY,
    ECJP_PS_WAIT_VALUE,
    ECJP_PS_IN_VALUE,
    ECJP_PS_WAIT_COLON,
    ECJP_PS_WAIT_COMMA,
    ECJP_PS_END,
    ECJP_PS_MAX_STATUS
} ecjp_parse_status_t;

typedef enum {
    ECJP_PA_START = 0,
    ECJP_PA_IN_ARRAY,
    ECJP_PA_IN_STRING,
    ECJP_PA_IN_NUMBER,
    ECJP_PA_IN_BOOL,
    ECJP_PA_IN_NULL,
    ECJP_PA_OBJ_ELEM,
    ECJP_PA_ARRAY_ELEM,
    ECJP_PA_WAIT_COMMA,
    ECJP_PA_ERROR,
    ECJP_PA_END,
    ECJP_PA_MAX_STATUS
} ecjp_parse_array_status_t;

/*
 * Structures and type definitions
 * for keys list used to parse a Json-like input string
 * This list doesn't store the key and its value, but only
 * the position, length and type of each key found in the input string.
 * To retrieve the value associated to a key, use ecjp_get_key() or ecjp_read_key()
*/
typedef struct key_token {
    ECJP_TYPE_POS_KEY   start_pos;
    ECJP_TYPE_LEN_KEY   length;
    unsigned char       type;
} ecjp_key_token_t;

typedef struct key_elem {
    ecjp_key_token_t key;
    struct key_elem *next;
} ecjp_key_elem_t;

/*
 * Structures and type definitions
 * for items list used to store values read from a Json-like input string
 * This list stores the type and value of each item read from the input string.
 * No need to store position or length as the value is copied in the item structure.
 * No need to scan again the input string to retrieve the value.
 * This implementation use dynamic memory allocation for the value field and use much more memory than
 * the keys list implementation.
*/
typedef struct item_token {
    ecjp_value_type_t   type;
    void                *value;
    unsigned int        value_size;
} ecjp_item_token_t;

typedef struct item_elem {
    ecjp_item_token_t item;
    struct item_elem *next;
} ecjp_item_elem_t;

typedef union  {
        unsigned char all;
        struct {
            unsigned char in_string     : 1;
            unsigned char in_number     : 1;
            unsigned char in_key        : 1;
            unsigned char in_value      : 1;
            unsigned char trailing_comma: 1;
            unsigned char start_zero    : 1;
            unsigned char reserved      : 2;
        };
} ecjp_flags_t;

typedef struct ecjp_parse_stack_item {
    char char_value[ECJP_MAX_PARSE_STACK_DEPTH];
    int top;
} ecjp_parse_stack_item_t;

typedef struct ecjp_check_result {
    int                 err_pos;
    ECJP_TYPE_POS_KEY   num_keys;
    ecjp_struct_type_t  struct_type;
    int                 memory_used;
} ecjp_check_result_t;

typedef struct ecjp_parser_data {
    int index;
    int status;
    int open_brackets;
    int num_objects;
    int open_square_brackets;
    int num_arrays;
    ecjp_parse_stack_item_t parse_stack;
    ecjp_flags_t flags;
} ecjp_parser_data_t;

typedef struct ecjp_outdata {
    ecjp_return_code_t  error_code;
    ECJP_TYPE_POS_KEY   last_pos;
    ECJP_TYPE_LEN_KEY   length;
    ecjp_value_type_t   type;
    void                *value;
    unsigned int        value_size;
} ecjp_outdata_t;

typedef struct ecjp_indata {
    char                key[ECJP_MAX_KEY_LEN];
    ecjp_value_type_t   type;
    ECJP_TYPE_POS_KEY   pos;
    ECJP_TYPE_LEN_KEY   length;
} ecjp_indata_t;


extern char *ecjp_type[ECJP_TYPE_MAX_TYPES];

// Function declarations (public API)
ecjp_return_code_t ecjp_dummy(void);
ecjp_return_code_t ecjp_get_version(int *major, int *minor, int *patch);
ecjp_return_code_t ecjp_get_version_string(char *version_string, int max_length);
ecjp_return_code_t ecjp_show_error(const char *input, int err_pos);

#ifdef ECJP_TOKEN_LIST
// alternative functions using items list
ecjp_return_code_t ecjp_free_item_list(ecjp_item_elem_t **item_list);
ecjp_return_code_t ecjp_check_syntax_2(const char *input, ecjp_check_result_t *res);
ecjp_return_code_t ecjp_load_2(const char *input, ecjp_item_elem_t **item_list, ecjp_check_result_t *res);
ecjp_return_code_t ecjp_check_and_load_2(const char *input, ecjp_item_elem_t **item_list, ecjp_check_result_t *res);
ecjp_return_code_t ecjp_read_element(ecjp_item_elem_t *item_list, int index, ecjp_outdata_t *out);
ecjp_return_code_t ecjp_split_key_and_value(ecjp_item_elem_t *item_list, char *key, char *value, ecjp_bool_t leave_quotes);
ecjp_return_code_t ecjp_read_key_2(ecjp_item_elem_t *item_list, const char *key, unsigned int index, ecjp_outdata_t *out);
#else
ecjp_return_code_t ecjp_print_keys(const char *input, ecjp_key_elem_t *key_list);
ecjp_return_code_t ecjp_free_key_list(ecjp_key_elem_t **key_list);
ecjp_return_code_t ecjp_get_key(const char input[],char *key,ecjp_key_elem_t **key_list,ECJP_TYPE_POS_KEY start,ecjp_outdata_t *out);
ecjp_return_code_t ecjp_read_key(const char input[],ecjp_indata_t *in,ecjp_outdata_t *out);
ecjp_return_code_t ecjp_read_array_element(const char input[],int index,ecjp_outdata_t *out);
ecjp_return_code_t ecjp_get_keys_and_value(char *ptr,ecjp_key_elem_t *key_list);
ecjp_return_code_t ecjp_check_and_load(const char *input, ecjp_key_elem_t **key_list, ecjp_check_result_t *res, unsigned short int level);
ecjp_return_code_t ecjp_check_syntax(const char *input, ecjp_check_result_t *res);
ecjp_return_code_t ecjp_load(const char *input, ecjp_key_elem_t **key_list, ecjp_check_result_t *res, unsigned short int level);
#endif  // ECJP_TOKEN_LIST


#ifdef __cplusplus
}
#endif

#endif // ECJP_H


