#include "main.h"

//#include <signal.h>

// TODOS
// - Implement file redirection
// OPTIONAL TODOS
// - Use editline() to allow scrolling through the history store.
// - Use readline() to allow for tab completion


// COMPLETED
// - Implement pipes
// - Implement DOT command
// - Implement "history" command. Requires storing each previous command string
// - Implement cd, which alters the Current Working Directory
// - Implement "! number", which executes command "number" in history store.


static char _PROMPT[] = "wish $ ";
char buffer[BUF_SIZE];
char* path;
FILE* SCRIPT;

struct History history;

void argv_debug(int __argc, char** __argv){
        printf("ARGC: %i\n", __argc);
        printf("ARGV:\n");
        for(int i = 0; i < __argc; i++){
            printf("\t%s\n", __argv[i]);
            fflush(stdout);
        }
}

void pipe_id_debug(int id_set[1024][2], int size){
    for(int i = 0; i < size; i++){
        printf("PIPE[%i]\n", i);
        printf("    [0]: %x\n", id_set[i][0]);
        printf("    [1]: %x\n", id_set[i][1]);
    }
}

void prompt(){
    printf("%s", _PROMPT);
    fflush(stdout);
    read(STDIN_FILENO, buffer, BUF_SIZE);
}

int redirection_flags(char* __str){
    if(strchr(__str, '|') != NULL){
        return 0;
    }else if(strchr(__str, '>') != NULL){
        return 1;
    }else if(strchr(__str, '<') != NULL){
        return 2;
    }

    return -1;
};

void execution();

