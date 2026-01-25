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

#include "ecjp.h"

#ifdef ECJP_RUN_ON_PC
    #if DEBUG
        #define ecjp_printf(format, ...)    printf(format, __VA_ARGS__)
        #define ecjp_print(format)          printf(format)
    #else
        #define ecjp_printf(format, ...)
        #define ecjp_print(format)
    #endif
#else
    #ifdef ECJP_RUN_ON_MCU
        #define ecjp_printf(format, ...)
        #define ecjp_print(format)
    #else
        #if DEBUG
            #define ecjp_printf(format, ...)    printf(format, __VA_ARGS__)
            #define ecjp_print(format)          printf(format)
        #else
            #define ecjp_printf(format, ...)
            #define ecjp_print(format)
        #endif
    #endif // ECJP_RUN_ON_MCU
#endif // ECJP_RUN_ON_PC


char *ecjp_type[ECJP_TYPE_MAX_TYPES] = {
    "UNDEFINED",
    "STRING",
    "NUMBER",
    "OBJECT",
    "ARRAY",
    "BOOL",
    "NULL",
    "KEY_VALUE_PAIR"
};

/* Internal function definitions */

/*
    Function: ecjp_is_whitespace()
        This function checks if a character is a whitespace character.
        Parameters:
        - c: The character to check.
        Returns:
        - ECJP_BOOL_TRUE if the character is a whitespace character.
        - ECJP_BOOL_FALSE otherwise.
*/
ecjp_bool_t ecjp_is_whitespace(char c)
{
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        return ECJP_BOOL_TRUE;
    }
    return ECJP_BOOL_FALSE;
};

/*
 *   Function: ecjp_is_excode()
        This function checks if a character is a valid hexadecimal digit.
        Parameters:
        - c: The character to check.
        Returns:
        - ECJP_BOOL_TRUE if the character is a hexadecimal digit (0-9, a-f, A-F).
        - ECJP_BOOL_FALSE otherwise.
*/
ecjp_bool_t ecjp_is_excode(char c)
{
    if (c >= '0' && c <= '9') {
        // it's a digit
        return ECJP_BOOL_TRUE;
    } else if (c >= 'a' && c <= 'f') {
        // it's a letter in exadecimal range
        return ECJP_BOOL_TRUE;
    } else if (c >= 'A' && c <= 'F') {
        // it's a capital letter in exadecimal range
        return ECJP_BOOL_TRUE;
    }
    return ECJP_BOOL_FALSE;
}

/*
 *  Function: ecjp_is_ctrl()
        This function checks if a character is a control character (non-printable).
        Parameters:
        - c: The character to check.
        Returns:
        - ECJP_BOOL_TRUE if the character is a control character (0x00-0x1F, 0x7F) excluding newline, carriage return, and tab.
        - ECJP_BOOL_FALSE otherwise.
*/
ecjp_bool_t ecjp_is_ctrl(char c)
{
    if (((c >= 0x00) && (c <= 0x1F)) || (c == 0x7F)) {
        if ((c != '\n') && (c != '\r') && (c != '\t')) {
            return ECJP_BOOL_TRUE;
        }
    }
    return ECJP_BOOL_FALSE;
}

/*
 *  Function: ecjp_push_parse_stack()
        This function pushes a character onto the parse stack.
        Parameters:
        - s: Pointer to the parse stack.
        - c: The character to push onto the stack.
        Returns:
        - ECJP_BOOL_TRUE on success.
        - ECJP_BOOL_FALSE if the stack is full.
*/
ecjp_bool_t ecjp_push_parse_stack(ecjp_parse_stack_item_t *s, char c)
{
    if (s->top >= ECJP_MAX_PARSE_STACK_DEPTH - 1) {
        ecjp_printf("%s - %d: Parse stack overflow\n", __FUNCTION__,__LINE__);
        return ECJP_BOOL_FALSE;
    }
    s->top++;
    s->char_value[s->top] = c;
    return ECJP_BOOL_TRUE;
};

/*
 * Function: ecjp_pop_parse_stack()
        This function pops a character from the parse stack and checks if it matches the expected character.
        Parameters:
        - s: Pointer to the parse stack.
        - expected: The expected character to pop from the stack.
        Returns:
        - ECJP_BOOL_TRUE if the popped character matches the expected character.
        - ECJP_BOOL_FALSE if the stack is empty or if there is a mismatch.
*/
ecjp_bool_t ecjp_pop_parse_stack(ecjp_parse_stack_item_t *s, char expected)
{
    if (s->top < 0) {
        ecjp_printf("%s - %d: Parse stack underflow\n", __FUNCTION__,__LINE__);
        return ECJP_BOOL_FALSE;
    }
    if (expected != s->char_value[s->top]) {
        ecjp_printf("%s - %d: Parse stack mismatch: expected %c, got %c\n", __FUNCTION__,__LINE__, expected, s->char_value[s->top]);
        return ECJP_BOOL_FALSE;
    }
    else {
#ifdef DEBUG_VERBOSE
        ecjp_printf("%s - %d: Popped %c from parse stack\n", __FUNCTION__,__LINE__, s->char_value[s->top]);
#endif
        s->char_value[s->top] = 0;
    }
    s->top--;
    return ECJP_BOOL_TRUE;
};

/*
* Function: ecjp_peek_parse_stack()
        This function peeks at the top character of the parse stack without removing it and checks if it matches the expected character.
        Parameters:
        - s: Pointer to the parse stack.
        - check: The character to check against the top of the stack.
        Returns:
        - ECJP_BOOL_TRUE if the top character matches the check character.
        - ECJP_BOOL_FALSE if the stack is empty or if there is a mismatch.
*/
ecjp_bool_t ecjp_peek_parse_stack(ecjp_parse_stack_item_t *s, char check)
{
    if (s->top < 0) {
        ecjp_printf("%s - %d: Parse stack underflow on peek\n", __FUNCTION__,__LINE__);
        return ECJP_BOOL_FALSE;
    }
    if (check == s->char_value[s->top]) {
        return ECJP_BOOL_TRUE;
    } else {
#ifdef DEBUG_VERBOSE        
        ecjp_printf("%s - %d: Parse stack peek mismatch: search %c, got %c\n", __FUNCTION__,__LINE__, check, s->char_value[s->top]);
#endif
        return ECJP_BOOL_FALSE;
    }
    return ECJP_BOOL_TRUE;
};

/*
 * Function: ecjp_get_level_parse_stack()
        This function retrieves the current level (top index) of the parse stack.
        Parameters:
        - s: Pointer to the parse stack.
        Returns:
        - The current top index of the stack. 0 means one item in the stack.
        - -1 if the stack is empty.
*/
int ecjp_get_level_parse_stack(ecjp_parse_stack_item_t *s)
{
    if (s->top < 0) {
        ecjp_printf("%s - %d: Parse stack is empty!\n", __FUNCTION__,__LINE__);
        return -1;
    }
    return s->top;
};

#ifdef DEBUG_VERBOSE
/* 
 * Function: ecjp_print_check_summary()
        This function prints a summary of the parsing process for debugging purposes.
        Parameters:
        - p: Pointer to the parser data structure.
*/
void ecjp_print_check_summary(ecjp_parser_data_t *p)
{
    ecjp_flags_t flags = p->flags;
    ecjp_print("Parsing summary:\n");
    ecjp_printf("  Total characters processed: %d\n", p->index);
    ecjp_printf("  Final parser status: %d\n", p->status);
    ecjp_printf("  Total objects: %d\n", p->num_objects);
    ecjp_printf("  Total arrays: %d\n", p->num_arrays);
    ecjp_printf("  Final open brackets count: %d\n", p->open_brackets);
    ecjp_printf("  Final open square brackets count: %d\n", p->open_square_brackets);
    ecjp_print("Flags status:\n");
    ecjp_printf("  in_string: %d\n", flags.in_string);
    ecjp_printf("  in_key: %d\n", flags.in_key);
    ecjp_printf("  in_value: %d\n", flags.in_value);
    ecjp_printf("  in_number: %d\n", flags.in_number);
    ecjp_printf("  trailing_comma: %d\n", flags.trailing_comma);
    ecjp_print("Stack:\n");
    ecjp_printf("  Stack top index: %d\n", p->parse_stack.top);
    return;
};
#endif


/* Public function definitions */

/*
    Function: ecjp_dummy()
        A simple dummy function to test library linkage.
        Returns:
        - ECJP_NO_ERROR on success.
*/
ecjp_return_code_t ecjp_dummy(void)
{
    ecjp_printf("%s: Hello ecjp library!\n",__FUNCTION__);
    return ECJP_NO_ERROR;
};

/*
    Function: ecjp_get_version()
        This function retrieves the version of the eCjp library.
        Parameters:
        - major: Pointer to an integer to store the major version number.
        - minor: Pointer to an integer to store the minor version number.
        - patch: Pointer to an integer to store the patch version number.
        Returns:
        - ECJP_NO_ERROR on success.
*/
ecjp_return_code_t ecjp_get_version(int *major, int *minor, int *patch)
{
    if (major != NULL) {
        *major = ECJP_VERSION_MAJOR;
    }
    if (minor != NULL) {
        *minor = ECJP_VERSION_MINOR;
    }
    if (patch != NULL) {
        *patch = ECJP_VERSION_PATCH;
    }
    return ECJP_NO_ERROR;
};

/*
    Function: ecjp_get_version_string()
        This function retrieves the version of the eCjp library as a string.
        Parameters:
        - version_string: Pointer to a character array to store the version string.
        - max_length: Maximum length of the version string buffer.
        Returns:
        - ECJP_NO_ERROR on success.
        - ECJP_NULL_POINTER if version_string is NULL or max_length is 0.
*/
ecjp_return_code_t ecjp_get_version_string(char *version_string, int max_length)
{
    if (version_string == NULL || max_length == 0) {
        return ECJP_NULL_POINTER;
    }
    snprintf(version_string, max_length, "%d.%d.%d", ECJP_VERSION_MAJOR, ECJP_VERSION_MINOR, ECJP_VERSION_PATCH);
    return ECJP_NO_ERROR;
};

/*
    Function: ecjp_show_error()
        This function displays the input string with an indicator pointing to the error position.
        Parameters:
        - input: The JSON-like input string.
        - err_pos: The position of the error in the input string.
        Returns:
        - ECJP_NO_ERROR on success.
*/
ecjp_return_code_t ecjp_show_error(const char *input, int err_pos)
{
    int row, i, j, err_row, err_column;
    char input_no_newline[ECJP_MAX_INPUT_SIZE];

    // Remove newlines from input for proper error display
    j = 0;
    memset(input_no_newline, 0, ECJP_MAX_INPUT_SIZE);
    for (i = 0; i < strlen(input) && j < ECJP_MAX_INPUT_SIZE - 1; i++) {
        if (input[i] != '\n' && input[i] != '\r') {
            input_no_newline[j++] = input[i];
        }
        else {
            // Replace newline with space to maintain character positions
            input_no_newline[j++] = ' ';
        }
    }
    input_no_newline[j] = '\0';
#ifdef DEBUG_VERBOSE
    ecjp_printf("%s - %d: j = %d - i= %d -input len = %ld\n",__FUNCTION__,__LINE__,j,i,strlen(input));
    ecjp_printf("%s - %d: Input without newlines for error display:\n%s\n",__FUNCTION__,__LINE__,input_no_newline);
#endif

    row = strlen(input) / ECJP_MAX_PRINT_COLUMNS;
    err_row = err_pos / ECJP_MAX_PRINT_COLUMNS;
    err_column = err_pos % ECJP_MAX_PRINT_COLUMNS;

    ecjp_printf("%s - %d: Error at position %d (row %d, column %d):\n", __FUNCTION__,__LINE__,err_pos, err_row + 1, err_column + 1);

    int remain;
    for (i = 0; i <= row; i++) {
        remain = strlen(input_no_newline) - (i * ECJP_MAX_PRINT_COLUMNS);
        remain = (remain > ECJP_MAX_PRINT_COLUMNS) ? ECJP_MAX_PRINT_COLUMNS : remain;
        ecjp_printf("%.*s\n",remain, &input_no_newline[i * ECJP_MAX_PRINT_COLUMNS]);
        if(i == err_row) {
            for (j = 0; j < err_column; j++) {
                ecjp_print("-");
            }
            ecjp_print("^\n");
        }
    }
    return ECJP_NO_ERROR;
};


