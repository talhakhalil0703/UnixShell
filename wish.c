#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define DELIM " "

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) do {fprintf(stderr, fmt, ##__VA_ARGS__);} while (0)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

static char *PATH = "/bin";

static FILE * input_stream = NULL;
static FILE * output_stream = NULL;

static void print_tokens(char** tokens, size_t token_count) {
    for(size_t i = 0; i < token_count; i++){
        fprintf(output_stream, "token: %s, ", tokens[i]);
    }
    fprintf(output_stream, "\n");
}

static void error()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
}

/**
 * _openFile - opens the file with the given filename and mode
 * @param file_name: the path of the file to be opened
 * @param opt: the mode in which to open the file (e.g. "r" for read)
 * @return filestream
*/
static FILE * _openFile(char * file_name, char * opt){
    FILE * input_file = fopen(file_name, opt);
    if (input_file == NULL){
        error();
    }
    return input_file;
}

static void shell_loop()
{
    printf("wish> ");
    char *line = NULL;
    char *token = NULL;
    bool exit_program = false;
    size_t len = 0;
    size_t nread;

    while ((nread = getline(&line, &len, input_stream)) != -1)
    {
        // fwrite(line, nread, 1, stdout);
        char *token_line = line;
        char **tokens = malloc(sizeof(char*));
        size_t token_count = 0;
        while ((token = strsep(&token_line, DELIM)) != NULL)
        {
            token_count += 1;
            tokens = realloc(tokens, sizeof(char*)*token_count);
            tokens[token_count-1] = (char*) malloc(sizeof(char)*strlen(token)+1);
            strcpy(tokens[token_count-1], token);
            tokens[token_count-1] = strsep(&tokens[token_count-1], "\n");
            tokens[token_count-1] = strsep(&tokens[token_count-1], "\t");
        }

        print_tokens(tokens, token_count);
        DEBUG_PRINT("Tokens Generated\n");

        if (token_count == 1 && !strcmp(tokens[0], "exit")) {
            exit_program = true;
        } else {
            //Ignoring if path has been updated yet going to strictly check for bin
            DEBUG_PRINT("Creating app_path variable\n");
            // Note that strlen excludes the null byte so we have to account for it
            size_t len_of_token = strlen(tokens[0]);
            DEBUG_PRINT("len of app token %ld\n", len_of_token);
            size_t len_of_path = strlen(PATH);
            DEBUG_PRINT("len of PATH %ld\n", len_of_path);
            char *app_path = (char *) malloc(sizeof(char)*(len_of_path+len_of_token+2));
            DEBUG_PRINT("Allocated Memory for App Path \n");
            strcpy(app_path, PATH);
            DEBUG_PRINT("App Path %s\n", app_path);
            strcat(app_path, "/");
            strcat(app_path, tokens[0]);
            DEBUG_PRINT("Created App Path: %s\n", app_path);
            if (access(app_path, F_OK) == 0) {
                DEBUG_PRINT("App path exists \n");
                // App path does indeed exist
                // Create args
                char **args = malloc(sizeof(char*)*token_count-1);
                for(int i = 0; i < token_count-1; i++){
                    args[i] = (char *) malloc(sizeof(char)*(strlen(tokens[i+1])+1));
                    strcpy(args[i], tokens[i+1]);
                }
                execv(app_path, args);
                for(int i = 0; i < token_count-1; i++){
                    free(args[i]);
                }
                free(args);
            } else {
                DEBUG_PRINT("App path does not exist\n");
            }
            free(app_path);
        }


        for (int i =0; i < token_count; i++){
            free(tokens[i]);
        }
        free(tokens);
        free(token_line);
        if (exit_program) break;
    }

    free(line);
    free(token);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    input_stream = stdin;
    output_stream = stdout;

    switch(argc) {
        case 1:
            break;
        case 2:
            input_stream = _openFile(argv[1], "r");
        default:
        error();
    }

    shell_loop();

    exit(EXIT_SUCCESS);
}