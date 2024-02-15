#include "main.h"

FILE* get_script(char* argv[]){
    FILE* SCRIPT = NULL;
    if(argv[1] != NULL){
        printf("argument detected\n");
        char* absolute_path = NULL;
        if(access(argv[1], F_OK)!= 0){
            char* absolute_path = getcwd(NULL, BUF_SIZE);
            strcat(absolute_path, "/");
            strcat(absolute_path, argv[1]);
        }else{
            absolute_path = argv[1];
        }
        printf("SCRIPT %s\n", absolute_path);
        if(access(absolute_path, X_OK) == 0){
            printf("access ok\n");
            SCRIPT = fopen(absolute_path, "r"); 
        }else{
            printf("execution not possible...");
        }
    }
    return SCRIPT;
}