#ifdef ECJP_TOKEN_LIST

/******* ALTERNATIVE IMPLEMENTATION *********/
/* 
In this implementation, we parse the input string and extract item tokens (values) instead of keys.
We store these item tokens in a linked list passed as a parameter.
This implementation is useful when the focus is on the values rather than the keys but use much more memory.
*/

/* Internal API functions */

/*
 *  Function: ecjp_store_tmp_item()
    This function store a character in the temporary buffer for building an item token value.
    Parameters:
    - buffer: The temporary buffer to store the character.
    - p_buffer: Pointer to the current position in the buffer.
    - c: The character to be stored.
    Returns:
    - ECJP_NO_ERROR if the character is stored successfully.
    - ECJP_NO_SPACE_IN_BUFFER_VALUE if the buffer exceeds maximum length.
*/
ecjp_return_code_t ecjp_store_tmp_item(char *buffer, int *p_buffer, char c)
{
    if(*p_buffer >= (ECJP_MAX_ITEM_LEN - 1)) {
        ecjp_printf("%s - %d: Item length exceeds maximum limit (p_buffer = %d, limit = %d)\n", __FUNCTION__,__LINE__, *p_buffer, ECJP_MAX_ITEM_LEN);
        return ECJP_NO_SPACE_IN_BUFFER_VALUE;
    }
    buffer[*p_buffer] = c;
    (*p_buffer)++;
#ifdef DEBUG_VERBOSE    
    ecjp_printf("%s - %d: Stored char '%c' in tmp buffer at position %d\n", __FUNCTION__,__LINE__, c, *p_buffer - 1);
#endif
    return ECJP_NO_ERROR;
}

/*
 *  Function: ecjp_load_item()
    This function load an item token from the temporary buffer.
    Parameters:
    - token: Pointer to the item token structure to be loaded.
    - tmp_buffer: The temporary buffer containing the item value.
    - p_buffer: The size of the item value in the buffer.
    Returns:
    - The length of the loaded item token value.
    - 0 if there is a memory allocation error.
*/
int ecjp_load_item(ecjp_item_token_t *token, char *tmp_buffer, int p_buffer)
{
    int len = (p_buffer + 1);

    // allocate memory for token value, copy tmp_buffer to token value
    // in tmp_buffer there are no more than ECJP_MAX_ITEM_LEN characters
    token->value = malloc(len);
    if (token->value == NULL) {
        ecjp_printf("%s - %d: Memory allocation error for item token value\n", __FUNCTION__,__LINE__);
        token->value_size = 0;
        return 0;
    }   
    // null terminate the string
    tmp_buffer[p_buffer] = '\0';
    token->value_size = len;
    strncpy(token->value, tmp_buffer, len);

    ecjp_printf("%s - %d: Loaded item token of type %s, size %u, value: %s\n", __FUNCTION__,__LINE__, ecjp_type[token->type], token->value_size, (char *)token->value);
    return len;
}

/*
 *  Function: ecjp_reset_tmp_buffer()
    This function reset the temporary buffer for building an item token value.
    Parameters:
    - tmp_buffer: The temporary buffer to be reset.
    - p_buffer: Pointer to the current position in the buffer to be reset.
*/
void ecjp_reset_tmp_buffer(char *tmp_buffer, int *p_buffer)
{
    memset(tmp_buffer, 0, ECJP_MAX_ITEM_LEN);
    *p_buffer = 0;
    return;
}

/*
 *  Function: ecjp_init_item_token()
    This function initialize an item token structure.
    Parameters:
    - token: Pointer to the item token structure to be initialized.
*/
void ecjp_init_item_token(ecjp_item_token_t *token)
{
    token->type = ECJP_TYPE_UNDEFINED;
    token->value = NULL;
    token->value_size = 0;
    return;
}

/*
 *  Function: ecjp_free_item_token()
    This function free the memory allocated for an item token structure.
    Parameters:
    - token: Pointer to the item token structure to be freed.
*/
void ecjp_free_item_token(ecjp_item_token_t *token)
{
    if (token->value != NULL) {
        free(token->value);
        token->value = NULL;
    }
    token->value_size = 0;
    token->type = ECJP_TYPE_UNDEFINED;
    return;
}

/*
 * Function: ecjp_add_node_item_end()
        This function adds a new item token node to the end of the linked list.
        Parameters:
        - head: Pointer to the head of the linked list.
        - data: Pointer to the item token data to add.
        Returns:
        - 0 on success.
        - -1 on memory allocation failure.
*/
int ecjp_add_node_item_end(ecjp_item_elem_t **head, ecjp_item_token_t *data)
{
    ecjp_item_elem_t  *new_node;
    ecjp_item_elem_t  *current;
    
    new_node = (ecjp_item_elem_t *)malloc(sizeof(ecjp_item_elem_t));
    if (!new_node)  
        return -1;
    new_node->item.type = data->type;
    new_node->item.value = data->value;
    new_node->item.value_size = data->value_size;
    new_node->next = NULL;
    if (*head == NULL)
    {
        *head = new_node;
    }
    else
    {
        current = *head;
        while (current->next != NULL)
            current = current->next;
        current->next = new_node;
    }
    return 0;
}

/* Public API functions */

/* 
 *  Function: ecjp_free_item_list()
    This function free the memory allocated for a list of item elements.
    Parameters:
    - item_list: Pointer to the list of item elements to be freed.
    Returns:
    - ECJP_NO_ERROR if the list is freed successfully.
*/
ecjp_return_code_t ecjp_free_item_list(ecjp_item_elem_t **item_list)
{
    ecjp_item_elem_t *current;
    ecjp_item_elem_t *next;

    if(item_list == NULL || *item_list == NULL)
        return ECJP_NO_ERROR;
        
    current = *item_list;

    while (current != NULL) {
        free(current->item.value);
        next = current->next;
        free(current);
        current = next;
    }
    *item_list = NULL;

    return ECJP_NO_ERROR;
}

