#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>

#define MAXARG 80
#define MAXLINE 80
#define BG "BACKGROUND"
#define FG "FOREGROUND"
#define RU "RUNNING"
#define ST "STOP"

struct job{
    pid_t pid;
    int job_id;
    char* state;
    char* bg_fg;
    char commandLine[MAXLINE];
    struct job* next;
};

struct job* foregroundHolder = NULL;
struct job* backgroundList = NULL;

void deleteJob(pid_t pid, bool removeFromMemory);
int totalJobs = 1;
int stdoutHolder;

void prompt(char * input){

	printf("prompt > ");
	fgets(input, MAXLINE, stdin);
}

int readCommand(char * userInput, char tokens[][MAXARG]){
	int x = 0;
	int i = 0;	
	for(int y = 0; userInput[y] != '\0'; y++){
	   if(userInput[y] == 32 || userInput[y] == 9){	
		tokens[x][i] = '\0';
  		x++;
		i = 0;
	}else{
	    (tokens[x][i]) = userInput[y];
	    i++;
	}
}
	return x + 1;
}

void printDirectory(){
	char directory[80];
	getcwd(directory, sizeof(directory));
	printf("CURRENT WORKING DIRECTORY = %s\n", directory);
}

struct job* findJob(bool idIsNum, int jobNum, pid_t pid){
   for(struct job* temp = backgroundList; temp != NULL; temp = temp->next){
        if(idIsNum){
            if(temp->job_id == jobNum){
                return temp;
            }

        } else{
            if(temp->pid == pid){
                return temp;
            }
        }
   }
}

struct job* returnJob(char* tokens[]){
    struct job* temp;
    if(tokens[1][0] == '%'){
        int jobNum = atoi(tokens[1] + 1);
        temp = findJob(true, jobNum, -1);
    } else{
        int jobPid = atoi(tokens[1]);
        temp = findJob(false, -1, jobPid);
    }
    return temp;
}

void builtInCommand(char* tokens[MAXARG]){
       	if(strcmp(tokens[0], "pwd") == 0){
	    printDirectory();
     	    
     } else if(strcmp(tokens[0], "cd") == 0){
	    if(chdir(tokens[1]) == -1){
		printf("FAILED TO CHANGED DIRECTORY\n");
	    } 
     }else if(strcmp(tokens[0], "jobs") == 0){
        for(struct job * temp = backgroundList; temp != NULL; temp = temp->next){
            printf("[%d] (%d) %s %s", temp->job_id, temp->pid, temp->state, temp->commandLine);
        } 

     }else if(strcmp(tokens[0], "fg") == 0 && foregroundHolder == NULL){
        struct job* temp = returnJob(tokens);
                deleteJob(temp->pid, false);
                foregroundHolder = temp;
              if(foregroundHolder->state == ST){
                  foregroundHolder->state = RU;
                  foregroundHolder->bg_fg = FG;
                kill(temp->pid, SIGCONT);    
                pause();
                pause();
              }else{
                  foregroundHolder->state = RU;
                  foregroundHolder->bg_fg = FG;
                  pause();
                 }
     } else if(strcmp(tokens[0], "bg") == 0){
            struct job* temp = returnJob(tokens);
                if(temp->state == ST){
                    temp-> state = RU;
                    kill(temp->pid, SIGTSTP);
                    kill(temp->pid, SIGCONT);
                }
     } else if(strcmp(tokens[0], "kill") == 0){
            struct job* temp = returnJob(tokens);
                kill(temp->pid, SIGINT);
     }
}
void insertJob(pid_t pid, int jobId, char* state, char* bg_fg, char* command){
   struct job* temp = (struct job*) malloc(sizeof(struct job)); 
   temp->pid = pid;
   temp->job_id = jobId;
    temp->state = state;
    temp->bg_fg = bg_fg;
    strcpy(temp->commandLine, command);

    if(strcmp(bg_fg, BG) == 0){
       temp->next = backgroundList;
       backgroundList = temp;
    } else{
        temp->next = foregroundHolder;
        foregroundHolder = temp;
    }

}

void deleteJob(pid_t pid, bool removeFromMemory){
    struct job* temp = backgroundList;    
    if(temp->next == NULL){
        if(pid == temp->pid){
            if(removeFromMemory)
                free(temp);
            backgroundList = NULL;
        } 
    }
   for(;backgroundList != NULL && temp->next != NULL; temp = temp->next){
        if(pid == temp->pid){
            backgroundList = temp->next;
            if(removeFromMemory)
                free(temp);
            break;
        } else if(pid == temp->next->pid){
            if(temp->next->next != NULL){
                struct job* holder = temp->next->next;
                if(removeFromMemory)
                    free(temp->next);
                temp->next = holder;
            } else{
                if(removeFromMemory)
                    free(temp->next);
                temp->next = NULL;
                break;
            }
        }
   }
 }
