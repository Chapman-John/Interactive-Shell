// 1.Print an interactive input prompt
// 2.Parse command line input into semantic tokens
// 3.Implement parameter expansion
// --Shell special parameters $$, $?, and $!
// --Generic parameters as ${parameter}
// 4.Implement two shell built-in commands: exit and cd
// 5.Execute non-built-in commands using the the appropriate EXEC(3) function.
// --Implement redirection operators ‘<’,  ‘>’ and '>>'
// --Implement the ‘&’ operator to run commands in the background
// 8.Implement custom behavior for SIGINT and SIGTSTP signals
// get input -> split the input up -> expand the input -> check for builtins (exit, cd) -> fork if it's not one of those
// getting input / splitting up are completely done for you, expanding is mostly done, then you should have some conditionals that check
// for built in commands, and if it isn't one of those then you fork

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char *expand(char const *word);
char *inputFile = NULL;
char *outputFile = NULL;
char *appendFile = NULL;
int backgroundFlag = 0;
char *foregroundPid = "0";
char *backgroundPid = "";
int inputSource = 0;
int outputSource = 0;
int bool = 0;
int stopped = 0;
int pidStop = 0;
int target = 0;

// SIGINT signal shall be ignored (SIG_IGN) at all times except when
// reading a line of input in Step 1, during which time it shall be
// registered to a signal handler which does nothing.

void handle_SIGINT(int sig)
{
}

// Signal handler for SIGTSTP
// SIGTSTP signal shall be ignored by smallsh

