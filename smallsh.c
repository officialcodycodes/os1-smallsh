/*********************************************************************
** Program Filename: smallsh.c
** Author: Cody DePeel
** Date: 11-18-2023
** Description: A simple shell program that can run commands and
**              perform basic shell functionality.
** Input: User input for shell commands
** Output: Output from shell commands
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>


#define MAX_LENGTH 2048  // The maximum length of a command
#define MAX_ARGS 512     // The maximum number of arguments

int last_exit_method = -5;  // The status of the last foreground process
int allow_background = 1;  // Flag for whether background processes are allowed
int processes[100];  // Array of process PIDs
int num_of_processes = 0;  // Number of processes in the array
int bg_children_running = 0; // Number of background children running

struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};

/*********************************************************************
** Function: expand_instances
** Description: Expands any instances of "$$" in the input string
** Parameters: char** arg_list, int num_of_args
** Pre-Conditions: arg_list is a list of arguments, num_of_args is the
**                 number of arguments in arg_list
** Post-Conditions: Any instances of "$$" in arg_list are expanded to
**                  the process ID
*********************************************************************/
void expand_instances(char** arg_list, int num_of_args){
   int i;
   char temp_str[MAX_LENGTH];
   char pid_str[10];

   // Loop through the arguments
   for(i = 0; i < num_of_args; i++){
      // If the argument contains "$$", expand it
      if(strstr(arg_list[i], "$$") != NULL){
         // Get the process ID and convert it to a string
         sprintf(pid_str, "%d", getpid());

         // Replace the "$$" with the process ID
         strcpy(temp_str, arg_list[i]);
         strcpy(arg_list[i], "");
         strcat(arg_list[i], pid_str);
         strcat(arg_list[i], temp_str + 2);
      }
   }
}

/*********************************************************************
** Function: check_for_SIGINT
** Description: Checks if the last foreground process was terminated
** Parameters: None
** Pre-Conditions: None
** Post-Conditions: If the last foreground process was terminated by
**                  a signal, the signal number is printed
*********************************************************************/
void check_for_SIGINT(){
   // If the child process was terminated by a signal, print the signal number
   if(WIFSIGNALED(last_exit_method) != 0){
      printf("terminated by signal %d\n", WTERMSIG(last_exit_method));
      fflush(stdout);
   }
}


/*********************************************************************
** Function: catch_SIGTSTP
** Description: Handles SIGTSTP when shell catches signal (CTRL+Z)
** Parameters: None
** Pre-Conditions: None
** Post-Conditions: If background processes are allowed, they are
**                  turned off. If background processes are not
**                  allowed, they are turned on.
*********************************************************************/
void catch_SIGTSTP(){
   // If background processes are allowed, turn them off
   if(allow_background){
      char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
      write(STDOUT_FILENO, message, 54);
      allow_background = 0;
   }
   // If background processes are not allowed, turn them on
   else{
      char* message = "\nExiting foreground-only mode\n";
      write(STDOUT_FILENO, message, 32);
      allow_background = 1;
   }
   fflush(stdout);
}


/*********************************************************************
** Function: signal_handlers
** Description: Handles signals (CTRL+C && CTRL+Z)
** Parameters: None
** Pre-Conditions: User presses CTRL+C or CTRL+Z
** Post-Conditions: SIGTSTP is handled by catch_SIGTSTP() and SIGINT
**                  is ignored
*********************************************************************/
void signal_handlers(){
   // Handle SIGTSTP when shell catches signal (CTRL+Z)
   SIGTSTP_action.sa_handler = catch_SIGTSTP;
   sigfillset(&SIGTSTP_action.sa_mask);
   SIGTSTP_action.sa_flags = SA_RESTART;
   
   // Ignore SIGINT when shell catches signal (CTRL+C)
   SIGINT_action.sa_handler = SIG_IGN;

   sigaction(SIGTSTP, &SIGTSTP_action, NULL); // Register the SIGTSTP handler
   sigaction(SIGINT, &SIGINT_action, NULL); // Register the SIGINT handler
}


/*********************************************************************
** Function: user_cmd
** Description: Gets user input and parses it into a list of arguments
** Parameters: char** arg_list, char* input_buffer
** Pre-Conditions: arg_list is a list of arguments, input_buffer is
**                 a buffer for user input
** Post-Conditions: arg_list contains the arguments from user input
**                  and the number of arguments is returned
*********************************************************************/
int user_cmd(char** arg_list, char* input_buffer){
   int num_of_args = 0;
   char temp_input[MAX_LENGTH];


   printf(": "); // Print a colon to prompt input
   fflush(stdout);
   fgets(input_buffer, MAX_LENGTH, stdin);  // Get user input

   // Obtain the first token from user's input
   char* token = strtok(input_buffer, " ");

   // Loop through the input string
   while(token != NULL){
      arg_list[num_of_args] = token;
      // handle_expansion();  // Handle $$ in the input string

      token = strtok(NULL, " "); // Get the next stuff in the input string
      num_of_args++;
   }
   return num_of_args;
}

