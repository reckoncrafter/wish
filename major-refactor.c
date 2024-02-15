#include "main.h"

#define BUF_SIZE 1024

static char _PROMPT[] = "wish $ ";
char buffer[BUF_SIZE];
char* path;
FILE* SCRIPT;

struct History history;

struct PARSE_STATE{
    int pipec;
    char*** argvs;
};

int capture_input();
struct PARSE_STATE parse();
void execution(char** _argv);

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

int main(int argc, char* argv[]){
    SCRIPT = get_script(argv);
    history = History_init();

    while(true){
        capture_input();
        printf("%s", buffer);

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

begin_parse:
        struct PARSE_STATE ps = parse();

        {
            if(strcmp(ps.argvs[0][0], "history") == 0)
            {
                int index = 0;
                struct item* walk = history.HEAD;

                // captures index argument
                if(ps.argvs[0][1]!=NULL){
                    index = atoi(ps.argvs[0][1]);
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
            else if(strcmp(ps.argvs[0][0], "!") == 0)
            {
                int index;
                if(ps.argvs[0][1]!=NULL){
                    index = atoi(ps.argvs[0][1]);
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
            else if (strcmp(ps.argvs[0][0], "exit") == 0)
            {
                exit(0);
            }
            else if (strcmp(ps.argvs[0][0], "cd") == 0)
            {
                char* dir = ps.argvs[0][1];
                if(chdir(dir) == -1){
                    perror(dir);
                }
                continue;
            }
        }

        {
            int old_fds[2] = {STDIN_FILENO, STDOUT_FILENO};
            for(int i = 0; i < ps.pipec; i++){
                int new_fds[2];
                if(i != ps.pipec-1){ // if there is a next cmd
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
                        if(i != ps.pipec-1){ // if there is a next cmd
                            close(new_fds[0]);
                            dup2(new_fds[1], 1);
                            close(new_fds[1]);
                        }
                        execution(ps.argvs[i]);
                        break;
                    default: // parent
                        if(i != 0){ // if there is a previous command
                            close(old_fds[0]);
                            close(old_fds[1]);
                        }
                        if(i != ps.pipec-1){ // if there is a next cmd
                            old_fds[0] = new_fds[0];
                            old_fds[1] = new_fds[1];
                        }
                }
            }
            if(ps.pipec > 1){ // If there are multiple cmds
                close(old_fds[0]);
                close(old_fds[1]);
            }
        }
    }

}

int capture_input(){
    if(SCRIPT != NULL){ // are we running a script?
        char* line = NULL;
        size_t len = 0;
        if(getline(&line, &len, SCRIPT) != -1){
            strcpy(buffer, line);
        }else{
            exit(0);
        }
    }else{ //  normal input
        printf("%s", _PROMPT);
        fflush(stdout);
        read(STDIN_FILENO, buffer, BUF_SIZE);
    }
    return 0;
}

struct PARSE_STATE parse(){
    struct PARSE_STATE ps;
    char* pipes[BUF_SIZE];
    char* saveptr_args;
    char* saveptr_pipes;

    {// break into pipes
        char* tok = __strtok_r(buffer, "|><", &saveptr_pipes);
        while(tok != NULL){
            pipes[ps.pipec] = (char*) malloc(sizeof(char) * (strlen(tok)+1));
            strcpy(pipes[ps.pipec], tok);
            
            tok = __strtok_r(NULL, "|><", &saveptr_pipes);
            
            ps.pipec++;
        }
        pipes[ps.pipec] = NULL;
        ps.argvs = (char***) malloc(sizeof(char**) * ps.pipec+1);
        //argv_debug(pipec, pipes);
    }

    
    {// break pipes into arguments
        for(int i = 0; i < ps.pipec; i++){
            { // count number of tokens before tokenization and allocate
                int count = 0;
                char *ptr = pipes[i];
                while((ptr = strchr(ptr, ' ')) != NULL) {
                    count++;
                    ptr++;
                }
                ps.argvs[i] = (char**) malloc(sizeof(char*) * count+1);
            }

            regex_t compiled;
            
            // Matches all characters part of a quote block OR all non-whitespace nor quote characters in a sequence
            int comp = regcomp(&compiled, "\"[^\r\n\t\f\v]+\"|[^\"\r\n\t\f\v ]+", REG_EXTENDED);
            int offset = 0;
            int argc = 0;

            char tok[BUF_SIZE];

            while(strtok_regex(&compiled, pipes[i], &offset, tok) != -1){
                ps.argvs[i][argc] = (char*) malloc(sizeof(char) * (strlen(tok)+1));
                strcpy(ps.argvs[i][argc], tok);
                argc++;
            }
            
            ps.argvs[i][argc] = NULL; // Ending argv with a NULL pointer
            //argv_debug(argc, argvs[i]);
        }
    }
    return ps;
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
            execve(program_path, _argv, NULL);
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