/* 
 * Function: ecjp_check_and_load_2()
    This function checks the syntax of a JSON-like input string and loads item tokens (values)
    into a linked list if the syntax is valid.
    Parameters:
    - input: The JSON-like input string to be checked and loaded.
    - item_list: Pointer to a list of item elements loaded with the item tokens found in the input string.
    - res: Pointer to a structure to store the result of the check, including any error position.
    Returns:
    - ECJP_NO_ERROR if the input string is valid.
    - ECJP_NULL_POINTER if any input pointer is NULL.
    - ECJP_EMPTY_STRING if the input string is empty.
    - ECJP_SYNTAX_ERROR if there is a syntax error in the input string.
*/
ecjp_return_code_t ecjp_check_and_load_2(const char *input, ecjp_item_elem_t **item_list, ecjp_check_result_t *res)
{
    ecjp_parser_data_t parser_data;
    ecjp_parser_data_t *p;
    ecjp_item_token_t token;
    char tmp_buffer[ECJP_MAX_ITEM_LEN];
    int p_buffer = 0;

    memset(tmp_buffer, 0, ECJP_MAX_ITEM_LEN);
    memset(&token, 0, sizeof(ecjp_item_token_t));
    memset(&parser_data, 0, sizeof(ecjp_parser_data_t));
    p =  &parser_data;
    p->parse_stack.top = -1;

    if ((input == NULL) || (res == NULL)) {
        ecjp_printf("%s - %d: NULL pointer input/res\n",__FUNCTION__,__LINE__);
        return ECJP_NULL_POINTER;
    }
    if (strlen(input) == 0) {
        ecjp_printf("%s - %d: Empty string input\n",__FUNCTION__,__LINE__);
        return ECJP_EMPTY_STRING;
    }
#ifdef DEBUG_VERBOSE
    ecjp_printf("%s - %d:\nInput string: %s\n",__FUNCTION__,__LINE__,input);
#endif

    p->index = 0;
    p->flags.all = 0;
    p->status = ECJP_PS_START;
    while (input[p->index] != '\0') {
        // walk through the string
#ifdef DEBUG_VERBOSE        
        ecjp_printf("%s - %d: Index %d, Status %d, Char '%c'\n", __FUNCTION__,__LINE__, p->index, p->status, input[p->index]);
#endif
        switch (p->status)
        {
            case ECJP_PS_START:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }   
                        p->status = ECJP_PS_IN_OBJECT;
                        res->struct_type = ECJP_ST_OBJ;
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }   
                        p->status = ECJP_PS_IN_ARRAY;
                        res->struct_type = ECJP_ST_ARRAY;
                        break;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Input must start with '{' or ']'\n", __FUNCTION__,__LINE__);
                        return ECJP_SYNTAX_ERROR;
                        break;
                }
                break;
 
            case ECJP_PS_IN_OBJECT:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_OBJECT;
                        // copy in tmp buffer
                        if (p_buffer == 0) {
                            token.type = ECJP_TYPE_OBJECT;
                        }
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;  

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PS_END;
                        } else {
                            p->status = ECJP_PS_WAIT_COMMA;
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        }
                        break;

                    case ',':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->flags.trailing_comma = 1;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    case '"':
                        p->flags.in_string = 1;
                        p->status = ECJP_PS_IN_KEY;
                        p->flags.in_key = 1;
                        p->flags.trailing_comma = 0;
                        if (p_buffer == 0) {
                            token.type = ECJP_TYPE_KEY_VALUE_PAIR;
                        }
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;
                    
                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Expected string key or closing bracket, receive: %c\n", __FUNCTION__,__LINE__,input[p->index]);
                        return ECJP_SYNTAX_ERROR;
                        break;  
                }
                break;
            
            case ECJP_PS_IN_ARRAY:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_OBJECT;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        if (p_buffer == 0) {
                            token.type = ECJP_TYPE_OBJECT;
                        }
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->status = ECJP_PS_WAIT_COMMA;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_ARRAY;
                        if (p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        if (p_buffer == 0) {
                            token.type = ECJP_TYPE_ARRAY;
                        }
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    case ']':
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->status = ECJP_PS_WAIT_COMMA;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;
                    
                    case '"':
                        p->flags.in_string = 1;
                        p->status = ECJP_PS_IN_VALUE;
                        if (p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        if (p_buffer == 0) {
                            token.type = ECJP_TYPE_STRING;
                        }
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    default:
                        if (strncmp(&input[p->index], "true", 4) == 0 || strncmp(&input[p->index], "false", 5) == 0 || strncmp(&input[p->index], "null", 4) == 0) {
                            // valid value
                            p->status = ECJP_PS_WAIT_COMMA;
                            if (strncmp(&input[p->index], "true", 4) == 0) {
                                if (p_buffer == 0) {
                                    token.type = ECJP_TYPE_BOOL;
                                }
                                for (int i = 0; i < 4; i++) {
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index + i]);
                                }
                            } else if (strncmp(&input[p->index], "false", 5) == 0) {
                                if (p_buffer == 0) {
                                    token.type = ECJP_TYPE_BOOL;
                                }
                                for (int i = 0; i < 5; i++) {
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index + i]);
                                }
                            } else {
                                if (p_buffer == 0) {
                                    token.type = ECJP_TYPE_NULL;
                                }
                                for (int i = 0; i < 4; i++) {
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index + i]);
                                }
                            }   
                            p->index += (input[p->index] == 'f') ? 5 : 4; // move index forward
                            if (p->flags.trailing_comma) {
                                p->flags.trailing_comma = 0;
                            }   
                            continue;
                        }
                        if ((input[p->index] >= '0' && input[p->index] <= '9') || input[p->index] == '-') {
                            // valid value start
                            p->flags.in_number = 1;
                            if (p_buffer == 0) {
                                token.type = ECJP_TYPE_NUMBER;
                            }
                            if (input[p->index] == '0') {
                                p->flags.start_zero = 1;
                            }
                            p->status = ECJP_PS_IN_VALUE;
                            if (p->flags.trailing_comma) {
                                p->flags.trailing_comma = 0;
                            }
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);       
                        } else {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Invalid character in value\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        break;
                }
                break;
            
            case ECJP_PS_IN_KEY:
                // in this status we are always inside a string (p->flags.in_string == 1) 
                switch (input[p->index]) {
                    case '\\':
                        // skip escaped character
                        p->index++;
                        switch(input[p->index]) {
                            case '"':
                            case '\\':
                            case '/':
                            case 'b':
                            case 'f':
                            case 'r':
                            case 'n':
                            case 't':
                                p->index++;
                                continue;

                            case 'u':
                                if(ecjp_is_excode(input[p->index+1]) &&
                                   ecjp_is_excode(input[p->index+2]) &&
                                   ecjp_is_excode(input[p->index+3]) &&
                                   ecjp_is_excode(input[p->index+4])) {
                                    for (int i = 0; i < 5; i++) {
                                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index - 1 + i]);
                                    }
                                    p->index += 4;
                                } else {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Invalid character in unicode sequence\n", __FUNCTION__,__LINE__);
                                    return ECJP_SYNTAX_ERROR;
                                }
                                break;

                            default:
                                res->err_pos = p->index;
                                ecjp_printf("%s - %d: Invalid character in escape sequence (%c)\n", __FUNCTION__,__LINE__, input[p->index]);
                                return ECJP_SYNTAX_ERROR;
                                break;
                        }
                        break;

                    case '"':
                        p->flags.in_string = 0;
                        p->status = ECJP_PS_WAIT_COLON;
                        p->flags.in_key = 0;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    default:
                        if(ecjp_is_ctrl(input[p->index])) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Invalid control character in key\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        } else {
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        }
                        break;
                }
                break;

            case ECJP_PS_WAIT_COLON:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case ':':
                        p->status = ECJP_PS_WAIT_VALUE;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Expected colon after key, received: %c\n",__FUNCTION__,__LINE__,input[p->index]);
                        return ECJP_SYNTAX_ERROR;
                        break;
                }
                break;

            case ECJP_PS_WAIT_VALUE:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_OBJECT;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_ARRAY;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    case '"':
                        p->flags.in_string = 1;
                        p->status = ECJP_PS_IN_VALUE;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    default:
                        if (strncmp(&input[p->index], "true", 4) == 0 || strncmp(&input[p->index], "false", 5) == 0 || strncmp(&input[p->index], "null", 4) == 0) {
                            // valid value
                            p->status = ECJP_PS_WAIT_COMMA;
                            if (strncmp(&input[p->index], "true", 4) == 0) {
                                for (int i = 0; i < 4; i++) {
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index + i]);
                                }
                            } else if (strncmp(&input[p->index], "false", 5) == 0) {
                                for (int i = 0; i < 5; i++) {
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index + i]);
                                }
                            } else {
                                for (int i = 0; i < 4; i++) {
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index + i]);
                                }
                            }
                            p->index += (input[p->index] == 'f') ? 5 : 4; // move index forward
                            continue;
                        }
                        if ((input[p->index] >= '0' && input[p->index] <= '9') || input[p->index] == '-') {
                            // valid value start
                            p->flags.in_number = 1;
                            if (input[p->index] == '0') {
                                p->flags.start_zero = 1;
                            }
                            p->status = ECJP_PS_IN_VALUE;
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        } else {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Invalid character in value\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        break;
                }
                break;

            case ECJP_PS_IN_VALUE:
                switch(input[p->index]) {
                    case '"':
                        if(!p->flags.in_string) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected quote in value\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->flags.in_string = 0;
                        p->status = ECJP_PS_WAIT_COMMA;
                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        break;

                    default:
                        if (p->flags.in_string) {
                            switch (input[p->index]) {
                                case '\\':
                                    // skip escaped character
                                    p->index++;
                                    switch(input[p->index]) {
                                        case '"':
                                        case '\\':
                                        case '/':
                                        case 'b':
                                        case 'f':
                                        case 'r':
                                        case 'n':
                                        case 't':
                                            p->index++;
                                            continue;

                                        case 'u':
                                            if(ecjp_is_excode(input[p->index+1]) &&
                                               ecjp_is_excode(input[p->index+2]) &&
                                               ecjp_is_excode(input[p->index+3]) &&
                                               ecjp_is_excode(input[p->index+4])) {
                                                for (int i = 0; i < 5; i++) {
                                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index - 1 + i]);
                                                }
                                                p->index += 4;
                                            } else {
                                                res->err_pos = p->index;
                                                ecjp_printf("%s - %d: Invalid character in unicode sequence\n", __FUNCTION__,__LINE__);
                                                return ECJP_SYNTAX_ERROR;
                                            }
                                            break;

                                        default:
                                            res->err_pos = p->index;
                                            ecjp_printf("%s - %d: Invalid character in escape sequence (%c)\n", __FUNCTION__,__LINE__, input[p->index]);
                                            return ECJP_SYNTAX_ERROR;
                                            break;
                                    }
                                    break;

                                default:
                                    if (ecjp_is_ctrl(input[p->index])) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Invalid control character inside value\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                                    break;
                            }
                        } else {
                            switch (input[p->index]) {
                                case ' ':
                                case '\n':
                                case '\r':
                                case '\t':
                                    if (p->flags.in_number) {
                                        p->flags.in_number = 0;
                                        p->flags.start_zero = 0;
                                        p->status = ECJP_PS_WAIT_COMMA;
                                    }
                                    else {
                                        // skip whitespace
                                        p->index++;
                                        continue;
                                    }
                                    break;
                                
                                case ',':
                                    if(p->flags.trailing_comma) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    if (p->flags.in_number) {
                                        p->flags.in_number = 0;
                                        p->flags.start_zero = 0;
                                    }
                                    p->flags.trailing_comma = 1;
                                    if (ecjp_peek_parse_stack(&(p->parse_stack),'[') == ECJP_BOOL_TRUE) {
                                        p->status = ECJP_PS_IN_ARRAY;
                                    } else {
                                        p->status = ECJP_PS_IN_OBJECT;
                                    }
                                    if (ecjp_get_level_parse_stack(&(p->parse_stack)) == 0) {
                                        if (item_list != NULL)
                                        {
                                           res->memory_used += ecjp_load_item(&token, tmp_buffer, p_buffer);
                                            // Add item token to the list
                                            if (ecjp_add_node_item_end(item_list, &token) != 0) {
                                                res->err_pos = p->index;
                                                ecjp_printf("%s - %d: Failed to add item token to the list\n", __FUNCTION__,__LINE__);
                                                return ECJP_GENERIC_ERROR;
                                            } else {
#ifdef DEBUG_VERBOSE
                                                ecjp_printf("%s - %d: Added item token to the list, item_list = %p\n", __FUNCTION__,__LINE__, (void *)*item_list);
#endif
                                                res->memory_used += sizeof(ecjp_item_elem_t);
                                                res->num_keys++;
                                            }
                                        }
                                        // Reset token for next item
                                        ecjp_init_item_token(&token);
                                        ecjp_reset_tmp_buffer(tmp_buffer, &p_buffer);
                                    } else {
                                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                                    }
                                    break;
                                
                                case '0':
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                case '5':
                                case '6':
                                case '7':
                                case '8':
                                case '9':
                                case '.':
                                case 'e':
                                case 'E':
                                case '+':
                                case '-':
                                    if (p->flags.in_number) {
                                        // valid number continuation
                                        ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]); 
                                        if (input[p->index] == '.') {
                                            if (p->flags.start_zero == 1) {
                                                p->flags.start_zero = 0;
                                            }
                                            break;
                                        } else {
                                            if (p->flags.start_zero == 1) {
                                                // number can start with '0' unless is decimal value
                                                res->err_pos = p->index;
                                                ecjp_printf("%s - %d: Number can't start with value 0\n", __FUNCTION__,__LINE__);
                                                return ECJP_SYNTAX_ERROR;
                                            }
                                            break;
                                        }
                                    } else {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Unexpected number character in value\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    break;

                                case '}':
                                    if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    p->open_brackets--;
                                    if (p->open_brackets < 0) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                                        p->status = ECJP_PS_END;
                                    } else {
                                        p->status = ECJP_PS_WAIT_COMMA;
                                    }
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                                    break;

                                case ']':
                                    if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    p->open_square_brackets--;
                                    if (p->open_square_brackets < 0) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    if (p->open_square_brackets == 0 && p->open_brackets == 0) {
                                        p->status = ECJP_PS_END;
                                    } else {
                                        p->status = ECJP_PS_WAIT_COMMA;
                                    }
                                    ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                                    break;

                                default:
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Invalid character in value\n", __FUNCTION__,__LINE__);
                                    return ECJP_SYNTAX_ERROR;
                                    break;
                            }
                        }
                        break;
                }
                break;
                        
            case ECJP_PS_WAIT_COMMA:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;
                    
                    case '}':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_get_level_parse_stack(&(p->parse_stack)) > 0) {
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        }
                        if (ecjp_get_level_parse_stack(&(p->parse_stack)) == 0) {
                            if (item_list != NULL)
                            {
                                res->memory_used += ecjp_load_item(&token, tmp_buffer, p_buffer);
                                // Add item token to the list
                                if (ecjp_add_node_item_end(item_list, &token) != 0) {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Failed to add item token to the list\n", __FUNCTION__,__LINE__);
                                    return ECJP_GENERIC_ERROR;
                                } else {
#ifdef DEBUG_VERBOSE
                                    ecjp_printf("%s - %d: Added item token to the list, item_list = %p\n", __FUNCTION__,__LINE__, (void *)*item_list);
#endif
                                    res->memory_used += sizeof(ecjp_item_elem_t);
                                    res->num_keys++;
                                }
                            }
                            // Reset token for next item
                            ecjp_init_item_token(&token);
                            ecjp_reset_tmp_buffer(tmp_buffer, &p_buffer);
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PS_END;
                        }
                        break;

                    case ']':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Trailing comma before closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_get_level_parse_stack(&(p->parse_stack)) > 0) {
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        }
                        if (ecjp_get_level_parse_stack(&(p->parse_stack)) == 0) {
                            if (item_list != NULL)
                            {
                                res->memory_used += ecjp_load_item(&token, tmp_buffer, p_buffer);
                                // Add item token to the list
                                if (ecjp_add_node_item_end(item_list, &token) != 0) {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Failed to add item token to the list\n", __FUNCTION__,__LINE__);
                                    return ECJP_GENERIC_ERROR;
                                } else {
#ifdef DEBUG_VERBOSE
                                    ecjp_printf("%s - %d: Added item token to the list, item_list = %p\n", __FUNCTION__,__LINE__, (void *)*item_list);
#endif
                                    res->memory_used += sizeof(ecjp_item_elem_t);
                                    res->num_keys++;
                                }
                            }
                            // Reset token for next item
                            ecjp_init_item_token(&token);
                            ecjp_reset_tmp_buffer(tmp_buffer, &p_buffer);
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PS_END;
                        }
                        break;

                    case ',':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_get_level_parse_stack(&(p->parse_stack)) > 0) {
                            ecjp_store_tmp_item(tmp_buffer, &p_buffer, input[p->index]);
                        }
                        if (ecjp_get_level_parse_stack(&(p->parse_stack)) == 0) {
                            if (item_list != NULL)
                            {
                                res->memory_used += ecjp_load_item(&token, tmp_buffer, p_buffer);
                                // Add item token to the list
                                if (ecjp_add_node_item_end(item_list, &token) != 0) {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Failed to add item token to the list\n", __FUNCTION__,__LINE__);
                                    return ECJP_GENERIC_ERROR;
                                } else {
#ifdef DEBUG_VERBOSE
                                    ecjp_printf("%s - %d: Added item token to the list, item_list = %p\n", __FUNCTION__,__LINE__, (void *)*item_list);
#endif
                                    res->memory_used += sizeof(ecjp_item_elem_t);
                                    res->num_keys++;
                                }
                            }
                            // Reset token for next item
                            ecjp_init_item_token(&token);
                            ecjp_reset_tmp_buffer(tmp_buffer, &p_buffer);
                        }
                        if (ecjp_peek_parse_stack(&(p->parse_stack),'[') == ECJP_BOOL_TRUE) {
                            p->status = ECJP_PS_IN_ARRAY;
                        } else {
                            p->status = ECJP_PS_IN_OBJECT;
                        }
                        p->flags.trailing_comma = 1;
                        break;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Expected comma after value\n", __FUNCTION__,__LINE__);
                        return ECJP_SYNTAX_ERROR;
                        break;
                }   
                break;
        
            case ECJP_PS_END:
                // should be only whitespace after end
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Unexpected character after end of JSON\n", __FUNCTION__,__LINE__);
                        return ECJP_SYNTAX_ERROR;
                }
                break;
        
            default:
                break;
        }

        p->index++;
    }

    if (p->status != ECJP_PS_END) {
        ecjp_printf("%s - %d: Incomplete JSON structure\n", __FUNCTION__,__LINE__);
        return ECJP_SYNTAX_ERROR;
    }
    if ((p->open_brackets != 0) || (p->open_square_brackets != 0)) {
        res->err_pos = (p->index - 1);
        ecjp_printf("%s - %d: Mismatched brackets at end of input\n", __FUNCTION__,__LINE__);
        return ECJP_BRACKETS_MISSING;
    }