/*********************************************************************
** Function: exit_program
** Description: Exits the shell
** Parameters: None
** Pre-Conditions: User enters "exit" command in the shell
** Post-Conditions: If there are any background processes running,
**                  they are killed and the shell exits with status 1.
*********************************************************************/
void exit_program(){
   // Kill all other processees or jobs started by the shell
   if(num_of_processes > 0){
      int i;
      for(i = 0; i < num_of_processes; i++){
         kill(processes[i], SIGKILL);
      }
      exit(1);
   }
   else{
      exit(0);
   }
}


/*********************************************************************
** Function: change_directory
** Description: Changes the current working directory of the shell
** Parameters: char** arg_list, int num_of_args
** Pre-Conditions: User enters "cd" command in the shell, which is
**                 followed by a file path (optional) within arg_list
** Post-Conditions: If no file path is provided, the shell changes to
**                  the HOME directory. If a file path is provided,
**                  the shell changes to that directory. If the user
**                  tries to go to a bad directory, an error message
**                  is printed.
*********************************************************************/
void change_directory(char** arg_list, int num_of_args){
   int status;

   if(num_of_args == 1){
      // If no arguments, change to the HOME directory
      status = chdir(getenv("HOME"));
   }
   else{
      // A file path was provided -- in SECOND argument (1st is "cd")
      status = chdir(arg_list[1]);
   }

   // If the user tries to go to a bad directory, tell the user
   if(status == -1){  // chdir() returns -1 if error occurred
      printf("ERROR: bad directory\n");
      fflush(stdout);
   }
}


/*********************************************************************
** Function: status
** Description: Prints the exit value or terminating signal of the
**              last foreground process
** Parameters: None
** Pre-Conditions: User enters "status" command in the shell
** Post-Conditions: The exit value or terminating signal of the last
**                  foreground process is printed.
*********************************************************************/
void status(){
   // printf("In status...\n");

   // Return "exit value 0" if no foreground process has been run yet
   if(last_exit_method == -5)
      printf("exit value 0\n");
   
   // If child process terminates normally, print exit value
   else if(WIFEXITED(last_exit_method) != 0)
      printf("exit value %d\n", WEXITSTATUS(last_exit_method));
   
   // If child process was terminated by a signal, print signal number
   else
      printf("terminated by signal %d\n", WTERMSIG(last_exit_method));
   fflush(stdout);

} 


/*********************************************************************
** Function: child_fork
** Description: Handles the forking of a child process and the
**              execution of the command in the child process.
** Parameters: char** arg_list, int is_background
** Pre-Conditions: A child process has been forked, and the command
**                 is to be executed in the child process.
** Post-Conditions: The command is executed in the child process.
*********************************************************************/
void child_fork(char** arg_list, int is_background){
   int i;
   int have_input = 0;  // Flag for whether the command has input redirection
   int have_output = 0;  // Flag for whether the command has output redirection
   char input_file[MAX_LENGTH];
   char output_file[MAX_LENGTH];

   // Loop through the arguments
   for(i = 0; arg_list[i] != NULL; i++){
      // If the argument contains "<", set the input file
      if(strcmp(arg_list[i], "<") == 0){
         arg_list[i] = NULL; // Set the "<" to NULL
         strcpy(input_file, arg_list[i + 1]);
         have_input = 1;
         i++;
      }
      // If the argument contains ">", set the output file
      else if(strcmp(arg_list[i], ">") == 0){
         arg_list[i] = NULL; // Set the ">" to NULL
         strcpy(output_file, arg_list[i + 1]);
         have_output = 1;
         i++;
      }
   }

   // If the command has input redirection, open the input file
   if(have_input){
      int input_fd = open(input_file, O_RDONLY);
      if(input_fd == -1){
         printf("cannot open %s for input\n", input_file);
         fflush(stdout);
         exit(1);
      }
      dup2(input_fd, 0);  // Redirect stdin to the input file
      close(input_fd);
   }

   // If the command has output redirection, open the output file
   if(have_output){
      int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if(output_fd == -1){
         printf("cannot open %s for output\n", output_file);
         fflush(stdout);
         exit(1);
      }
      dup2(output_fd, 1);  // Redirect stdout to the output file
      close(output_fd);
   }

   // Allow CTRL+C to kill foreground processes
   if(!is_background){
      // 
      SIGINT_action.sa_handler = SIG_DFL;
   }
   sigaction(SIGINT, &SIGINT_action, NULL); // Register the SIGINT handler

   // Execute the command
   if(execvp(arg_list[0], arg_list) == -1){
      // If the command fails, print an error message
      perror(arg_list[0]);
      fflush(stdout);
      exit(1);
   }
}


