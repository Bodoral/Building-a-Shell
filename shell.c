#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tokenizer.h"
#include "bgProcesses.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;


/* Most recently launched process */
pid_t mrp_id;



/* Built-in commands */
int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);
int cmd_fg(struct tokens *tokens);
int cmd_bg(struct tokens *tokens);



int progExe(char *program,struct tokens *tokens);
char * pathResolution(char * value);
int put_in_foreground(pid_t pid);


/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "prints the current working directory"},
    {cmd_cd, "cd", "change the current working directory"},
    {cmd_wait,"wait","waits until all background jobs have terminated"},
    {cmd_fg,"fg","move the process with id pid to the foreground"},
    {cmd_bg,"bg","resume a paused background process"},
    
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
    exit(0);
}

/*  Shows current working directory */
int cmd_pwd(struct tokens *tokens)
{
    long size;
    char *buf;
    
    //determine the system limit associated with a pathname
    size = pathconf(".", _PC_PATH_MAX);
    
    if ((buf = (char *)malloc((size_t)size)) != NULL)
        if(getcwd(buf, (size_t)size))
            printf("%s\n",buf);
    
    return 1;
}



/* Changes the current working directory */
int cmd_cd(struct tokens *tokens)
{
    if(tokens_get_length(tokens) == 1)
        chdir(getenv("HOME"));
    
    else
        chdir(tokens_get_token(tokens,1));
    
    return 1;
}



/* Waits until all background jobs have terminated */
int cmd_wait(struct tokens *tokens)
{
    
    while (waitpid(-1,NULL,0)!=-1);
    return 1;
    
}


/* Move the process with id pid to the foreground or the most recently launched process */
int cmd_fg(struct tokens *tokens)
{
    if(tokens_get_token(tokens,1) == NULL)
    {
        /* checks that the process has not yet exited */
        if(waitpid(mrp_id, NULL , WNOHANG) == 0)
        {
            kill( mrp_id, SIGCONT);
            put_in_foreground(mrp_id);
        }
        
        /* remove the process from the background processes list and update mrp_id */
        pop_process();
        mrp_id = last_process();
        return 1;
    }
    
    
    
    /* convert string to integrs */
    int dec= 0;
    char *num =tokens_get_token(tokens,1);
    int len = strlen(tokens_get_token(tokens,1));
    for(int i=0; i<len; i++)
        dec = dec * 10 + ( num[i] - '0' );
    
    
    kill( dec, SIGCONT);
    return put_in_foreground(dec);
    
}



int cmd_bg(struct tokens *tokens)
{
    if(tokens_get_token(tokens,1) == NULL)
       return kill( mrp_id, SIGCONT);
    
    
    /* convert string to integrs */
    int dec= 0;
    char *num =tokens_get_token(tokens,1);
    int len = strlen(tokens_get_token(tokens,1));
    for(int i=0; i<len; i++)
        dec = dec * 10 + ( num[i] - '0' );
    
    return kill( dec, SIGCONT);
    
}