#ifdef DEBUG_VERBOSE
    ecjp_print_check_summary(p);
#endif
    ecjp_printf("%s - %d: Allocated memory used: %d bytes\n", __FUNCTION__,__LINE__, res->memory_used);


    return ECJP_NO_ERROR;
};

/*
    Function: ecjp_check_syntax_2()
    This function call ecjp_check_and_load_2() without pointer to store the items to perform only syntax checking.
    Parameters:
    - input: The JSON-like input string to be checked and loaded.
    - res: Pointer to a structure to store the result of the check, including any error position.
    Returns:
    - ECJP_NO_ERROR if the input string is valid.
    - ECJP_NULL_POINTER if any input pointer is NULL.
    - ECJP_EMPTY_STRING if the input string is empty.
    - ECJP_SYNTAX_ERROR if there is a syntax error in the input string.
*/
ecjp_return_code_t ecjp_check_syntax_2(const char *input, ecjp_check_result_t *res)
{
    return ecjp_check_and_load_2(input, NULL, res);
}

/*
    Function: ecjp_load_2()
    This function call ecjp_check_and_load_2() with pointer to store the items. Perform syntax checking
    and store all items found in the input string at the first level of the structure.
    Parameters:
    - input: The JSON-like input string to be checked and loaded.
    - item_list: Pointer to a list of item elements loaded with the items found in the input string.
    - res: Pointer to a structure to store the result of the check, including any error position.
    Returns:
    - ECJP_NO_ERROR if the input string is valid.
    - ECJP_NULL_POINTER if any input pointer is NULL.
    - ECJP_EMPTY_STRING if the input string is empty.
    - ECJP_SYNTAX_ERROR if there is a syntax error in the input string.
*/
ecjp_return_code_t ecjp_load_2(const char *input, ecjp_item_elem_t **item_list, ecjp_check_result_t *res)
{    
    return ecjp_check_and_load_2(input, item_list, res);
}

/*
 * Function: ecjp_split_key_and_value()
 * --------------------
 * Splits a key-value pair item into separate key and value strings.
 * Parameters:
 *      item_list: Pointer to the ecjp_item_elem_t containing the key-value pair.
 *      key: Pointer to a char array where the extracted key will be stored.
 *      value: Pointer to a char array where the extracted value will be stored.
 *      leave_quotes: Boolean flag indicating whether to retain quotes around the key.
 * Returns:
 *  ECJP_NO_ERROR on success
 *  ECJP_NULL_POINTER if any input pointer is NULL
 *  ECJP_SYNTAX_ERROR if the item is not a key-value pair
 *  ECJP_NO_SPACE_IN_BUFFER_VALUE if the key or value exceeds maximum length of the buffers
*/
ecjp_return_code_t ecjp_split_key_and_value(ecjp_item_elem_t *item_list, char *key, char *value, ecjp_bool_t leave_quotes)
{
    int i, j;
    char inside_string = 0;

    if (item_list == NULL || key == NULL || value == NULL) {
        return ECJP_NULL_POINTER;
    }

    if (item_list->item.type != ECJP_TYPE_KEY_VALUE_PAIR) {
        return ECJP_SYNTAX_ERROR;
    }
    // NOTE: The function assumes that the key and value are separated by a colon (:) and that the key is a string enclosed in quotes
    // and there are no other quotes in the key.
    for (i = 0; i < item_list->item.value_size && i < ECJP_MAX_KEY_LEN; i++) {
        if ((((char *)item_list->item.value)[i] == ':') && (inside_string == 0)) {
            break;
        }
        else {
            if (((char *)item_list->item.value)[i] != '"') {
                (*key) = ((char *)item_list->item.value)[i];
                key++;
            } else {
                if (inside_string == 0) {
                    inside_string = 1;
                } else {
                    inside_string = 0;
                }
                if (leave_quotes) {
                    (*key) = ((char *)item_list->item.value)[i];
                    key++;
                }
            }
        }
    }
    if (i == ECJP_MAX_KEY_LEN) {
        return ECJP_NO_SPACE_IN_BUFFER_VALUE;
    }
    (*key) = '\0';
    i++; // skip the colon

    for (j = 0; i < item_list->item.value_size && j < ECJP_MAX_KEY_VALUE_LEN; i++, j++) {
        (*value) = ((char *)item_list->item.value)[i];
        value++;
    }
    if (j == ECJP_MAX_KEY_VALUE_LEN) {
        return ECJP_NO_SPACE_IN_BUFFER_VALUE;
    }
    (*value) = '\0';

    return ECJP_NO_ERROR;
}

/*
 * Function: ecjp_read_element()
 * --------------------
 * Reads an element from the item list by its index and copies its value to the output structure.
 * Parameters:
 *      item_list: Pointer to the head of the ecjp_item_elem_t linked list.
 *      index: The index of the element to read.
 *      out: Pointer to an ecjp_outdata_t structure where the output will be stored.
 * Returns:
 *  ECJP_NO_ERROR on success
 *  ECJP_NULL_POINTER if item_list or out is NULL
 *  ECJP_INDEX_OUT_OF_BOUNDS if the index is out of bounds
*/
ecjp_return_code_t ecjp_read_element(ecjp_item_elem_t *item_list, int index, ecjp_outdata_t *out)
{
    int current_index;
    current_index = 0;

    if (item_list == NULL || out == NULL) {
        return ECJP_NULL_POINTER;
    }

    while ((item_list != NULL) && (current_index <= index)) {
        if (current_index == index) {
            ecjp_printf("%s - %d: Find element of index %d: Type = %s, Value = %s\n", __FUNCTION__, __LINE__,index, ecjp_type[item_list->item.type], (char *)item_list->item.value);
            if ((out->value != NULL) && (out->value_size >= item_list->item.value_size)) {
                memset(out->value, 0, out->value_size);
                out->type = item_list->item.type;
                out->value_size = item_list->item.value_size;
                memcpy(out->value, item_list->item.value, out->value_size);
            }
            break;
        }
        item_list = item_list->next;
        current_index++;
    }
    if (item_list == NULL) {
        return ECJP_INDEX_OUT_OF_BOUNDS;
    }

    return ECJP_NO_ERROR;
}

/*
 * Function: ecjp_read_key_2()
 * --------------------
 * Reads the value associated with a specified key from the item list, starting from a given index.
 * Parameters:
 *      item_list: Pointer to the head of the ecjp_item_elem_t linked list.
 *      key: The key to search for.
 *      index: The starting index for the search.
 *      out: Pointer to an ecjp_outdata_t structure where the output will be stored.
 * Returns:
 *  ECJP_NO_ERROR on success
 *  ECJP_NULL_POINTER if item_list, key, or out is NULL
 *  ECJP_INDEX_NOT_FOUND if the key is not found in the list
*/
ecjp_return_code_t ecjp_read_key_2(ecjp_item_elem_t *item_list, const char *key, unsigned int index, ecjp_outdata_t *out)
{
    ecjp_item_elem_t *current_item;
    int current_index;
    char extracted_key[ECJP_MAX_KEY_LEN] = {0};
    char extracted_value[ECJP_MAX_KEY_VALUE_LEN] = {0};
    ecjp_return_code_t split_res;

    current_index = 0;

    if (item_list == NULL || key == NULL || out == NULL) {
        return ECJP_NULL_POINTER;
    }
#ifdef DEBUG_VERBOSE
    ecjp_printf("%s - %d: Search for key: %s starting from index %d\n", __FUNCTION__, __LINE__, key, index);
#endif

    current_item = item_list;
    while (current_item != NULL) {
        split_res = ECJP_NO_ERROR;
        memset(extracted_key, 0, ECJP_MAX_KEY_LEN);
        memset(extracted_value, 0, ECJP_MAX_KEY_VALUE_LEN);

        if (current_index < index) {
            current_item = current_item->next;
            current_index++;
            continue;
        }
        if (current_item->item.type == ECJP_TYPE_KEY_VALUE_PAIR) {
            split_res = ecjp_split_key_and_value(current_item, extracted_key, extracted_value, ECJP_BOOL_FALSE);
            if (split_res != ECJP_NO_ERROR) {
                ecjp_printf("%s - %d: Fail to split key and value pair (res = %d)\n", __FUNCTION__, __LINE__,split_res);
                return split_res;
            }
#ifdef DEBUG_VERBOSE
            ecjp_printf("%s - %d: extracted_key = %s, extracted_value = %s\n", __FUNCTION__, __LINE__, extracted_key, extracted_value);
#endif
            if (strncmp(extracted_key, key, strlen(extracted_key)) == 0) {
#ifdef DEBUG_VERBOSE
                ecjp_printf("%s - %d: Found key: %s with value: %s\n", __FUNCTION__, __LINE__, extracted_key, extracted_value);
#endif
                if ((out->value != NULL) && (out->value_size >= current_item->item.value_size)) {
                    memset(out->value, 0, out->value_size);
                    out->type = current_item->item.type;
                    out->value_size = strlen(extracted_value) + 1;
                    memcpy(out->value, extracted_value, out->value_size);
                    out->last_pos = current_index;
                    out->error_code = ECJP_NO_ERROR;
                }
                return ECJP_NO_ERROR;
            }
        }
        current_item = current_item->next;
        current_index++;
    }
#ifdef DEBUG_VERBOSE
    ecjp_printf("%s - %d: Fail with error ECJP_INDEX_NOT_FOUND\n", __FUNCTION__, __LINE__);
#endif
    return ECJP_INDEX_NOT_FOUND;
}

#else

/* Internal function definitions */

/*
 * Function: ecjp_add_node_end()
        This function adds a new key token node to the end of the linked list.
        Parameters:
        - head: Pointer to the head of the linked list.
        - data: Pointer to the key token data to add.
        Returns:
        - 0 on success.
        - -1 on memory allocation failure.
*/
int ecjp_add_node_end(ecjp_key_elem_t **head, ecjp_key_token_t *data)
{
    ecjp_key_elem_t  *new_node;
    ecjp_key_elem_t  *current;

    new_node = (ecjp_key_elem_t *)malloc(sizeof(ecjp_key_elem_t));
    if (!new_node)
        return -1;
    new_node->key.start_pos = data->start_pos;
    new_node->key.length = data->length;
    new_node->key.type = data->type;
    new_node->next = NULL;
    if (*head == NULL)
    {
        *head = new_node;
    }
    else
    {
        current = *head;
        while (current->next != NULL)
            current = current->next;
        current->next = new_node;
    }
    return 0;
}

/*
 * Function: ecjp_internal_copy_array_element()
        This function copies an array element from a buffer to an output structure if it matches the requested index.
        Parameters:
        - buffer: The buffer containing the array element.
        - p_buffer: The size of the buffer.
        - index: The index of the requested array element.
        - num_elements: The current number of elements processed.
        - out: Pointer to an output structure to store the array element's details.
        Returns:
        - ECJP_INDEX_NOT_FOUND if the requested index does not match the current element.
        - ECJP_NO_ERROR if the requested element is found and copied to out.
*/
ecjp_return_code_t ecjp_internal_copy_array_element(char *buffer, unsigned int p_buffer, int index, int num_elements, ecjp_outdata_t *out)
{
    ecjp_return_code_t ret = ECJP_INDEX_NOT_FOUND;

    // check if this is the requested element
    if ((index != ECJP_ARRAY_NO_INDEX) && (num_elements - 1 == index)) {
        // found requested element
        if (p_buffer >= ECJP_MAX_ARRAY_ELEM_LEN) {
            ecjp_printf("%s - %d: Value buffer too small, truncated to %d bytes\n", __FUNCTION__,__LINE__,(ECJP_MAX_ARRAY_ELEM_LEN - 1));
        }
        // copy to out structure
        if ((out->value != NULL) && (out->value_size > 0)) {
            strncpy(out->value, buffer, (p_buffer < out->value_size) ? p_buffer : (out->value_size - 1));
            *(char *)(out->value + ((p_buffer < out->value_size) ? p_buffer : (out->value_size - 1))) = '\0'; // null terminate
            out->length = (p_buffer < out->value_size) ? p_buffer : (out->value_size - 1);
            out->error_code = ECJP_NO_ERROR;
            ret = out->error_code;
        } else {
            out->error_code = ECJP_NO_SPACE_IN_BUFFER_VALUE;
            ret = out->error_code;
        }
    }
    return ret;
}