/*********************************************************************
** Function: other_cmds
** Description: Handles all non-built-in commands (forks off a child)
** Parameters: char** arg_list, int num_of_args
** Pre-Conditions: User has entered a non-built-in command (not "cd", 
**                 "status", or "exit")
** Post-Conditions: The command is executed in a child process.
*********************************************************************/
void other_cmds(char** arg_list, int num_of_args){
   int is_background = 0;  // Flag for whether the process is background or not
   int child_exit_method = -5;  // The exit value of the child process
   pid_t spawnpid = -5;

   // Determine if the process is a background process
   if(strcmp(arg_list[num_of_args - 1], "&") == 0){
      if(allow_background){
         // Only set the flag if background processes are allowed
         is_background = 1;  
      }
      arg_list[num_of_args - 1] = NULL;  // Set the "&" to NULL
   }

   spawnpid = fork();
   processes[num_of_processes] = spawnpid;  // Add the process to the array of processes
   num_of_processes++;  // Increment the number of processes

   // Fork off a child process
   switch (spawnpid){
      case -1:
         perror("Hull Breach!\n");
         fflush(stdout);
         exit(1);
         break;
      case 0:
         // Child process
         child_fork(arg_list, is_background);
         break;
      default:
         // Parent process
         if(!is_background){
            /*  If it is not a background process, wait for the
            *  child process to terminate and print the exit value or
            *  terminating signal */
            waitpid(spawnpid, &child_exit_method, 0);
            last_exit_method = child_exit_method;  // Update the exit value
            check_for_SIGINT(); // Check for CTRL+C
         }
         else{
            // Print the background process ID
            // waitpid(spawnpid, &child_exit_method, WNOHANG);
            printf("background pid is %d\n", spawnpid);
            fflush(stdout);
            bg_children_running++;
         }
         break;
   }
}


/*********************************************************************
** Function: parse_cmd
** Description: Parses the command and calls the appropriate function.
** Parameters: char** arg_list, int num_of_args
** Pre-Conditions: A command has been entered by the user, and the
**                 command has been parsed into arg_list.
** Post-Conditions: The appropriate function is called based on the
**                  command entered by the user.
*********************************************************************/
void parse_cmd(char** arg_list, int num_of_args){
   // If the first character is empty or a "#" comment, do nothing
   if(arg_list[0][0] == '#' || arg_list[0][0] == '\n'){
      return;  
   }
   
   /* We don't want \n because otherwise it won't match the strcmp()'s,
     so replace "\n" with "\0" at the end of the string  */
   size_t len = strlen(arg_list[num_of_args - 1]);
   if(len > 0 && arg_list[num_of_args - 1][len - 1] == '\n'){
      arg_list[num_of_args - 1][len - 1] = '\0';
   }

   // Expand any instances of "$$" in the command into the process ID
   expand_instances(arg_list, num_of_args);

   // If the command is "exit", exit the shell
   if(strcmp(arg_list[0], "exit") == 0){
      exit_program(arg_list, num_of_args);
   }
   else if(strcmp(arg_list[0], "cd") == 0){
      change_directory(arg_list, num_of_args);
   }
   else if(strcmp(arg_list[0], "status") == 0){
      status();
   }
   else{  // For all non-built-in commands, parent will fork off a child
      other_cmds(arg_list, num_of_args);
   }
}


/*********************************************************************
** Function: check_children
** Description: Checks if any background children have completed
** Parameters: None
** Pre-Conditions: None
** Post-Conditions: If a background child has completed, the PID and
**                  exit value or terminating signal is printed.
*********************************************************************/
void check_children(){
   // Check if any background children have completed
   if(bg_children_running){
      pid_t child_pid = waitpid(-1, &last_exit_method, WNOHANG);

      // If a child process has completed, print out PID and exit value
      if (child_pid > 0){
         printf("background pid %d is done: ", child_pid);
         fflush(stdout);

         // If child process terminates normally, print exit value
         if(WIFEXITED(last_exit_method) != 0){
            printf("exit value %d\n", WEXITSTATUS(last_exit_method));
         }
         // If child process was terminated by a signal, print signal number
         else{
            printf("terminated by signal %d\n", WTERMSIG(last_exit_method));
         }
      }
   }
}


/*********************************************************************
** Function: main
** Description: Main function for the shell
** Parameters: None
** Pre-Conditions: None
** Post-Conditions: The shell is running and waiting for user input.
*********************************************************************/
int main(){
   char* arg_list[MAX_ARGS];  // List of all arguments in a command
   char input_buffer[MAX_LENGTH];  // Buffer for user input

   // Handle signals (CTRL+C && CTRL+Z)
   signal_handlers();

   // Main program loop
   while(1){
      check_children();

      int num_of_args = user_cmd(arg_list, input_buffer);  // User inputs for shell cmds
      arg_list[num_of_args] = NULL;  // Set the last element to NULL
      
      // printf("In main...\n");

      parse_cmd(arg_list, num_of_args);  // Functionality of each of the cmds
   }

   return 0;
}
