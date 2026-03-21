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

#ifndef ECJP_LIMIT_H
#define ECJP_LIMIT_H

#include "config.h"

#ifdef ECJP_RUN_ON_PC
#define ECJP_MAX_INPUT_SIZE          5*1024*1024 // 5 MB
// NOTE: if I set ECJP_MAX_INPUT_SIZE too high, 10 MB for example, the stack allocation fails
#define ECJP_MAX_PRINT_COLUMNS       80
#define ECJP_MAX_PARSE_STACK_DEPTH   2048
#define ECJP_MAX_KEY_LEN             512
#define ECJP_MAX_KEY_VALUE_LEN       1024*16 // 16 kB
#define ECJP_MAX_ARRAY_ELEM_LEN      1024*100// 100 kB
#define ECJP_MAX_ITEM_LEN            1024*100// 100 kB
#define ECJP_MAX_NESTED_LEVEL        1024

#define ECJP_TYPE_POS_KEY            unsigned short int
#define ECJP_TYPE_LEN_KEY            unsigned short int  

#else
    #ifdef ECJP_RUN_ON_MCU
        #define ECJP_MAX_INPUT_SIZE          1024 // 1 kB
        #define ECJP_MAX_PRINT_COLUMNS       80
        #define ECJP_MAX_PARSE_STACK_DEPTH   64
        #define ECJP_MAX_KEY_LEN             32
        #define ECJP_MAX_KEY_VALUE_LEN       128
        #define ECJP_MAX_ARRAY_ELEM_LEN      256
        #define ECJP_MAX_ITEM_LEN            512
        #define ECJP_MAX_NESTED_LEVEL        8

        #define ECJP_TYPE_POS_KEY            unsigned short int  
        #define ECJP_TYPE_LEN_KEY            unsigned char  

    #else
        // run with default
        #define ECJP_MAX_INPUT_SIZE          8192 // 8 kB
        #define ECJP_MAX_PRINT_COLUMNS       80
        #define ECJP_MAX_PARSE_STACK_DEPTH   128
        #define ECJP_MAX_KEY_LEN             64
        #define ECJP_MAX_KEY_VALUE_LEN       1024 // 1 kB
        #define ECJP_MAX_ITEM_LEN            512
        #define ECJP_MAX_ARRAY_ELEM_LEN      1024
        #define ECJP_MAX_NESTED_LEVEL        12

        #define ECJP_TYPE_POS_KEY            unsigned short int  
        #define ECJP_TYPE_LEN_KEY            unsigned char  

    #endif // ECJP_RUN_ON_MCU

#endif // ECJP_RUN_ON_PC

#endif // ECJP_LIMIT_H