/*
    Function: ecjp_print_keys()
        This function prints all keys stored in the linked list along with their types.
        Parameters:
        - input: The JSON-like input string.
        - key_list: Pointer to the linked list of keys.
        Returns:
        - ECJP_NO_ERROR on success.
        - ECJP_NULL_POINTER if input or key_list is NULL.
*/
ecjp_return_code_t  ecjp_print_keys(const char *input, ecjp_key_elem_t *key_list)
{
    ecjp_key_elem_t *current = key_list;
    int key_index = 0;
    char buffer[ECJP_MAX_KEY_LEN];

    if (input == NULL)
        return ECJP_NULL_POINTER;

    while (current != NULL) {
        if(current->key.length >= ECJP_MAX_KEY_LEN) {
            ecjp_printf("Key length exceeds maximum buffer size (%d), truncating output.\n", ECJP_MAX_KEY_LEN - 1);
        }
        memset(buffer, 0, sizeof(buffer));
        strncpy(buffer, &input[current->key.start_pos], (current->key.length < ECJP_MAX_KEY_LEN) ? current->key.length : (ECJP_MAX_KEY_LEN - 1));
        ecjp_printf("%s - %d: Key #%d: %s - Type: %s\n",__FUNCTION__,__LINE__,key_index,buffer,ecjp_type[current->key.type]);

        current = current->next;
        key_index++;
    }
    return ECJP_NO_ERROR;
};

/*
    Function: ecjp_free_key_list()
        This function frees the memory allocated for the linked list of keys.
        Parameters:
        - key_list: Pointer to a pointer to the linked list of keys.
        Returns:
        - ECJP_NO_ERROR on success.
*/
ecjp_return_code_t ecjp_free_key_list(ecjp_key_elem_t **key_list)
{
    ecjp_key_elem_t *current;
    ecjp_key_elem_t *next;

    if(key_list == NULL || *key_list == NULL)
        return ECJP_NO_ERROR;
        
    current = *key_list;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    *key_list = NULL;

    return ECJP_NO_ERROR;
}

/*
    Function: ecjp_get_key()
        This function retrieves a specific key from a JSON-like input string and stores its details in an output structure.
        Parameters:
        - input: The JSON-like input string.
        - key: The specific key to search for. If NULL, retrieves the next key in the list starting from 'start' position.
        - key_list: Pointer to a pointer to the linked list of keys.
        - start: The position to start searching from.
        - out: Pointer to an output structure to store the found key's details.
        Returns:
        - ECJP_NO_MORE_KEY if no more keys are found.
        - ECJP_NO_ERROR if the specified key is found and stored in out.
        - ECJP_GENERIC_ERROR if an error occurs.
        - ECJP_NULL_POINTER if any input pointer is NULL.
*/
ecjp_return_code_t ecjp_get_key(const char input[],char *key,ecjp_key_elem_t **key_list,ECJP_TYPE_POS_KEY start,ecjp_outdata_t *out)
{
    ecjp_key_elem_t *current = *key_list;
    char buffer[ECJP_MAX_KEY_LEN];
    unsigned char len;
    ecjp_return_code_t ret;

    ret = ECJP_GENERIC_ERROR;
    if (input == NULL || key_list == NULL || *key_list == NULL || out == NULL) {
        ret = ECJP_NULL_POINTER;
        return ret;
    }
    
    if (start != 0) {
        // walk the list to find the start position
        while ((current != NULL) && (current->key.start_pos != start)) {
            current = current->next;
        }
        // skip to next token
        current = current->next;
    }
    if(current == NULL) {
        ret = ECJP_NO_MORE_KEY;
        return ret;
    }
    
    while (current != NULL) {
        if(current->key.length >= ECJP_MAX_KEY_LEN) {
            ecjp_printf("%s - %d: Key length exceeds maximum buffer size (%d), truncating output.\n",__FUNCTION__,__LINE__,(ECJP_MAX_KEY_LEN - 1));
        }
        memset(buffer, 0, sizeof(buffer));
        len = (current->key.length < ECJP_MAX_KEY_LEN) ? current->key.length : (ECJP_MAX_KEY_LEN - 1);
        strncpy(buffer, &input[current->key.start_pos], len);
        if(key != NULL)
        {
            // search for specific key, return NO_ERROR only if key is found, no need to copy again in out.value
#ifdef DEBUG_VERBOSE
            ecjp_printf("%s - %d: Comparing key '%s' with stored key '%s' for %d bytes at pos %d\n",__FUNCTION__,__LINE__,key, buffer,len,current->key.start_pos);
#endif
            if (strncmp(buffer, key, len) == 0) {
                out->error_code = ECJP_NO_ERROR;
                out->type = current->key.type;
                out->last_pos = current->key.start_pos;
                out->length = current->key.length;
                ret = out->error_code;
                break;
            }
        } else {
#ifdef DEBUG_VERBOSE
            ecjp_printf("%s - %d: Find key '%s' stored at pos %d\n",__FUNCTION__,__LINE__,buffer,current->key.start_pos);
#endif
            out->error_code = ECJP_NO_ERROR;
            out->type = current->key.type;
            out->last_pos = current->key.start_pos;
            out->length = current->key.length;
            if ((out->value != NULL) && (out->value_size > 0)) {
                char *dst = (char *)out->value;
                size_t max = out->value_size - 1;
                if (len > max) {
                    len = max;
                }
                memcpy(dst, buffer, len);
                dst[len] = '\0'; // null terminate
            } else {
                out->error_code = ECJP_NO_SPACE_IN_BUFFER_VALUE;
            }
            ret = out->error_code;
            break;
        }
        current = current->next;
    }
    if (ret == ECJP_GENERIC_ERROR)
        ret = ECJP_NO_MORE_KEY;

    return ret;
}

/*
 Function: ecjp_read_key()
    This function reads the value associated with a specific key from a JSON-like input string.
    Parameters:
    - input: The JSON-like input string.
    - in: Pointer to an input structure containing key details.
    - out: Pointer to an output structure to store the key's value details.
    Returns:
    - ECJP_NO_ERROR if the key's value is successfully read and stored in out.
    - ECJP_NULL_POINTER if any input pointer is NULL.
    - ECJP_NO_SPACE_IN_BUFFER_VALUE if the output buffer is insufficient.
    - ECJP_EMPTY_STRING if the key has no associated value (should not happen in well-formed JSON).
*/
ecjp_return_code_t ecjp_read_key(const char input[],ecjp_indata_t *in,ecjp_outdata_t *out)
{
    ecjp_return_code_t ret = ECJP_NO_ERROR;
    char *ptr;
    char *ptr_value = NULL; // pointer to value start
    unsigned int vsize;
    unsigned short int open_brackets;

    if (in == NULL || out == NULL) {
        ecjp_printf("%s - %d: NULL pointer in/out",__FUNCTION__,__LINE__);
        ret = ECJP_NULL_POINTER;
        return ret;
    }
    if (input == NULL) {
        ecjp_printf("%s - %d: No input, NULL pointer",__FUNCTION__,__LINE__);
        ret = ECJP_NULL_POINTER;
        return ret;
    }

    if (in->type == ECJP_TYPE_UNDEFINED) {
        // skip research
        out->type = ECJP_TYPE_UNDEFINED;
        out->error_code = ECJP_EMPTY_STRING;
        ret = out->error_code;
        return ret;
    }

    if(out->value == NULL || out->value_size == 0) {
        ecjp_printf("%s - %d: No buffer supplied for store value\n", __FUNCTION__,__LINE__);
        out->error_code = ECJP_NO_SPACE_IN_BUFFER_VALUE;
        ret = out->error_code;
        return ret;
    }

    vsize = 0;
    open_brackets = 0;
    
    // set to input buffer start position
    ptr = (char *)&input[in->pos + in->length + 1]; // skip key and quote
    // find colon
    while (*ptr != ':' && *ptr != '\0') {
        ptr++;
    }
    if(*ptr == ':') {
        ptr++; // skip colon
    }
    // calculate value size
    switch (in->type) {
        case ECJP_TYPE_STRING:
            out->type = ECJP_TYPE_STRING;
            while(ecjp_is_whitespace(*ptr) == ECJP_BOOL_TRUE) {
                ptr++;
            }
            if (*ptr == '"') {
                ptr++;
                ptr_value = ptr;
                while (*ptr != '"' && *ptr != '\0') {
                    vsize++;
                    ptr++;
                }
                out->error_code = ECJP_NO_ERROR;
            }   
            break;

        case ECJP_TYPE_NUMBER:
            out->type = ECJP_TYPE_NUMBER;
            while(ecjp_is_whitespace(*ptr) == ECJP_BOOL_TRUE) {
                ptr++;
            }
            ptr_value = ptr;
            while ((*ptr >= '0' && *ptr <= '9') || *ptr == '-' || *ptr == '+' || *ptr == '.' || *ptr == 'e' || *ptr == 'E') {
                vsize++;
                ptr++;
            }
            out->error_code = ECJP_NO_ERROR;
            break;
        
        case ECJP_TYPE_BOOL:
            out->type = ECJP_TYPE_BOOL;
            while(ecjp_is_whitespace(*ptr) == ECJP_BOOL_TRUE) {
                ptr++;
            }
            ptr_value = ptr;
            if (strncmp(ptr, "true", 4) == 0 || strncmp(ptr, "false", 5) == 0) {
                vsize = (strncmp(ptr, "true", 4) == 0) ? 4 : 5;
            }
            out->error_code = ECJP_NO_ERROR;
            break;

        case ECJP_TYPE_NULL:
            out->type = ECJP_TYPE_NULL;
            while(ecjp_is_whitespace(*ptr) == ECJP_BOOL_TRUE) {
                ptr++;
            }
            ptr_value = ptr;
            vsize = 4; // "null"
            out->error_code = ECJP_NO_ERROR;
            break;

        case ECJP_TYPE_OBJECT:
            out->type = ECJP_TYPE_OBJECT;
            while(ecjp_is_whitespace(*ptr) == ECJP_BOOL_TRUE) {
                ptr++;
            }
            ptr_value = ptr;
            if(*ptr == '{') {
                vsize = 1; // at least the opening bracket
                open_brackets = 1;
                ptr++;
                while ((open_brackets != 0) && (*ptr != '\0')) {
                    if(*ptr == '}') {
                        open_brackets--;
                    } else if (*ptr == '{') {
                        open_brackets++;
                    }
                    vsize++;
                    ptr++;
                    out->error_code = ECJP_NO_ERROR;
                }
            } else {
                vsize = 0;
                out->error_code = ECJP_EMPTY_STRING;
            }   
            break;

        case ECJP_TYPE_ARRAY:
            out->type = ECJP_TYPE_ARRAY;
            while(ecjp_is_whitespace(*ptr) == ECJP_BOOL_TRUE) {
                ptr++;
            }
            ptr_value = ptr;
            if(*ptr == '[') {
                vsize = 1; // at least the opening bracket
                open_brackets = 1;
                ptr++;
                while ((open_brackets != 0) && (*ptr != '\0')) {
                    if(*ptr == ']') {
                        open_brackets--;
                    } else if (*ptr == '[') {
                        open_brackets++;
                    }   
                    vsize++;
                    ptr++;
                }
                out->error_code = ECJP_NO_ERROR;
            } else {
                vsize = 0;
                out->error_code = ECJP_EMPTY_STRING;
            }
            break;

        default:
            vsize = 0;
            out->type = ECJP_TYPE_UNDEFINED;
            out->error_code = ECJP_EMPTY_STRING;
            break;
    }
    // sanity check
    if (ptr_value == NULL) {
        ecjp_printf("%s - %d: Internal error, value pointer is NULL\n", __FUNCTION__,__LINE__);
        out->error_code = ECJP_GENERIC_ERROR;
        ret = out->error_code;
        return ret;
    }
    if (out->value_size <= vsize) {
        vsize = out->value_size - 1; // leave space for null terminator
        memcpy(out->value, ptr_value, out->value_size);
        ((char *)out->value)[vsize] = '\0';
        ecjp_printf("%s - %d: Value buffer too small, truncated to %d bytes\n", __FUNCTION__,__LINE__,(int)vsize);
        out->error_code = ECJP_NO_SPACE_IN_BUFFER_VALUE;
    }
    else
        memcpy(out->value, ptr_value, vsize);

    out->last_pos = in->pos;
    out->length = in->length;

#ifdef DEBUG_VERBOSE
    ecjp_printf("%s - %d: Find key %s of type %s with value: %s\n",__FUNCTION__,__LINE__,in->key,ecjp_type[out->type],(char *)out->value);
#endif

    ret = out->error_code;

    return ret;
}

