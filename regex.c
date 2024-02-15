#include "main.h"

int strtok_regex(regex_t* __compiled, char* __str, int* offset, char* __store){
    regmatch_t match_array[1];
    char* cursor = __str + *offset;

    if(regexec(__compiled, cursor, 1, match_array, 0)){
        // No match
        return -1;
    };
    int i = 0;
    for(i = match_array[0].rm_so; i < match_array[0].rm_eo; i++){
        __store[i - match_array[0].rm_so] = cursor[i];
        __store[(i - match_array[0].rm_so) + 1] = '\0'; // please kill me
    }
    *offset += match_array[0].rm_eo;
}