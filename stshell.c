#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "errno.h"
#include "unistd.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

//Signal handler function to ignore interrupt signal (ctrl+c)
void sig_handler(int signo)
{
  if (signo == SIGINT)
    printf("\nSIGINT ignored by stshell.\n");

}

int main() {

	//Variables for parsing the commands
	int i,j,k; //counters for command tokens for argv, argv2, argv3 accordingly
	char *argv[10]; //first command in a pipe
	char *argv2[10]; //second command in a pipe
	char *argv3[10]; //third command in a pipe
	char command[1024]; //original command
	char *token; //holds temporary tokens
	
	int file; //file descriptor for when file redirects are used
	char *infile; //input filename

	//flags
	int redirect; //0- no redirect to file, 1- write to file, 2-append to file
	int pipes; //counts number of pipes
	int directin; //0- no redirect from file, 1-redirect from file

	//listener for interrupt signal
	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		printf("\ncan't catch SIGINT\n");
	}

	//Main loop, runs until 'exit' is entered
	while (1) {
		//reset flags
		pipes = 0;
		redirect = 0;
		directin = 0;

		//get a command from the user
	    printf("hello: ");
	    fgets(command, 1024, stdin);
	    command[strlen(command) - 1] = '\0'; // replace \n with \0

		//if command is exit, stshell exits
		if(strcmp(command,"exit") == 0)
		{
			return 0;
		}

	    /* parse command line */
	    i = 0, j= 0, k = 0; //reset counters
	    token = strtok (command," ");

	    while (token != 0)
	    {
			//Redirect to file output
			if(strcmp(token, ">") == 0)
			{
				//printf("redirect 1 detected!\n");
				token = strtok (0, " ");
				redirect = 1;
				break;
			}
			else if(strcmp(token, ">>") == 0)
			{
				//printf("redirect 2 detected!\n");
				token = strtok (0, " ");
				redirect = 2;
				break;
			}
			else if(strcmp(token, "<" ) == 0)
			{
				token = strtok (0, " ");
				infile = token;
				directin = 1;
			}
				else if(strcmp(token, "|") == 0)
			{
				//printf("pipe detected!\n");
				pipes++;
				token = strtok (0, " ");
			}
			
			//save command in the correct array based on pipes
			if(pipes == 0)
			{
				argv[i] = token;
				i++;
			}
			else if(pipes == 1)
			{
				argv2[j] = token;
				j++;
			}
			else if(pipes == 2)
			{
				argv3[k] = token;
				k++;
			}
			
			token = strtok (0, " ");
	    }
	    argv[i] = 0;
		argv2[j] = 0;
		argv3[k] = 0;


	    //if received an empty command, loop back for another command
	    if (argv[0] == 0)
		{
			continue;
		}

		//if command was given with pipes
		if(pipes)
		{
	    	//create a pipe
			int p[2];

			if(pipe(p) < 0)
			{
				return 0;
			}

			int p2[2];

			if(pipe(p2) < 0)
			{
				return 0;
			}

			int pidL = fork();
			if(pidL == 0)
			{
				//in "leftmost" child

				//leftmost command may have redirect from file
				if(directin)
				{
					file = open(token, O_RDONLY);
					dup2(file, 0);
					close(file);
				}

				dup2(p[1], 1); //redirect std-out (fd = 1) into pipe write-end

				//close unused or cloned pipe ends
				close(p[0]); 
				close(p[1]);
				close(p2[0]);
				close(p2[1]);

				execvp(argv[0], argv); //execute leftmost command
			}

			int pidM = fork();
			if(pidM == 0)
			{
				//in "middle" child
				
				//redirect output to file if there is redirect to file AND if there is no second pipe
				if(redirect && pipes != 2)
				{
					if(redirect == 1)
					{
						//file = fopen(token, "w");
						file = open(token, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

					}
					else
					{
						//file = fopen(token, "a");
						file = open(token, O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
					}
					dup2(file, 1);
					close(file);
				}

				dup2(p[0], 0); //redirect std-in (fd = 0) to pipe read-end
				if(pipes == 2)
				{
					dup2(p2[1], 1); //redirect std-out (fd = 1) into pipe2 write-end
				}

				//close unused or cloned pipe ends
				close(p[0]);
				close(p[1]);
				close(p2[0]);
				close(p2[1]);

				execvp(argv2[0], argv2); //execute middle command
			}
			
			int pidR;
			if(pipes == 2)
			{
				pidR = fork();
				if(pidR == 0)
				{
					//in "rightmost" child
					
					//redirect output to file if needed
					if(redirect)
					{
						if(redirect == 1)
						{
							//file = fopen(token, "w");
							file = open(token, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

						}
						else
						{
							//file = fopen(token, "a");
							file = open(token, O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
						}
						dup2(file, 1);
						close(file);
					}

					dup2(p2[0], 0); //redirect std-in (fd = 0) to pipe read-end

					//close unused or cloned pipe ends
					close(p[0]);
					close(p[1]);
					close(p2[0]);
					close(p2[1]);

					execvp(argv3[0], argv3); //execute rightmost command
				}
			}

			//close pipe on parent so right children do not wait for input from parent
			close(p[0]);
			close(p[1]);
			close(p2[0]);
			close(p2[1]);

			//wait for all children before getting back to loop
			waitpid(pidL, 0, 0);
			waitpid(pidM, 0, 0);
			if(pipes == 2)
			{
				waitpid(pidR, 0, 0);
			}

		}
		else if(redirect && !pipes) //command is without pipes but has redirect to file
		{	
			if(fork() == 0)
			{
				if(redirect == 1)
				{
					//file = fopen(token, "w");
					file = open(token, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

				}
				else
				{
					//file = fopen(token, "a");
					file = open(token, O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
				}
				if(directin)
				{
					int file2 = open(token, O_RDONLY);
					dup2(file2, 0);
					close(file2);
				}
				dup2(file, 1);
				close(file);
				execvp(argv[0], argv);
			}

			wait(0);
		}
		else if(directin) //no pipes or redirect to file, but has redirect from file
		{
			file = open(token, O_RDONLY);
			dup2(file, 0);
			close(file);
		}
		else //no special elements
		{
			if(fork() == 0)
				execvp(argv[0], argv);

			wait(0);
	    }    
        
	}
}