/*
    Function: ecjp_get_keys_and_value()
        This function retrieves all keys and their associated values from a JSON-like input string and prints them to stdout.
        Parameters:
        - ptr: The JSON-like input string.
        - key_list: Pointer to the linked list of keys.
        Returns:
        - ECJP_NO_ERROR on success.
        - ECJP_NO_MORE_KEY when all keys have been processed.
*/
ecjp_return_code_t ecjp_get_keys_and_value(char *ptr,ecjp_key_elem_t *key_list)
{
    ecjp_outdata_t out_get;
    ecjp_outdata_t out_read, out_array;
    ecjp_indata_t in;
    ecjp_return_code_t ret;
    ECJP_TYPE_POS_KEY start = 0;
    int key_count = 0;

    memset(&out_get,0,sizeof(out_get));
    out_get.value = malloc(ECJP_MAX_KEY_LEN);
    out_get.value_size = ECJP_MAX_KEY_LEN;
    memset(out_get.value,0,out_get.value_size);

    memset(&out_read,0,sizeof(out_read));
    out_read.value = malloc(ECJP_MAX_KEY_VALUE_LEN);
    out_read.value_size = ECJP_MAX_KEY_VALUE_LEN;
    memset(out_read.value,0,out_read.value_size);

    memset(&out_array,0,sizeof(out_array));
    out_array.value = malloc(ECJP_MAX_ARRAY_ELEM_LEN);
    out_array.value_size = ECJP_MAX_ARRAY_ELEM_LEN;
    memset(out_array.value,0,out_array.value_size);

    memset(&in,0,sizeof(in));

    ret = ECJP_NO_ERROR;

    do {
        ret = ecjp_get_key(ptr,NULL,&key_list,start,&out_get);
        if (ret != ECJP_NO_MORE_KEY) {
            key_count++;
            fprintf(stdout,
                    "Find key #%d: \"%s\" [error_code=%d type=%s length=%d last_pos=%d] ",
                    key_count,
                    (char *)out_get.value,
                    out_get.error_code,
                    ecjp_type[out_get.type],
                    out_get.length,
                    out_get.last_pos);
            // read value for this key
            in.length = out_get.length;
            in.type = out_get.type;
            in.pos = out_get.last_pos;
            start = in.pos; // update start position for next search
            strncpy(in.key,out_get.value,out_get.value_size);
            if(ecjp_read_key(ptr,&in,&out_read) == ECJP_NO_ERROR) {
                fprintf(stdout,
                        "value = %s\n",
                        (char *)out_read.value);
                if (in.type == ECJP_TYPE_ARRAY) {
                    int index = 0;
                    while (ecjp_read_array_element(out_read.value,index,&out_array) == ECJP_NO_ERROR) {
                        fprintf(stdout, "\tArray element #%d Type = %s, Value = %s\n", index, ecjp_type[out_array.type], (char *)out_array.value);
                        // reset out_read
                        out_array.error_code = ECJP_NO_ERROR;
                        out_array.last_pos = 0;
                        out_array.length = 0;
                        out_array.type = ECJP_TYPE_UNDEFINED;
                        memset(out_array.value,0,out_array.value_size);
                        index++;
                    }
                }
            } else {
                fprintf(stdout,
                        "Error reading key %s. Error code = %d\n",
                        in.key,
                        out_read.error_code);
                break;
            }
            // reset out_get
            out_get.error_code = ECJP_NO_ERROR;
            out_get.type = ECJP_TYPE_UNDEFINED;
            out_get.length = 0;
            out_get.last_pos = 0;
            memset(out_get.value,0,out_get.value_size);
            // reset out_read
            out_read.error_code = ECJP_NO_ERROR;
            out_read.last_pos = 0;
            out_read.length = 0;
            out_read.type = ECJP_TYPE_UNDEFINED;
            memset(out_read.value,0,out_read.value_size);
            // reset in
            memset(&in,0,sizeof(in));   
        }        
    } while(ret != ECJP_NO_MORE_KEY);
    
    free(out_get.value);
    free(out_read.value);

    return ret;
}

/*
    Function: ecjp_read_array_element()
        This function reads a specific element from a JSON-like array string.
        Parameters:
        - input: The JSON-like array string.
        - index: The index of the element to read. If use ECJP_ARRAY_NO_INDEX the function returns an error.
        - out: Pointer to an output structure to store the element's details.
        Returns:
        - ECJP_NO_ERROR if the element is successfully read and stored in out.
        - ECJP_NULL_POINTER if any input pointer is NULL.
        - ECJP_EMPTY_STRING if no index is specified.
*/
ecjp_return_code_t ecjp_read_array_element(const char input[],int index,ecjp_outdata_t *out)
{
    ecjp_return_code_t ret = ECJP_NO_ERROR;
    ecjp_parser_data_t *p;
    ecjp_parser_data_t parser_data;
    char buffer[ECJP_MAX_ARRAY_ELEM_LEN];
    int p_buffer = 0;
    int num_elements = 0;

    memset(&parser_data, 0, sizeof(ecjp_parser_data_t));
    p =  &parser_data;
    p->parse_stack.top = -1;
    
    if (out == NULL) {
        ecjp_printf("%s - %d: NULL pointer out",__FUNCTION__,__LINE__);
        ret = ECJP_NULL_POINTER;
        return ret;
    }
    if (index == ECJP_ARRAY_NO_INDEX) {
        ecjp_printf("%s - %d: No index specified",__FUNCTION__,__LINE__);
        ret = ECJP_EMPTY_STRING;
        return ret;
    }

#ifdef DEBUG_VERBOSE        
    ecjp_printf("%s - %d: input %s\n", __FUNCTION__,__LINE__, input);
#endif

    memset(buffer, 0, sizeof(buffer));

    p->index = 0;
    p->flags.all = 0;
    p->status = ECJP_PS_START;
    while ((input[p->index] != '\0') && (p->status != ECJP_PA_END) && (p->status != ECJP_PA_ERROR)) {
        // walk through the string
#ifdef DEBUG_VERBOSE        
        ecjp_printf("%s - %d: Index %d, Status %d, Char '%c'\n", __FUNCTION__,__LINE__, p->index, p->status, input[p->index]);
#endif
        switch (p->status)
        {
            case ECJP_PA_START:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        break;
                        
                    case '{':
                        ecjp_printf("%s - %d: Input must cannot start with '{' in an array\n", __FUNCTION__,__LINE__);
                        p->status = ECJP_PA_ERROR;
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }   
                        p->status = ECJP_PA_IN_ARRAY;
                        break;

                    default:
                        ecjp_printf("%s - %d: Input must start with '['\n", __FUNCTION__,__LINE__);
                        p->status = ECJP_PA_ERROR;
                        break;
                }
                break;

            case ECJP_PA_IN_ARRAY:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        break;

                    case ']':
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            p->status = ECJP_PA_WAIT_COMMA;
                        }
                        break;

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            p->status = ECJP_PA_WAIT_COMMA;
                        }
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_ARRAY_ELEM;
                        out->type = ECJP_TYPE_ARRAY;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_OBJ_ELEM;
                        out->type = ECJP_TYPE_OBJECT;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;
                    
                    case '"':
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_IN_STRING;
                        out->type = ECJP_TYPE_STRING;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;

                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '-':
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_IN_NUMBER;
                        out->type = ECJP_TYPE_NUMBER;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;

                    case 't':
                    case 'f':
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_IN_BOOL;
                        out->type = ECJP_TYPE_BOOL;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;

                    case 'n':
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_IN_NULL;
                        out->type = ECJP_TYPE_NULL;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;

                    default:
                        ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                        p->status = ECJP_PA_ERROR;
                        break;
                }
                break;

            case ECJP_PA_IN_STRING:
                switch(input[p->index]) {
                    case '"':
                        // copy closing quote
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        p->status = ECJP_PA_WAIT_COMMA;
                        break;

                    default:
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;
                }
                break;

            case ECJP_PA_IN_NUMBER:
                switch(input[p->index]) {
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '-':
                    case '+':
                    case '.':
                    case 'e':
                    case 'E':
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;

                    case ',':
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        p->flags.trailing_comma = 1;
                        p->status = ECJP_PA_IN_ARRAY;
                        // check if this is the requested element
                        num_elements++;
                        ret = ecjp_internal_copy_array_element(buffer, p_buffer, index, num_elements, out);
                        if (ret != ECJP_INDEX_NOT_FOUND) {
                            // found requested element, can exit
                            return ret;
                        }
                        // reset buffer for next element
                        memset(buffer, 0, sizeof(buffer));
                        p_buffer = 0;
                        break;

                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // whitespace breaks number
                        p->status = ECJP_PA_WAIT_COMMA;
                        continue;

                    default:
                        ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                        p->status = ECJP_PA_ERROR;
                        break;
                }
                break;

            case ECJP_PA_IN_BOOL:
                if (buffer[p_buffer - 1] == 't') {
                    // already have first character
                    if (strncmp(&input[p->index], "rue", 3) == 0) {
                        // copy the remaing characters of "true"
                        buffer[p_buffer++] = 'r';
                        buffer[p_buffer++] = 'u';
                        buffer[p_buffer++] = 'e';
                        p->index += 3;
                        p->status = ECJP_PA_WAIT_COMMA;
                        continue;
                    } else {
                        ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                        p->status = ECJP_PA_ERROR;
                    }                
                } else if (buffer[p_buffer - 1] == 'f') {
                    // already have first character
                    if (strncmp(&input[p->index], "alse", 4) == 0) {
                        // copy the remaining characters of "false"
                        buffer[p_buffer++] = 'a';
                        buffer[p_buffer++] = 'l';
                        buffer[p_buffer++] = 's';
                        buffer[p_buffer++] = 'e';
                        p->index += 4;
                        p->status = ECJP_PA_WAIT_COMMA;
                        continue;
                    } else {
                        ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                        p->status = ECJP_PA_ERROR;
                    }
                } else {
                    ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                    p->status = ECJP_PA_ERROR;
                }   
                break;

            case ECJP_PA_IN_NULL:
                if (buffer[p_buffer - 1] == 'n' || buffer[p_buffer - 1] == 'N') {
                    // already have first character
                    if (strncmp(&input[p->index], "ull", 3) == 0) {
                        // copy the remaing characters of "null"
                        buffer[p_buffer++] = 'u';
                        buffer[p_buffer++] = 'l';
                        buffer[p_buffer++] = 'l';
                        p->index += 3;
                        p->status = ECJP_PA_WAIT_COMMA;
                        continue;
                    } else {
                        ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                        p->status = ECJP_PA_ERROR;
                    }                
                } else {
                    ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                    p->status = ECJP_PA_ERROR;
                }
                break;

            case ECJP_PA_OBJ_ELEM:
                switch(input[p->index]) {
                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            // copy character in buffer
                            buffer[p_buffer] = input[p->index];
                            p_buffer++;
                            // check if this bracket closes the current object
                            if (ecjp_peek_parse_stack(&(p->parse_stack),'[') == ECJP_BOOL_TRUE && p->open_square_brackets == 1) {
                                p->status = ECJP_PA_WAIT_COMMA;
                            }
                        }
                        break;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }   
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }   
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;

                    case ']':
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            // copy character in buffer
                            buffer[p_buffer] = input[p->index];
                            p_buffer++;
                        }
                        break;

                    default:
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;
                }
                break;

            case ECJP_PA_ARRAY_ELEM:
                switch (input[p->index]) {
                    case ']':
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            // copy character in buffer
                            buffer[p_buffer] = input[p->index];
                            p_buffer++;
                            // check if this bracket closes the current array
                            if (p->open_square_brackets == 1) {
                                p->status = ECJP_PA_WAIT_COMMA;
                            }
                        }
                        break;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }   
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            p->status = ECJP_PA_ERROR;
                            break;
                        }   
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            // copy character in buffer
                            buffer[p_buffer] = input[p->index];
                            p_buffer++;
                        }
                        break;

                    default:
                        // copy character in buffer
                        buffer[p_buffer] = input[p->index];
                        p_buffer++;
                        break;
                }
                break;

            case ECJP_PA_WAIT_COMMA:
                switch(input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        break;
                        
                    case ',':
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        p->flags.trailing_comma = 1;
                        p->status = ECJP_PA_IN_ARRAY;
                        // check if this is the requested element
                        num_elements++;
                        ret = ecjp_internal_copy_array_element(buffer, p_buffer, index, num_elements, out);
                        if (ret != ECJP_INDEX_NOT_FOUND) {
                            return ret;
                        }
                        // reset buffer for next element
                        memset(buffer, 0, sizeof(buffer));
                        p_buffer = 0;
                        break;

                    case '[':
                    case '{':
                        ecjp_printf("%s - %d: Character %c unexpected\n", __FUNCTION__,__LINE__,input[p->index]);
                        p->status = ECJP_PA_ERROR;
                        break;

                    case ']':
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            p->status = ECJP_PA_IN_ARRAY;
                        }   
                        break;

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->flags.trailing_comma) {
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            p->status = ECJP_PA_ERROR;
                            break;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PA_END;
                        } else {
                            p->status = ECJP_PA_IN_ARRAY;
                        }
                        break;

                    default:
                        ecjp_printf("%s - %d: Character %c unexpected (index %d)\n", __FUNCTION__,__LINE__,input[p->index],p->index);
                        p->status = ECJP_PA_ERROR;
                        break;
                }
                break;

            case ECJP_PA_ERROR:
                ecjp_printf("%s - %d: Fail to parse the array\n", __FUNCTION__,__LINE__);
                ret = ECJP_SYNTAX_ERROR;
                return ret;
                break;

            case ECJP_PA_END:
                // copy the last element if needed
                // check if this is the requested element
                num_elements++;
                ret = ecjp_internal_copy_array_element(buffer, p_buffer, index, num_elements, out);
                if (ret != ECJP_INDEX_NOT_FOUND) {
                    return ret;
                }
                // reset buffer for next element
                memset(buffer, 0, sizeof(buffer));
                p_buffer = 0;
                // reached end of parsing