int main(int argc, char* argv[]){
    path = getenv("PATH");
    argv_debug(argc, argv);
    SCRIPT = NULL;

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
            exit(-1);
        }
    }

    history = History_init();

    while(true){
        memset(buffer, 0, BUF_SIZE);
        while(wait(NULL)>0); // wait for all children to die.

        if(SCRIPT != NULL){ // are we running a script?
            char* line = NULL;
            size_t len = 0;
            if(getline(&line, &len, SCRIPT) != -1){
                strcpy(buffer, line);
            }else{
                exit(0);
            }
        }else{
            prompt();
        }

        // char* argv[1024];
begin_parse:
        int pipec = 0;
        char* pipes[1024];

        char* saveptr_args;
        char* saveptr_pipes;

        char** argvs[64];

        History_append(&history, buffer);

        if(buffer[0] == '#'){ // comment, ignore
            continue;
        }
        if(buffer[0] == '.'){ // dot command. Insert current working directory at '.'
            char tmp[BUF_SIZE];
            strcpy(tmp, buffer+1); // skip '.'
            strcpy(buffer, getcwd(NULL, 0));
            
            strcat(buffer, tmp);
            printf("COMPLETED COMMAND: %s\n", buffer);
        }

        // break into pipes
        {
            char* tok = __strtok_r(buffer, "|><", &saveptr_pipes);
            
            while(tok != NULL){
                pipes[pipec] = (char*) calloc(strlen(tok)+1, sizeof(char));
                strcpy(pipes[pipec], tok);
                
                tok = __strtok_r(NULL, "|><", &saveptr_pipes);
                
                pipec++;
            }
            pipes[pipec] = NULL;
            argv_debug(pipec, pipes);
        }
        

        // break pipes into arguments
        {   
            for(int i = 0; i < pipec; i++){
                { // count number of tokens before tokenization and allocate
                    int count = 0;
                    char *ptr = pipes[i];
                    while((ptr = strchr(ptr, ' ')) != NULL) {
                        count++;
                        ptr++;
                    }
                    argvs[i] = (char**) malloc(sizeof(char*) * count+1);
                }

                regex_t compiled;
                
                // Matches all characters part of a quote block OR all non-whitespace nor quote characters in a sequence
                int comp = regcomp(&compiled, "\"[^\r\n\t\f\v]+\"|[^\"\r\n\t\f\v ]+", REG_EXTENDED);
                int offset = 0;
                int argc = 0;

                char tok[BUF_SIZE];

                while(strtok_regex(&compiled, pipes[i], &offset, tok) != -1){
                    argvs[i][argc] = (char*) malloc(sizeof(char) * (strlen(tok)+1));
                    strcpy(argvs[i][argc], tok);
                    argc++;
                }
                
                argvs[i][argc] = NULL; // Ending argv with a NULL pointer
                argv_debug(argc, argvs[i]);
            }
        }


        // I guess this is where built-ins will go
        {
            if(strcmp(argvs[0][0], "history") == 0)
            {
                int index = 0;
                struct item* walk = history.HEAD;

                // captures index argument
                if(argvs[0][1]!=NULL){
                    index = atoi(argvs[0][1]);
                }
                if(index > 0){
                    struct item* select = History_back_walk(&history, index);
                    if(select != NULL){
                        printf("%s", select->data);
                    }
                // default, no / invalid argument
                }else{
                    while(walk->next != NULL){
                        printf("%s", walk->data);
                        walk = walk->next;
                    }
                }
                continue;
            }
            else if(strcmp(argvs[0][0], "!") == 0)
            {
                int index;
                if(argvs[0][1]!=NULL){
                    index = atoi(argvs[0][1]);
                }
                if(index > 0){
                    struct item* select = History_back_walk(&history, index);
                    memset(buffer, 0, BUF_SIZE);
                    strcpy(buffer, select->data);
                    goto begin_parse; // cardinal sin. This skips the initial checks and cleaning
                                      // going straight into parsing the command in the buffer.
                                      // This is a prime example of you *shouldn't* use goto
                }
            }
            else if (strcmp(argvs[0][0], "exit") == 0)
            {
                exit(0);
            }
            else if (strcmp(argvs[0][0], "cd") == 0)
            {
                char* dir = argvs[0][1];
                if(chdir(dir) == -1){
                    perror(dir);
                }
                continue;
            }
        }
        
        {
            int old_fds[2] = {STDIN_FILENO, STDOUT_FILENO};
            for(int i = 0; i < pipec; i++){
                int new_fds[2];
                if(i != pipec-1){ // if there is a next cmd
                    pipe(new_fds);
                }
                int f = fork();
                switch(f){
                    case -1:
                        perror("fork error...");
                    case 0: // child
                        if(i != 0){ // if there is a previous cmd
                            dup2(old_fds[0], 0);
                            close(old_fds[0]);
                            close(old_fds[1]);
                        } 
                        if(i != pipec-1){ // if there is a next cmd
                            close(new_fds[0]);
                            dup2(new_fds[1], 1);
                            close(new_fds[1]);
                        }
                        execution(argvs[i]);
                        break;
                    default: // parent
                        if(i != 0){ // if there is a previous command
                            close(old_fds[0]);
                            close(old_fds[1]);
                        }
                        if(i != pipec-1){ // if there is a next cmd
                            old_fds[0] = new_fds[0];
                            old_fds[1] = new_fds[1];
                        }
                }
            }
            if(pipec > 1){ // If there are multiple cmds
                close(old_fds[0]);
                close(old_fds[1]);
            }
        }
        

    }
}


void execution(char** _argv){
    // find in path, unless path is absolute
    int found = false;
    char __path[BUF_SIZE];
    strcpy(__path, path);
    char* tok = strtok(__path, ":");

    while(!found){
        bool is_absolute = false;
        char* program_path;

        if(_argv[0][0] == '/'){ // check for absolute path
            program_path = _argv[0];
            is_absolute = true;
        }else{
            program_path = (char*) malloc(sizeof(char) * (strlen(tok)+1) );
            strcpy(program_path, tok);
            program_path = strcat(program_path, "/");
            program_path = strcat(program_path, _argv[0]);
        }

        if(access(program_path, F_OK) == 0){
            printf("%s exists.\n", program_path);

execute:
            
            //argv_debug(_argc, _argv);
            //printf("execve(%s)\n", program_path);
            execv(program_path, _argv);
            exit(0);
            
            found = true;
        }else{
            tok = strtok(NULL, ":");
            if(tok == NULL || is_absolute == true){
                printf("%s: command not found...\n", _argv[0]);
                break;
            }
        }
    }
}