void deleteBackgroundList(){
    struct job* temp;
    for(struct job* x = backgroundList; x != NULL;){
        temp = x;
        x= x->next;
        kill(temp->pid, SIGINT);
        free(temp);
    }
}

bool tokenizeInput(char tokens[][MAXLINE], char* arg[], int num){
    bool bg = false;
    for(int i =0; i < num; i++){
        tokens[i][strcspn(tokens[i], "\n")] = 0;
        arg[i] = tokens[i];
        if(strcmp(arg[i], "&") == 0){
            bg = true;
        }
    }
    arg[num] = NULL;
    return bg;

}
void redirection(char* tokens[MAXLINE])
{
    int out, in;
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    if((strcmp(tokens[1], ">") == 0 || strcmp(tokens[1], ">>")==0)&& tokens[3] == NULL){
       stdoutHolder = dup(STDOUT_FILENO);
       if(strcmp(tokens[1], ">") == 0){
            out = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, mode);
       }else{
            out = open(tokens[2], O_CREAT|O_WRONLY|O_APPEND, mode);
       }
       dup2(out, STDOUT_FILENO); 
       close(out);
     } else if(strcmp(tokens[1], "<") == 0 && tokens[3] == NULL){
        in = open(tokens[2], O_RDONLY);
        dup2(in, STDIN_FILENO);
        close(in);
     } else if(strcmp(tokens[1], "<") == 0 && (strcmp(tokens[3], ">") == 0 ) || strcmp(tokens[3], ">>")==0){
        in = open(tokens[2], O_RDONLY);
        if(strcmp(tokens[3], ">") == 0){
            out = open(tokens[4], O_WRONLY | O_CREAT | O_TRUNC, mode);
        } else{
            out = open(tokens[4], O_CREAT|O_WRONLY|O_APPEND, mode);
        }
        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
        close(in);
        close(out);        
     }
}

void eval(char tokens[][MAXLINE], int num, char * commandLine){
    char *arg[MAXLINE];
    bool builtIn = false;
    bool redirecting = false;
    bool bg = tokenizeInput(tokens, arg, num);
    if(strcmp(arg[0], "pwd") == 0 || strcmp(arg[0], "cd") == 0 || strcmp(arg[0], "jobs") == 0 || strcmp(arg[0], "fg") == 0||strcmp(arg[0], "bg") == 0 || strcmp(arg[0], "kill") == 0)
            builtIn = true;
    if(arg[1] != NULL)
	if(strcmp(arg[1], ">") == 0 || strcmp(arg[1], "<") == 0 || strcmp(arg[1],">>") == 0)
            redirecting = true; 
                
	pid_t pid;
	
	if(builtIn){
        if(redirecting){
            redirection(arg);    
            } 
        builtInCommand(arg);
        if(redirecting)
            dup2(stdoutHolder, STDOUT_FILENO);
    } else{
	  if((pid = fork()) == 0){
          setpgid(pid, 0);
          if(redirecting){
            redirection(arg);
            arg[1] = NULL;
           }
          if(execv(arg[0], arg) < 0){
            if(execvp(arg[0], arg)< 0){
		    	printf("%s: Command not found.\n", arg[0]);
		    	exit(0);
		} 
        }
	}
	}
	if(!bg && !builtIn){
        insertJob(pid, totalJobs, RU, FG, commandLine);
        totalJobs++;
	    pause();
	} else {
        if(!builtIn){
            insertJob(pid, totalJobs, RU, BG, commandLine);
            totalJobs++;
	        printf("%d %s\n", pid, arg[0]);
        }
 	}
}	
void sigTstp(int i){
    if(foregroundHolder != NULL){
        kill(foregroundHolder->pid, SIGTSTP);
        insertJob(foregroundHolder->pid, foregroundHolder->job_id, ST, BG, foregroundHolder->commandLine);
        free(foregroundHolder);
        foregroundHolder = NULL;
    }

}

void sigKill(int i){
	if(foregroundHolder != NULL){
		kill(foregroundHolder->pid, SIGINT);
	}

}
void sigChld(int i){
    pid_t pid;
    int status;
	while((pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0){
    if(WIFSTOPPED(status)){

    } else if( WIFSIGNALED(status) || WIFEXITED(status)){
        if(foregroundHolder != NULL && pid == foregroundHolder->pid){
            free(foregroundHolder);
            foregroundHolder = NULL;
        } else{
        deleteJob(pid, true);
        }
    } else{

    }
}
}

int main(){
	int numTokens = 0;
	char userInput[MAXLINE];	
	char commandTokens[MAXARG][MAXLINE];
	signal(SIGINT, sigKill);
	signal(SIGCHLD, sigChld);
    signal(SIGTSTP, sigTstp);
	while(1){
	    prompt(userInput);
	    if(strcmp(userInput, "quit\n") == 0 || feof(stdin)){
		    deleteBackgroundList();
            exit(0);
	      }
	numTokens = readCommand(userInput, commandTokens);
	
	eval(commandTokens, numTokens, userInput);



	}

}