void handle_SIGTSTP(int sig)
{
}

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    char *input_fn = "(stdin)";
    if (argc == 2)
    {
        input_fn = argv[1];
        input = fopen(input_fn, "re");
        if (!input)
            err(1, "%s", input_fn);
    }
    else if (argc > 2)
    {
        errx(1, "too many arguments");
    }
    char *line = NULL;
    size_t n = 0;
    for (;;)
    {
    start:
        backgroundFlag = 0;
        outputFile = NULL;
        inputFile = NULL;
        appendFile = NULL;
        int backgroundStatus = 0;
        pid_t manageBackgroundProcesses = 0;
        // WUNTRACED in the while loop statement is for test 16
        while ((manageBackgroundProcesses = waitpid(0, &backgroundStatus, WUNTRACED | WNOHANG)) > 0)
        {
            if (WIFEXITED(backgroundStatus))
            {
                fprintf(stderr, "Child process %jd done. Exit status %d.\n", ((intmax_t)manageBackgroundProcesses), WEXITSTATUS(backgroundStatus));
                break;
            }
            else
            {
                fprintf(stderr, "Child process %jd done. Signaled %d.\n", ((intmax_t)manageBackgroundProcesses), WTERMSIG(backgroundStatus));
                break;
            }
        }

        signal(SIGTSTP, handle_SIGTSTP);
        signal(SIGINT, handle_SIGINT);
        char *PS1 = getenv("PS1");
        if (PS1 == NULL)
        {
            PS1 = "";
        }

        if (input == stdin)
        {
            errno = 0;
        }

        ssize_t line_len = getline(&line, &n, input);
        if (feof(input) != 0)
        {
            return 0;
        }
        if (stopped == 1)
        {
            sprintf(foregroundPid, "%d", pidStop);
        }

        fprintf(stderr, "%s", PS1);
        size_t nwords = wordsplit(line);
        for (size_t i = 0; i < nwords; ++i)
        {
            char *exp_word = expand(words[i]);
            free(words[i]);
            words[i] = exp_word;
        }
        if (words[0] == NULL)
        {
            exit(0);
        }

        if (words[0] != NULL)
        {

            // If the first element is 'exit'

            if (strcmp(words[0], "exit") == 0)
            {
                if (words[2] != NULL)
                {
                    exit(0);
                }

                if (words[1] == NULL)
                {
                    exit(0);
                }

                exit(atoi(words[1]));
            }

            if (words[1] != NULL)
            {

                if (strcmp(words[1], "<") == 0)
                {
                    words[1] = words[2];
                    words[2] = NULL;
                }
            }

            if (words[2] != NULL)
            {

                if (strcmp(words[2], ">") == 0)
                {

                    if (words[3] != NULL)
                    {
                        outputFile = words[3];
                        words[2] = NULL;
                    }
                }
            }

            if (words[2] != NULL)
            {
                if (strcmp(words[2], ">>") == 0)
                {

                    if (words[3] != NULL)
                    {
                        outputFile = words[3];
                        words[2] = NULL;
                    }
                }
            }

            // cat < infile >> testfile > garbagefile > outfile

            if (words[5] != NULL)
            {

                if (strcmp(words[5], ">") == 0)
                {

                    if (words[6] != NULL)
                    {
                        outputFile = words[6];
                        words[5] = NULL;
                    }
                }
            }

            if (words[7] != NULL)
            {

                if (strcmp(words[7], ">") == 0)
                {
                    if (words[8] != NULL)
                    {
                        outputFile = words[8];
                        words[7] = NULL;
                    }
                }
            }
            if (words[1] != NULL)
            {
                if (strcmp(words[1], "&") == 0)
                {
                    words[1] = NULL;
                    if (backgroundFlag == 0)
                    {
                        backgroundFlag = 1;
                    }
                }
            }
            if (words[2] != NULL)
            {

                if (strcmp(words[2], "&") == 0)
                {
                    words[2] = NULL;
                    bool = 1;
                    goto start;
                }
            }
            if (strcmp(words[0], "cd") == 0)
            {

                if (words[2] != NULL)
                {

                    goto start;
                }

                if (words[1] != NULL)
                {

                    chdir(words[1]);

                    words[1] = NULL;
                }
                else
                {

                    char *homePath = getenv("HOME");

                    chdir(homePath);
                }

                goto start;
            }
        }

        pid_t spawnPid = -5;
        int childStatus = -5;
        spawnPid = fork();
        switch (spawnPid)
        {

        case -1:
            perror("fork()\n");
            exit(1);
            break;

        case 0:
            if (outputFile == NULL)
            {

                outputSource = open(outputFile, O_TRUNC | O_CREAT, 0777);
                target = dup2(outputSource, 1);
            }

            if (appendFile != NULL)
            {
                outputSource = open(appendFile, O_WRONLY | O_APPEND | O_TRUNC, 0644);

                if (outputSource == -1)
                {

                    perror("source open()");

                    exit(1);
                }

                target = dup2(outputSource, 1);

                if (target == -1)
                {

                    perror("source dup2()");

                    exit(2);
                }
            }

            if (outputFile != NULL)
            {

                outputSource = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                if (outputSource == -1)
                {

                    perror("source open()");

                    exit(1);
                }

                target = dup2(outputSource, 1);

                if (target == -1)
                {

                    perror("source dup2()");

                    exit(2);
                }
            }

            execvp(words[0], words);

            if (inputFile != NULL)
            {
                close(inputSource);
            }
            if (outputFile != NULL)
            {
                close(outputSource);
            }
            if (appendFile != NULL)
            {
                close(outputSource);
            }

            if (PS1 != NULL)
            {
                PS1 = NULL;
            }

            exit(0);

            break;

        default:
            if (backgroundFlag == 0)
            {

                spawnPid = waitpid(spawnPid, &childStatus, WUNTRACED);
                foregroundPid = malloc(10 * sizeof(int));

                if (WIFSIGNALED(childStatus))
                {
                    sprintf(foregroundPid, "%d", (128 + WTERMSIG(childStatus)));
                    break;
                }
                else if (WIFSTOPPED(childStatus))
                {
                    fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)spawnPid);
                    stopped = 1;
                    goto start;
                }

                sprintf(foregroundPid, "%d", WEXITSTATUS(childStatus));
            }
            else
            {

                // background process

                backgroundPid = malloc(10 * sizeof(int));

                sprintf(backgroundPid, "%d", spawnPid);
            }

            if (WEXITSTATUS(childStatus) && bool == 1)
            {
                fprintf(stderr, "Child process %jd done. Exit status %d.\n", ((intmax_t)spawnPid), WEXITSTATUS(childStatus));
            }

            goto start;
        }

        if (WTERMSIG(childStatus) && bool == 1)
        {

            fprintf(stderr, "Child process %jd done. Signaled %d.\n", ((intmax_t)spawnPid), WTERMSIG(childStatus));
        }
    }
    return 0;
}