#ifdef DEBUG_VERBOSE
                ecjp_printf("%s - %d: End of array parsing\n", __FUNCTION__,__LINE__);
#endif
                break;

            default:
                break;
        }
        p->index++;
    }

    // check if we reach the end of parsing without manage the last state
    if (p->status == ECJP_PA_END && p_buffer != 0) {
        // copy the last element if needed
        // check if this is the requested element
        num_elements++;
        ret = ecjp_internal_copy_array_element(buffer, p_buffer, index, num_elements, out);
#ifdef DEBUG_VERBOSE
        ecjp_printf("%s - %d: End of array parsing\n", __FUNCTION__,__LINE__);
#endif
    } else {
        if (p->status != ECJP_PA_END) {
            ecjp_printf("%s - %d: Incomplete JSON structure\n", __FUNCTION__,__LINE__);
            return ECJP_SYNTAX_ERROR;
        }
        if ((p->open_brackets != 0) || (p->open_square_brackets != 0)) {
            ecjp_printf("%s - %d: Mismatched brackets at end of input\n", __FUNCTION__,__LINE__);
            return ECJP_BRACKETS_MISSING;
        }
    }
    
    if (index > (num_elements - 1)) {
#ifdef DEBUG_VERBOSE
        ecjp_printf("%s - %d: Requested index %d exceeds number of elements %d\n", __FUNCTION__,__LINE__,index,(num_elements-1));
#endif
        return ECJP_INDEX_OUT_OF_BOUNDS;
    }  

    return ret;
}

