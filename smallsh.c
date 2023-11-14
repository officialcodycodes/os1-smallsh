#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>


#define MAX_LENGTH 2048  // The maximum length of a command
#define MAX_ARGS 512     // The maximum number of arguments

int exitMethod = -5;  // The status of the last foreground process

struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};

// Function that handles the expansion of $$ in the input string
void handle_expansion(){
   
}


void signal_handlers(){
   
   // Handle CTRL+C


}

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

void exit_program(){
   // Kill all other processees or jobs started by the shell
   
   printf("Exiting...\n");
   exit(0);
}

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
      printf("ERROR: Directory not found.\n");
      fflush(stdout);
   }
}

void status(){
   // printf("In status...\n");

   // Return "exit status 0" if no foreground process has been run yet
   if(exitMethod == -5)
      printf("exit status 0\n");
   
   // If child process terminates normally, print exit status
   else if(WIFEXITED(exitMethod) != 0)
      printf("exit status %d\n", WEXITSTATUS(exitMethod));
   
   // If child process was terminated by a signal, print signal number
   else
      printf("terminated by signal %d\n", WTERMSIG(exitMethod));
   fflush(stdout);

}

void parse_cmd(char** arg_list, int num_of_args){
   // If the first character is empty or a "#" comment, do nothing
   if(arg_list[0][0] == '#' || arg_list[0][0] == '\n'){
      return;  
   }
   
   // Replace "\n" with "\0" at the end of the string
   size_t len = strlen(arg_list[num_of_args - 1]);
   if(len > 0 && arg_list[num_of_args - 1][len - 1] == '\n'){
      arg_list[num_of_args - 1][len - 1] = '\0';
   }

   // If the command is "exit", exit the shell
   if(strcmp(arg_list[0], "exit") == 0){
      exit_program(arg_list, num_of_args);
   }
   else if(strcmp(arg_list[0], "cd") == 0){
      change_directory(arg_list, num_of_args);
   }
   else if(strcmp(arg_list[0], "status") == 0){
      status(arg_list, num_of_args);
   }
   else{  // For all non-built-in commands, parent will fork off a child
      
   }

}

int main(){
   char* arg_list[MAX_ARGS];  // List of all arguments in a command
   char input_buffer[MAX_LENGTH];  // Buffer for user input

   // Handle signals (CTRL+C && CTRL+Z)
   signal_handlers();

   // Main program loop
   while(1){
      
      int num_of_args = user_cmd(arg_list, input_buffer);  // User inputs for shell cmds
      arg_list[num_of_args] = NULL;  // Set the last element to NULL
      
      // printf("In main...\n");

      parse_cmd(arg_list, num_of_args);  // Functionality of each of the cmds
   }

   return 0;
}