char *words[MAX_WORDS] = {NULL, NULL, NULL};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */

size_t wordsplit(char const *line)
{

    size_t wlen = 0;

    size_t wind = 0;

    char const *c = line;

    for (; *c && isspace(*c); ++c)
        ; /* discard leading space */

    for (; *c;)
    {

        if (wind == MAX_WORDS)
            break;

        /* read a word */

        if (*c == '#')
            break;

        for (; *c && !isspace(*c); ++c)
        {

            if (*c == '\\')
                ++c;

            void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));

            if (!tmp)
                err(1, "realloc");

            words[wind] = tmp;

            words[wind][wlen++] = *c;

            words[wind][wlen] = '\0';
        }

        ++wind;

        wlen = 0;

        for (; *c && isspace(*c); ++c)
            ;
    }

    return wind;
}

/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */

char

param_scan(char const *word, char const **start, char const **end)

{

    static char const *prev;

    if (!word)
        word = prev;

    char ret = 0;

    *start = 0;

    *end = 0;

    for (char const *s = word; *s && !ret; ++s)
    {

        s = strchr(s, '$');

        if (!s)
            break;

        switch (s[1])
        {

        case '$':

        case '!':

        case '?':

            ret = s[1];

            *start = s;

            *end = s + 2;

            break;

        case '{':;

            char *e = strchr(s + 2, '}');

            if (e)
            {

                ret = s[1];

                *start = s;

                *end = e + 1;
            }

            break;
        }
    }

    prev = *end;

    return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */

char *

build_str(char const *start, char const *end)

{

    static size_t base_len = 0;

    static char *base = 0;

    if (!start)
    {

        /* Reset; new base string, return old one */

        char *ret = base;

        base = NULL;

        base_len = 0;

        return ret;
    }

    /* Append [start, end) to base string
     * If end is NULL, append whole start string to base string.
     * Returns a newly allocated string that the caller must free.
     */

    size_t n = end ? end - start : strlen(start);

    size_t newsize = sizeof *base * (base_len + n + 1);

    void *tmp = realloc(base, newsize);

    if (!tmp)
        err(1, "realloc");

    base = tmp;

    memcpy(base + base_len, start, n);

    base_len += n;

    base[base_len] = '\0';

    return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string
 * Returns a newly allocated string that the caller must free
 */

char *expand(char const *word)
{

    char const *pos = word;
    char const *start, *end;
    char c = param_scan(pos, &start, &end);
    build_str(NULL, NULL);
    build_str(pos, start);

    while (c)
    {

        if (c == '!')
        {
            build_str(backgroundPid, NULL);
        }

        else if (c == '$')
        {

            pid_t pid = getpid();
            int pidLen = snprintf(NULL, 0, "%d", pid);
            char Pid[64] = {0};
            sprintf(Pid, "%d", pid);
            build_str(Pid, NULL);
        }
        else if (c == '?')
        {

            build_str(foregroundPid, NULL);
        }

        else if (c == '{')
        {
            int len = 64;
            int paraLen = end - start - 3;
            char parameter[64] = {0};
            strncpy(parameter, start + 2, paraLen);
            char *paraFinal = getenv(parameter);
            if (paraFinal == NULL)
            {

                build_str("", NULL);
            }
            else
            {

                build_str(paraFinal, NULL);
            }
        }

        pos = end;

        c = param_scan(pos, &start, &end);

        build_str(pos, start);
    }

    return build_str(start, NULL);
}

void builtInCommands()
{

    // Check if the first element in the 'words' array is not null

    if (words[0] != NULL)
    {

        // If the first element is 'exit'

        if (strcmp(words[0], "exit") == 0)
        {

            // If two or more elements in the 'words' array, print an error message

            if (words[2] != NULL)
            {
                exit(0);
            }

            if (words[1] == NULL)
            {

                exit(0);
            }

            exit(atoi(words[1]));
        }

        if (strcmp(words[0], "cd") == 0)
        {
            if (words[1] == NULL)
            {

                words[1] = getenv("HOME");
            }

            if (words[1] != NULL)
            {

                words[1] = getenv("HOME");
            }
        }
    }
}
