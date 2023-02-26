#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...)                \
    do                                       \
    {                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#define DELIM " "
#define REDIRECTION_OPERATOR ">"
#define PARALLEL_OPERATOR "&"

static char *PATH = NULL;
// static bool ERORR_OCCURED = false;

static FILE *INPUT_STREAM = NULL;
static pid_t * PIDS_TO_WAIT_FOR = NULL;
static size_t PIDS_TO_WAIT_FOR_COUNT = 0;

#ifdef DEBUG
static void printTokens(char **tokens, size_t token_count)
{
    DEBUG_PRINT("token count: %ld \n", token_count);
    for (size_t i = 0; i < token_count; i++)
    {
        DEBUG_PRINT("token: '%s', ", tokens[i]);
    }
    DEBUG_PRINT("\n");
}
#endif

static void error()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
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

static size_t splitCommandsForOperator(char *line, char ***command_lines_ptr, char* op)
{
    char **command_lines = *command_lines_ptr;
    char *single_command = NULL;
    char *line_ptr = line;
    size_t commands = 0;
    while ((single_command = strsep(&line_ptr, op)) != NULL)
    {
        commands++;
        command_lines = realloc(command_lines, sizeof(char *) * commands);
        command_lines[commands - 1] = malloc(sizeof(char) * (strlen(single_command) + 1));
        command_lines[commands - 1] = strcpy(command_lines[commands - 1], single_command);
    }

    free(single_command);
    *command_lines_ptr = command_lines;
    return commands;
}


static size_t extractTokens(char *line, char ***tokens_ptr)
{
    char **tokens = *tokens_ptr;
    char ** redirection_lines = NULL;
    size_t token_count = 0;
    size_t lines_to_process = splitCommandsForOperator(line, &redirection_lines, REDIRECTION_OPERATOR);
    for (size_t i = 0; i < lines_to_process; i++){
        if (!strcmp(redirection_lines[i], "")) continue;
        char *line_ptr = redirection_lines[i];
        char *token = NULL;
        while ((token = strsep(&line_ptr, DELIM)) != NULL)
        {
            if (!strcmp(token, "") || !strcmp(token, "\n")) continue;
            token_count += 1;
            tokens = (char **)realloc(tokens, sizeof(char *) * (token_count));
            tokens[token_count - 1] = (char *)malloc(sizeof(char) * strlen(token) + 1);
            strcpy(tokens[token_count - 1], token);
            tokens[token_count - 1] = strsep(&tokens[token_count - 1], "\n");
            tokens[token_count - 1] = strsep(&tokens[token_count - 1], "\t");
            DEBUG_PRINT("READ TOKEN: %s\n", tokens[token_count - 1]);
        }

        if (i != lines_to_process-1){
            token_count += 1;
            tokens = (char **)realloc(tokens, sizeof(char *) * (token_count));
            tokens[token_count - 1] = (char *)malloc(sizeof(char) * strlen(REDIRECTION_OPERATOR) + 1);
            strcpy(tokens[token_count - 1], REDIRECTION_OPERATOR);
        }
        free(token);
    }
    *tokens_ptr = tokens;
    for (size_t i =0; i < lines_to_process; i++){
        free(redirection_lines[i]);
    }
    free(redirection_lines);
    return token_count;
}

static void launchApplication(char *app_path, char **tokens, size_t token_count, char *output_stream_path)
{
    // Create args
    char **args = malloc(sizeof(char *) * (token_count + 1));
    for (int i = 0; i < token_count; i++)
    {
        args[i] = (char *)malloc(sizeof(char) * (strlen(tokens[i]) + 1));
        strcpy(args[i], tokens[i]);
    }
    args[token_count] = NULL;
    DEBUG_PRINT("Launched application\n");
    pid_t id = fork();
    if (id == 0)
    {
        if (output_stream_path != NULL)
        {
            freopen(output_stream_path, "w", stdout);
            freopen(output_stream_path, "w", stderr);
        }
        execv(app_path, args);
    }
    else if (id > 0)
    {
        PIDS_TO_WAIT_FOR_COUNT++;
        PIDS_TO_WAIT_FOR = realloc(PIDS_TO_WAIT_FOR,sizeof(pid_t)*PIDS_TO_WAIT_FOR_COUNT);
        PIDS_TO_WAIT_FOR[PIDS_TO_WAIT_FOR_COUNT-1] = id;
    }
    else
    {
        error();
    }
    // Memory leak if not addressed...
    for (int i = 0; i < token_count; i++)
    {
        free(args[i]);
    }
    free(args);
}

static void builtInPath(char **tokens, size_t token_count)
{
    if (token_count == 1)
    {
        // There is are no arguments for pathm clear the path
        free(PATH);
        PATH = NULL;
    }
    else
    {
        // Append onto the existing path
        // Tokens should already be parsed out
        for (size_t i = 1; i < token_count; i++)
        {
            if (PATH != NULL)
            {
                PATH = (char *)realloc(PATH, (strlen(PATH) + strlen(tokens[i]) + 2) * sizeof(char));
                PATH = strcat(PATH, ":");
                PATH = strcat(PATH, tokens[i]);
            }
            else
            {
                PATH = (char *)malloc((strlen(tokens[i]) + 1) * sizeof(char));
                PATH = strcpy(PATH, tokens[i]);
            }
        }
    }

    DEBUG_PRINT("Path is now set to: %s\n", PATH);
}

static void builtInCd(char **tokens, size_t token_count)
{
    if (token_count == 1 || token_count > 2)
    {
        error();
        return;
    }
    int rc = chdir(tokens[1]);
    if (rc != 0)
    {
        error();
        return;
    }
}

static void customCommand(char **tokens, size_t token_count)
{
    if (PATH == NULL) {
        error();
        return;
    }
    // Count the number od redirect operators, if greater than 1 error
    size_t redirect_count = 0;
    ssize_t redirect_token_pos = token_count;
    char *redirect_stream_path = NULL;
    for (size_t i = 0; i < token_count; i++)
    {
        if (!strcmp(tokens[i], REDIRECTION_OPERATOR))
        {
            redirect_count += 1;
            redirect_token_pos = i;
        }
    }

    if (redirect_count > 1)
    {
        error();
        return;
    }

    if (redirect_token_pos != token_count)
    {
        if (redirect_token_pos != token_count - 2)
        {
            // More than one file on the right side of the redirection operator
            error();
            return;
        }
        redirect_stream_path = tokens[token_count - 1];
    }

    // Using the count limit the command amount run
    // Redirect can be done in launcApplication
    char *temp_path = malloc((strlen(PATH) + 1) * sizeof(char));
    temp_path = strcpy(temp_path, PATH);
    char *temp_ptr = temp_path;
    char *single_path = NULL;
    bool command_ran = false;
    size_t len_of_token = strlen(tokens[0]);
    DEBUG_PRINT("len of app token %ld\n", len_of_token);
    while ((single_path = strsep(&temp_ptr, ":")) != NULL && !command_ran)
    {
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
        if (access(app_path, X_OK) == 0)
        {
            DEBUG_PRINT("App path exists \n");
            launchApplication(app_path, tokens, redirect_token_pos, redirect_stream_path);
            command_ran = true;
        }
        free(app_path);
    }
    if (!command_ran)
    {
        error();
    }
    free(temp_path);
}

static bool executeCommands(char **tokens, size_t token_count)
{
    if (token_count == 0) return false;
    if (!strcmp(tokens[0], "exit"))
    {
        if (token_count > 1)
        {
            error();
        }
        return true;
    }
    else if (!strcmp(tokens[0], "cd"))
    {
        builtInCd(tokens, token_count);
    }
    else if (!strcmp(tokens[0], "path"))
    {
        builtInPath(tokens, token_count);
    }
    else
    {
        customCommand(tokens, token_count);
    }
    return false;
}

static void shellLoop()
{
    PATH = (char *)malloc(sizeof(char) * 5);
    PATH = strcpy(PATH, "/bin");
    char *line = NULL;
    bool exit_program = false;
    size_t len = 0;
    size_t nread = 0;

    while (!exit_program && (nread = getline(&line, &len, INPUT_STREAM)) != -1)
    {
        if (INPUT_STREAM == stdin)
            printf("wish> ");
        char **command_lines = NULL;
        size_t commands = splitCommandsForOperator(line, &command_lines, PARALLEL_OPERATOR);
        for (size_t i = 0; i < commands; i++)
        {
            char **tokens = NULL;
            size_t token_count = 0;
            token_count = extractTokens(command_lines[i], &tokens);
#if DEBUG
            printTokens(tokens, token_count);
#endif
            DEBUG_PRINT("Tokens Generated\n");
            exit_program += executeCommands(tokens, token_count);

            for (int i = 0; i < token_count; i++)
            {
                free(tokens[i]);
            }
            free(tokens);
        }

        for (size_t i=0; i < PIDS_TO_WAIT_FOR_COUNT; i++){
            waitpid(PIDS_TO_WAIT_FOR[i], NULL, 0);
        }
        PIDS_TO_WAIT_FOR_COUNT = 0;

        for (size_t i = 0; i < commands; i++)
        {
            free(command_lines[i]);
        }
        free(command_lines);
    }
    free(line);
    free(PATH);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    INPUT_STREAM = stdin;

    switch (argc)
    {
    case 1:
        break;
    case 2:
        INPUT_STREAM = _openFile(argv[1], "r");
        if (INPUT_STREAM == NULL)
        {
            exit(1);
        }
        break;
    default:
        error();
        exit(1);
    }

    shellLoop();
    exit(EXIT_SUCCESS);
}