/*
    Function: ecjp_check_and_load()
        This function checks the syntax of a JSON-like input string and prepares it for further processing.
        Parameters:
        - input: The JSON-like input string to be checked and loaded.
        - key_list: Pointer to a list of key elements loaded with the keys found in the input string.
        - res: Pointer to a structure to store the result of the check, including any error position.
        - level: The level of checking to be performed; used to manage keys inside nested structures.
        Returns:
        - ECJP_NO_ERROR if the input string is valid.
        - ECJP_NULL_POINTER if any input pointer is NULL.
        - ECJP_EMPTY_STRING if the input string is empty.
        - ECJP_SYNTAX_ERROR if there is a syntax error in the input string.
*/
ecjp_return_code_t ecjp_check_and_load(const char *input, ecjp_key_elem_t **key_list, ecjp_check_result_t *res, unsigned short int level)
{
    ecjp_parser_data_t parser_data;
    ecjp_parser_data_t *p;
    ecjp_key_token_t key_token;

    memset(&key_token, 0, sizeof(ecjp_key_token_t));
    memset(&parser_data, 0, sizeof(ecjp_parser_data_t));
    p =  &parser_data;
    p->parse_stack.top = -1;

    if ((input == NULL) || (res == NULL)) {
        ecjp_printf("%s - %d: NULL pointer input/res\n",__FUNCTION__,__LINE__);
        return ECJP_NULL_POINTER;
    }
    if (strlen(input) == 0) {
        ecjp_printf("%s - %d: Empty string input\n",__FUNCTION__,__LINE__);
        return ECJP_EMPTY_STRING;
    }
#ifdef DEBUG_VERBOSE
    ecjp_printf("%s - %d:\nInput string: %s\n",__FUNCTION__,__LINE__,input);
#endif

    p->index = 0;
    p->flags.all = 0;
    p->status = ECJP_PS_START;
    while (input[p->index] != '\0') {
        // walk through the string
#ifdef DEBUG_VERBOSE        
        ecjp_printf("%s - %d: Index %d, Status %d, Char '%c'\n", __FUNCTION__,__LINE__, p->index, p->status, input[p->index]);
#endif
        switch (p->status)
        {
            case ECJP_PS_START:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }   
                        p->status = ECJP_PS_IN_OBJECT;
                        res->struct_type = ECJP_ST_OBJ;
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }   
                        p->status = ECJP_PS_IN_ARRAY;
                        res->struct_type = ECJP_ST_ARRAY;
                        break;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Input must start with '{' or ']'\n", __FUNCTION__,__LINE__);
                        return ECJP_SYNTAX_ERROR;
                        break;
                }
                break;
 
            case ECJP_PS_IN_OBJECT:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if(ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_OBJECT;
                        break;  

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PS_END;
                        } else {
                            p->status = ECJP_PS_WAIT_COMMA;
                        }
                        break;

                    case ',':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->flags.trailing_comma = 1;
                        break;

                    case '"':
                        p->flags.in_string = 1;
                        p->status = ECJP_PS_IN_KEY;
                        p->flags.in_key = 1;
                        p->flags.trailing_comma = 0;
                        // Record key position
                        key_token.start_pos = (p->index + 1) ; // skip initial quote
                        break;
                    
                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Expected string key or closing bracket, receive: %c\n", __FUNCTION__,__LINE__,input[p->index]);
                        return ECJP_SYNTAX_ERROR;
                        break;  
                }
                break;
            
            case ECJP_PS_IN_ARRAY:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_OBJECT;
                        if(p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }   
                        break;

                    case '}':
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->status = ECJP_PS_WAIT_COMMA;
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_ARRAY;
                        if (p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }   
                        break;

                    case ']':
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->status = ECJP_PS_WAIT_COMMA;
                        break;
                    
                    case '"':
                        p->flags.in_string = 1;
                        p->status = ECJP_PS_IN_VALUE;
                        if (p->flags.trailing_comma) {
                            p->flags.trailing_comma = 0;
                        }
                        break;

                    default:
                        if (strncmp(&input[p->index], "true", 4) == 0 || strncmp(&input[p->index], "false", 5) == 0 || strncmp(&input[p->index], "null", 4) == 0) {
                            // valid value
                            p->status = ECJP_PS_WAIT_COMMA;
                            p->index += (input[p->index] == 'f') ? 5 : 4; // move index forward
                            if (p->flags.trailing_comma) {
                                p->flags.trailing_comma = 0;
                            }   
                            continue;
                        }
                        if ((input[p->index] >= '0' && input[p->index] <= '9') || input[p->index] == '-') {
                            // valid value start
                            p->flags.in_number = 1;
                            if (input[p->index] == '0') {
                                p->flags.start_zero = 1;
                            }
                            p->status = ECJP_PS_IN_VALUE;
                            if (p->flags.trailing_comma) {
                                p->flags.trailing_comma = 0;
                            }       
                        } else {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Invalid character in value\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        break;
                }
                break;
            
            case ECJP_PS_IN_KEY:
                // in this status we are always inside a string (p->flags.in_string == 1) 
                switch (input[p->index]) {
                    case '\\':
                        // skip escaped character
                        p->index++;
                        switch(input[p->index]) {
                            case '"':
                            case '\\':
                            case '/':
                            case 'b':
                            case 'f':
                            case 'r':
                            case 'n':
                            case 't':
                                p->index++;
                                continue;

                            case 'u':
                                if(ecjp_is_excode(input[p->index+1]) &&
                                   ecjp_is_excode(input[p->index+2]) &&
                                   ecjp_is_excode(input[p->index+3]) &&
                                   ecjp_is_excode(input[p->index+4])) {
                                    p->index += 4;
                                } else {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Invalid character in unicode sequence\n", __FUNCTION__,__LINE__);
                                    return ECJP_SYNTAX_ERROR;
                                }
                                break;

                            default:
                                res->err_pos = p->index;
                                ecjp_printf("%s - %d: Invalid character in escape sequence (%c)\n", __FUNCTION__,__LINE__, input[p->index]);
                                return ECJP_SYNTAX_ERROR;
                                break;
                        }
                        break;

                    case '"':
                        p->flags.in_string = 0;
                        p->status = ECJP_PS_WAIT_COLON;
                        p->flags.in_key = 0;
                        break;

                    default:
                        if(ecjp_is_ctrl(input[p->index])) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Invalid control character in key\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        } else {
                            // continue in key
                            key_token.length++;
                        }
                        break;
                }
                break;

            case ECJP_PS_WAIT_COLON:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case ':':
                        p->status = ECJP_PS_WAIT_VALUE;
                        break;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Expected colon after key, received: %c\n",__FUNCTION__,__LINE__,input[p->index]);
                        return ECJP_SYNTAX_ERROR;
                        break;
                }
                break;

            case ECJP_PS_WAIT_VALUE:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    case '{':
                        p->open_brackets++;
                        p->num_objects++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_OBJECT;
                        // Record key type
                        key_token.type = ECJP_TYPE_OBJECT;
                        // Add key token to the list
                        if (key_list != NULL && p->open_brackets <= level)
                        {
                            if (ecjp_add_node_end(key_list, &key_token) != 0) {
                                res->err_pos = p->index;
                                ecjp_printf("%s - %d: Failed to add key token to the list\n", __FUNCTION__,__LINE__);
                                return ECJP_GENERIC_ERROR;
                            } else {
#ifdef DEBUG_VERBOSE
                                ecjp_printf("%s - %d: Added object key token to the list, key_list = %p\n", __FUNCTION__,__LINE__, (void *)*key_list);
#endif
                                res->num_keys++;
                            }
                        }
                        // Reset key_token for future keys
                        memset(&key_token, 0, sizeof(ecjp_key_token_t));
                        break;

                    case '[':
                        p->open_square_brackets++;
                        p->num_arrays++;
                        if (ecjp_push_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            return ECJP_GENERIC_ERROR;
                        }
                        p->status = ECJP_PS_IN_ARRAY;
                        // Record key type
                        key_token.type = ECJP_TYPE_ARRAY;
                        // Add key token to the list
                        if (key_list != NULL && p->open_brackets <= level)
                        {
                            if (ecjp_add_node_end(key_list, &key_token) != 0) {
                                res->err_pos = p->index;
                                ecjp_printf("%s - %d: Failed to add key token to the list\n", __FUNCTION__,__LINE__);
                                return ECJP_GENERIC_ERROR;
                            } else {
#ifdef DEBUG_VERBOSE
                                ecjp_printf("%s - %d: Added object key token to the list, key_list = %p\n", __FUNCTION__,__LINE__, (void *)*key_list);
#endif
                                res->num_keys++;
                            }
                        }
                        // Reset key_token for future keys
                        memset(&key_token, 0, sizeof(ecjp_key_token_t));
                        break;

                    case '"':
                        p->flags.in_string = 1;
                        p->status = ECJP_PS_IN_VALUE;
                        // Record key type
                        key_token.type = ECJP_TYPE_STRING;
                        // Add key token to the list
                        if (key_list != NULL && p->open_brackets <= level)
                        {
                            if (ecjp_add_node_end(key_list, &key_token) != 0) {
                                res->err_pos = p->index;
                                ecjp_printf("%s - %d: Failed to add key token to the list\n", __FUNCTION__,__LINE__);
                                return ECJP_GENERIC_ERROR;
                            } else {
#ifdef DEBUG_VERBOSE
                                ecjp_printf("%s - %d: Added object key token to the list, key_list = %p\n", __FUNCTION__,__LINE__, (void *)*key_list);
#endif
                                res->num_keys++;
                            }
                        }
                        // Reset key_token for future keys
                        memset(&key_token, 0, sizeof(ecjp_key_token_t));
                        break;

                    default:
                        if (strncmp(&input[p->index], "true", 4) == 0 || strncmp(&input[p->index], "false", 5) == 0 || strncmp(&input[p->index], "null", 4) == 0) {
                            // valid value
                            p->status = ECJP_PS_WAIT_COMMA;
                            // Record key type
                            if ((input[p->index] == 't') || (input[p->index] == 'f')) {
                                key_token.type = ECJP_TYPE_BOOL;
                            } else {
                                key_token.type = ECJP_TYPE_NULL;
                            }
                            // Add key token to the list
                            if (key_list != NULL && p->open_brackets <= level)
                            {
                                if (ecjp_add_node_end(key_list, &key_token) != 0) {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Failed to add key token to the list\n", __FUNCTION__,__LINE__);
                                    return ECJP_GENERIC_ERROR;
                                } else {
#ifdef DEBUG_VERBOSE
                                    ecjp_printf("%s - %d: Added object key token to the list, key_list = %p\n", __FUNCTION__,__LINE__, (void *)*key_list);
#endif
                                    res->num_keys++;
                                }
                            }
                            // Reset key_token for future keys
                            memset(&key_token, 0, sizeof(ecjp_key_token_t));
                            p->index += (input[p->index] == 'f') ? 5 : 4; // move index forward
                            continue;
                        }
                        if ((input[p->index] >= '0' && input[p->index] <= '9') || input[p->index] == '-') {
                            // valid value start
                            p->flags.in_number = 1;
                            if (input[p->index] == '0') {
                                p->flags.start_zero = 1;
                            }
                            p->status = ECJP_PS_IN_VALUE;
                            // Record key type
                            key_token.type = ECJP_TYPE_NUMBER;
                            // Add key token to the list
                            if (key_list != NULL && p->open_brackets <= level)
                            {
                                if (ecjp_add_node_end(key_list, &key_token) != 0) {
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Failed to add key token to the list\n", __FUNCTION__,__LINE__);
                                    return ECJP_GENERIC_ERROR;
                                } else {
#ifdef DEBUG_VERBOSE
                                    ecjp_printf("%s - %d: Added object key token to the list, key_list = %p\n", __FUNCTION__,__LINE__, (void *)*key_list);
#endif  
                                    res->num_keys++;
                                }
                            }
                            // Reset key_token for future keys
                            memset(&key_token, 0, sizeof(ecjp_key_token_t));
                        } else {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Invalid character in value\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        break;
                }
                break;

            case ECJP_PS_IN_VALUE:
                switch(input[p->index]) {
                    case '"':
                        if(!p->flags.in_string) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected quote in value\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->flags.in_string = 0;
                        p->status = ECJP_PS_WAIT_COMMA;
                        break;

                    default:
                        if (p->flags.in_string) {
                            switch (input[p->index]) {
                                case '\\':
                                    // skip escaped character
                                    p->index++;
                                    switch(input[p->index]) {
                                        case '"':
                                        case '\\':
                                        case '/':
                                        case 'b':
                                        case 'f':
                                        case 'r':
                                        case 'n':
                                        case 't':
                                            p->index++;
                                            continue;

                                        case 'u':
                                            if(ecjp_is_excode(input[p->index+1]) &&
                                               ecjp_is_excode(input[p->index+2]) &&
                                               ecjp_is_excode(input[p->index+3]) &&
                                               ecjp_is_excode(input[p->index+4])) {
                                                p->index += 4;
                                            } else {
                                                res->err_pos = p->index;
                                                ecjp_printf("%s - %d: Invalid character in unicode sequence\n", __FUNCTION__,__LINE__);
                                                return ECJP_SYNTAX_ERROR;
                                            }
                                            break;

                                        default:
                                            res->err_pos = p->index;
                                            ecjp_printf("%s - %d: Invalid character in escape sequence (%c)\n", __FUNCTION__,__LINE__, input[p->index]);
                                            return ECJP_SYNTAX_ERROR;
                                            break;
                                    }
                                    break;

                                default:
                                    if (ecjp_is_ctrl(input[p->index])) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Invalid control character inside value\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    break;
                            }
                        } else {
                            switch (input[p->index]) {
                                case ' ':
                                case '\n':
                                case '\r':
                                case '\t':
                                    if (p->flags.in_number) {
                                        p->flags.in_number = 0;
                                        p->flags.start_zero = 0;
                                        p->status = ECJP_PS_WAIT_COMMA;
                                    }
                                    else {
                                        // skip whitespace
                                        p->index++;
                                        continue;
                                    }
                                    break;
                                
                                case ',':
                                    if(p->flags.trailing_comma) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    if (p->flags.in_number) {
                                        p->flags.in_number = 0;
                                        p->flags.start_zero = 0;
                                    }
                                    p->flags.trailing_comma = 1;
                                    if (ecjp_peek_parse_stack(&(p->parse_stack),'[') == ECJP_BOOL_TRUE) {
                                        p->status = ECJP_PS_IN_ARRAY;
                                    } else {
                                        p->status = ECJP_PS_IN_OBJECT;
                                    }
                                    break;
                                
                                case '0':
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                case '5':
                                case '6':
                                case '7':
                                case '8':
                                case '9':
                                case '.':
                                case 'e':
                                case 'E':
                                case '+':
                                case '-':
                                    if (p->flags.in_number) {
                                        // valid number continuation
                                        if (input[p->index] == '.') {
                                            if (p->flags.start_zero == 1) {
                                                p->flags.start_zero = 0;
                                            } 
                                            break;
                                        } else {
                                            if (p->flags.start_zero == 1) {
                                                // number can start with '0' unless is decimal value
                                                res->err_pos = p->index;
                                                ecjp_printf("%s - %d: Number can't start with value 0\n", __FUNCTION__,__LINE__);
                                                return ECJP_SYNTAX_ERROR;
                                            }
                                            break;
                                        }
                                    } else {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Unexpected number character in value\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    break;

                                case '}':
                                    if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    p->open_brackets--;
                                    if (p->open_brackets < 0) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                                        p->status = ECJP_PS_END;
                                    } else {
                                        p->status = ECJP_PS_WAIT_COMMA;
                                    }
                                    break;

                                case ']':
                                    if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    p->open_square_brackets--;
                                    if (p->open_square_brackets < 0) {
                                        res->err_pos = p->index;
                                        ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                                        return ECJP_SYNTAX_ERROR;
                                    }
                                    if (p->open_square_brackets == 0 && p->open_brackets == 0) {
                                        p->status = ECJP_PS_END;
                                    } else {
                                        p->status = ECJP_PS_WAIT_COMMA;
                                    }
                                    break;

                                default:
                                    res->err_pos = p->index;
                                    ecjp_printf("%s - %d: Invalid character in value\n", __FUNCTION__,__LINE__);
                                    return ECJP_SYNTAX_ERROR;
                                    break;
                            }
                        }
                        break;
                }
                break;
                        
            case ECJP_PS_WAIT_COMMA:
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;
                    
                    case '}':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Trailing comma before closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '{') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->open_brackets--;
                        if (p->open_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PS_END;
                        }
                        break;

                    case ']':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Trailing comma before closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_pop_parse_stack(&(p->parse_stack), '[') == ECJP_BOOL_FALSE) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Unexpected closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        p->open_square_brackets--;
                        if (p->open_square_brackets < 0) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Mismatched closing square bracket\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (p->open_brackets == 0 && p->open_square_brackets == 0) {
                            p->status = ECJP_PS_END;
                        }
                        break;

                    case ',':
                        if (p->flags.trailing_comma) {
                            res->err_pos = p->index;
                            ecjp_printf("%s - %d: Multiple trailing commas\n", __FUNCTION__,__LINE__);
                            return ECJP_SYNTAX_ERROR;
                        }
                        if (ecjp_peek_parse_stack(&(p->parse_stack),'[') == ECJP_BOOL_TRUE) {
                            p->status = ECJP_PS_IN_ARRAY;
                        } else {
                            p->status = ECJP_PS_IN_OBJECT;
                        }
                        p->flags.trailing_comma = 1;
                        break;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Expected comma after value\n", __FUNCTION__,__LINE__);
                        return ECJP_SYNTAX_ERROR;
                        break;
                }   
                break;
        
            case ECJP_PS_END:
                // should be only whitespace after end
                switch (input[p->index]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // skip whitespace
                        p->index++;
                        continue;

                    default:
                        res->err_pos = p->index;
                        ecjp_printf("%s - %d: Unexpected character after end of JSON\n", __FUNCTION__,__LINE__);
                        return ECJP_SYNTAX_ERROR;
                }
                break;
        
            default:
                break;
        }

        p->index++;
    }

    if (p->status != ECJP_PS_END) {
        ecjp_printf("%s - %d: Incomplete JSON structure\n", __FUNCTION__,__LINE__);
        return ECJP_SYNTAX_ERROR;
    }
    if ((p->open_brackets != 0) || (p->open_square_brackets != 0)) {
        res->err_pos = (p->index - 1);
        ecjp_printf("%s - %d: Mismatched brackets at end of input\n", __FUNCTION__,__LINE__);
        return ECJP_BRACKETS_MISSING;
    }

#ifdef DEBUG_VERBOSE
    ecjp_print_check_summary(p);
#endif

    return ECJP_NO_ERROR;
};

/*
    Function: ecjp_check_syntax
    This function call ecjp_check_and_load() without pointer to store the keys to perform only syntax checking.
    Parameters:
    - input: The JSON-like input string to be checked and loaded.
    - res: Pointer to a structure to store the result of the check, including any error position.
    Returns:
    - ECJP_NO_ERROR if the input string is valid.
    - ECJP_NULL_POINTER if any input pointer is NULL.
    - ECJP_EMPTY_STRING if the input string is empty.
    - ECJP_SYNTAX_ERROR if there is a syntax error in the input string.
*/
ecjp_return_code_t ecjp_check_syntax(const char *input, ecjp_check_result_t *res)
{
    return ecjp_check_and_load(input, NULL, res, 0);
}

/*
    Function: ecjp_load
    This function call ecjp_check_and_load() with pointer to store the keys to perform syntax checking
    and store all keys found in the input string at nested level less than or equal to the specified level.
    Parameters:
    - input: The JSON-like input string to be checked and loaded.
    - key_list: Pointer to a list of key elements loaded with the keys found in the input string.
    - res: Pointer to a structure to store the result of the check, including any error position.
    - level: The level of checking to be performed; used to manage keys inside nested structures.
    Returns:
    - ECJP_NO_ERROR if the input string is valid.
    - ECJP_NULL_POINTER if any input pointer is NULL.
    - ECJP_EMPTY_STRING if the input string is empty.
    - ECJP_SYNTAX_ERROR if there is a syntax error in the input string.
*/
ecjp_return_code_t ecjp_load(const char *input, ecjp_key_elem_t **key_list, ecjp_check_result_t *res, unsigned short int level)
{
    return ecjp_check_and_load(input, key_list, res, level);
}

#endif // ECJP_TOKEN_LIST

/* TO DO: work in progress */
#if 0
ecjp_return_code_t ecjp_read_key_val_string(const char *input, ecjp_key_elem_t **key_list, char *string)
{
    return ECJP_NO_ERROR;
}

ecjp_return_code_t ecjp_read_key_val_number(const char *input, ecjp_key_elem_t **key_list, float *number)
{
    return ECJP_NO_ERROR;
}

ecjp_return_code_t ecjp_read_key_val_bool(const char *input, ecjp_key_elem_t **key_list, bool *boolean)
{
    return ECJP_NO_ERROR;
}   
#endif