/* Programs execution */
int progExe(char *program,struct tokens *tokens)
{
    int file_i = -1;
    int file_o = -1;
    int stdin_copy = dup(0);
    int stdout_copy = dup(1);
    int background_process =0;
    
    
    /* run process in the bachground */
    if(strcmp(tokens_get_token(tokens,tokens_get_length(tokens) -1) ,"&") == 0)
    {
        background_process = 1;
    }
    
    
    
    
    pid_t pid=fork();
    
    /* child process */
    if (pid==0)
    {
      
      
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTOU,SIG_DFL);
        signal(SIGTTIN,SIG_DFL);
        
       
        // program argv
        char *argv[tokens_get_length(tokens)+1];
        argv[0] = program;
        
        
        int j= 1;
        for(unsigned int i = 1 ; i < tokens_get_length(tokens); i++)
        {
            if(strcmp(tokens_get_token(tokens ,i),"<") == 0)
            {
                file_i = open(tokens_get_token(tokens,i+1),O_RDONLY);
                if(file_i)
                    dup2(file_i,STDIN_FILENO);
                i++;
            }
            
            else if(strcmp(tokens_get_token(tokens ,i),">") == 0)
            {
                file_o = open(tokens_get_token(tokens,i+1) ,O_RDWR|O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
                if (file_o)
                    dup2(file_o,STDOUT_FILENO);
                i++;
            }
            
            else if(strcmp(tokens_get_token(tokens,i),"&") == 0 && i == tokens_get_length(tokens) -1)
            {
                continue;
            }
            
            else
            {
                argv[j] = tokens_get_token(tokens,i);
                j++;
            }
        }
        
        
        argv[j] = NULL;
        execv(program,argv);

        exit(127); /* only if execv fails */
    }
    
    
    else if(background_process == 1)
    {
        setpgid(pid,pid);
        /* add the process to the background processes list */
        push_process(pid);
        mrp_id = pid;
     
    }
    
    
    /* pid!=0 and no & ; parent process then wait for child to exit */
    
    else
    {
        setpgid(pid,pid);
        put_in_foreground(pid);
    }
    
    
    if(file_i)
    {
        close(file_i);
        dup2(stdin_copy, file_i);
    }
    if(file_o)
    {
        close(file_o);
        dup2(stdout_copy,file_o);
    }
    return 1;
    
    
    
}



/*  Place process group in the foreground */
int put_in_foreground(pid_t pid)
{
    tcsetpgrp(0, pid);
    waitpid(pid,0,0);
    tcsetpgrp(0, getpid());
    
    /* Restore the shell’s terminal modes.  */
    tcgetattr(shell_terminal, &shell_tmodes);
    tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
    
    return 1;
}



/*  Gets full pathname of a program */
char * pathResolution(char * value)
{
    // gets PATH environment variable
    char *envvar = getenv("PATH");
    char *pathcpy = (char *)malloc(sizeof(char) * strlen(envvar));
    strcpy(pathcpy, envvar);
    
    
    // looks for the program in each directory on the PATH environment
    char *path = strtok(pathcpy,":");
    char  *p = (char *)malloc(sizeof(char) * strlen(envvar));
    while(path != NULL)
    {
        snprintf(p, sizeof(char) * strlen(envvar), "%s%s%s", path,"/", value);
        if (access(p ,F_OK) ==0)
            break;
    
       path= strtok(NULL, ":");
    }
    
    return p;
    
}



/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
            return i;
    return -1;
}


/* Intialization procedures for this shell */
void init_shell() {
    /* Our shell is connected to standard input. */
    shell_terminal = STDIN_FILENO;
    
    /* Check if we are running interactively */
    shell_is_interactive = isatty(shell_terminal);
    
    
    /* Shell ignores stop signals */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    
    
    
    if (shell_is_interactive) {
        /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
         * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
         * foreground, we'll receive a SIGCONT. */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);
        
        /* Saves the shell's process id */
        shell_pgid = getpid();
        
        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);
        
        /* Save the current termios to a variable, so it can be restored later. */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

int main(unused int argc, unused char *argv[]) {
    init_shell();
    
    static char line[4096];
    int line_num = 0;
    
    /* Please only print shell prompts when standard input is not a tty */
    if (shell_is_interactive)
        fprintf(stdout, "%d: ", line_num);
    
    while (fgets(line, 4096, stdin)) {
        /* Split our line into words. */
        struct tokens *tokens = tokenize(line);
        
        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0));
        
        if (fundex >= 0) {
            cmd_table[fundex].fun(tokens);
        } else {
            if(access(tokens_get_token(tokens,0),F_OK)== 0)
                progExe(tokens_get_token(tokens,0),tokens);
            
            else
                progExe(pathResolution(tokens_get_token(tokens,0)),tokens);
        }
        
        if (shell_is_interactive)
        /* Please only print shell prompts when standard input is not a tty */
            fprintf(stdout, "%d: ", ++line_num);
        
        /* Clean up memory */
        tokens_destroy(tokens);
    }
    
    return 0;
}
