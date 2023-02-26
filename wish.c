#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#define DELIM " "

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...)                \
    do                                       \
    {                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

static char *PATH = NULL;

static FILE *input_stream = NULL;
static FILE *output_stream = NULL;

#ifdef DEBUG
static void printTokens(char **tokens, size_t token_count)
{
    DEBUG_PRINT("token count: %ld \n", token_count);
    for (size_t i = 0; i < token_count; i++)
    {
        DEBUG_PRINT("token: %s, ", tokens[i]);
    }
    DEBUG_PRINT("\n");
}
#endif

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
static FILE *_openFile(char *file_name, char *opt)
{
    FILE *input_file = fopen(file_name, opt);
    if (input_file == NULL)
    {
        error();
    }
    return input_file;
}

static size_t extractTokens(char *line, char ***tokens_ptr)
{
    char **tokens = *tokens_ptr;
    char *token_line = line;
    char *token = NULL;
    size_t token_count = 0;
    while ((token = strsep(&token_line, DELIM)) != NULL)
    {
        token_count += 1;
        tokens = (char **)realloc(tokens, sizeof(char *) * (token_count));
        tokens[token_count - 1] = (char *)malloc(sizeof(char) * strlen(token) + 1);
        strcpy(tokens[token_count - 1], token);
        tokens[token_count - 1] = strsep(&tokens[token_count - 1], "\n");
        tokens[token_count - 1] = strsep(&tokens[token_count - 1], "\t");
        DEBUG_PRINT("READ TOKEN: %s\n", tokens[token_count - 1]);
    }
    free(token_line);
    free(token);
    *tokens_ptr = tokens;
    return token_count;
}

static bool launchApplication(char *app_path, char **tokens, size_t token_count)
{
    // Create args
    bool should_error = false;
    char **args = malloc(sizeof(char *) * (token_count+1));
    for (int i = 0; i < token_count; i++)
    {
        args[i] = (char *)malloc(sizeof(char) * (strlen(tokens[i]) + 1));
        strcpy(args[i], tokens[i]);
    }
    args[token_count] = NULL;
    DEBUG_PRINT("Launched application\n");
    pid_t id = fork();
    if (id == 0){
        execv(app_path, args);
    } else if (id > 0) {
        waitpid(id, NULL, 0);
    } else {
        should_error = true;
    }
    //Memory leak if not addressed...
    for (int i = 0; i < token_count; i++)
    {
        free(args[i]);
    }
    free(args);
    return should_error;
}

static bool executeCommands(char **tokens, size_t token_count)
{
    if (token_count == 1 && !strcmp(tokens[0], "exit"))
    {
        return true;
    } else if (!strcmp(tokens[0], "cd")){
        if (token_count == 1 || token_count > 2) {
            error();
        }
        int rc = chdir(tokens[1]);
        if (rc != 0){
            error();
        }
    } else if (!strcmp(tokens[0], "path")){
        if (token_count == 1) {
            //There is are no arguments for pathm clear the path
            free(PATH);
            PATH = NULL;
        } else {
            //Append onto the existing path
            //Tokens should already be parsed out
            for (size_t i = 1; i < token_count; i++){
                if (PATH != NULL) {
                    PATH = (char *) realloc(PATH, (strlen(PATH)+strlen(tokens[i]) + 2)*sizeof(char));
                    PATH = strcat(PATH, ":");
                    PATH = strcat(PATH, tokens[i]);
                } else {
                    PATH = (char *) malloc((strlen(tokens[i])+1)*sizeof(char));
                    PATH = strcpy(PATH, tokens[i]);
                }
            }
        }

        DEBUG_PRINT("Path is now set to: %s\n", PATH);
    }
    else
    {
        char * temp_path = PATH;
        char * single_path = NULL;
        bool should_error = true;
        size_t len_of_token = strlen(tokens[0]);
        DEBUG_PRINT("len of app token %ld\n", len_of_token);
        while((single_path = strsep(&temp_path, ":")) != NULL) {
            // Ignoring if path has been updated yet going to strictly check for bin
            DEBUG_PRINT("Creating app_path variable\n");
            size_t len_of_path = strlen(single_path);
            DEBUG_PRINT("len of PATH %ld\n", len_of_path);
            // Want to account for / and \0 at end of string so +2
            char *app_path = (char *)malloc(sizeof(char) * (len_of_path + len_of_token + 2));
            DEBUG_PRINT("Allocated Memory for App Path \n");
            strcpy(app_path, single_path);
            strcat(app_path, "/");
            strcat(app_path, tokens[0]);
            DEBUG_PRINT("Created App Path: %s\n", app_path);
            if (access(app_path, F_OK) == 0)
            {
                DEBUG_PRINT("App path exists \n");
                should_error = launchApplication(app_path,tokens, token_count);
            }
            free(app_path);
        }
        free(temp_path);
        if (should_error) error();
    }
    return false;
}

static void shellLoop()
{
    PATH = (char *) malloc(sizeof(char)*5);
    PATH = strcpy(PATH, "/bin");
    printf("wish> ");
    char *line = NULL;
    bool exit_program = false;
    size_t len = 0;
    size_t nread = 0;

    while ((nread = getline(&line, &len, input_stream)) != -1)
    {
        char **tokens = NULL;
        size_t token_count = 0;
        token_count = extractTokens(line, &tokens);
        #if DEBUG
        printTokens(tokens, token_count);
        #endif
        DEBUG_PRINT("Tokens Generated\n");
        exit_program = executeCommands(tokens, token_count);
        for (int i = 0; i < token_count; i++)
        {
            free(tokens[i]);
        }
        free(tokens);
        if (exit_program)
            break;
    }

    free(line);
    free(PATH);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    input_stream = stdin;
    output_stream = stdout;

    switch (argc)
    {
    case 1:
        break;
    case 2:
        input_stream = _openFile(argv[1], "r");
    default:
        error();
    }

    shellLoop();
    exit(EXIT_SUCCESS);
}