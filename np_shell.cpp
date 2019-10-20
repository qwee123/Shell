#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <vector>
using namespace std;

int checkPipe();
void pipeCountDown();
int mergePipeDelay(int);
void removePipe(int);
void execPipeCmd(vector<vector<char*>>*);
void function_ls(vector<char*>*);
void function_cat(vector<char*>*);
void function_executable(vector<char*>*);
void printEnv(vector<char*>*,char**);
void setEnv(vector<char*>*);
void parseCmd(char*,vector<vector<char*>>*);

struct delayPipe{
    int fd[2];
    int delay_count;
};
vector<delayPipe> pipes;

void initialize(){
    unsetenv("PATH");
    setenv("PATH","bin:.",1);
}

int main(int argc,char* argv[],char* envp[]){
    initialize();    

    bool exitflag = false;   
    char *cmd = new char[100];
    vector<vector<char*>> parsed_cmd;
    //cout << &parsed_cmd <<endl;
    //cout << &cmd <<endl;
    while(!exitflag){
        cout << "% "<< flush;
        cin.getline(cmd,100);
	parseCmd(cmd,&parsed_cmd);
	if(parsed_cmd.size() == 0)    continue;

	if(!strcmp(parsed_cmd[0][0],"exit"))
	    exitflag = true;
	else if(!strcmp(parsed_cmd[0][0],"printenv"))
	    printEnv(&parsed_cmd[0],envp);
	else if(!strcmp(parsed_cmd[0][0],"setenv"))
	    setEnv(&parsed_cmd[0]);
	else{
	    execPipeCmd(&parsed_cmd);
	}
	parsed_cmd.clear();
    }
}

void childHandler(){
    int status;
    while (waitpid(-1,&status,WNOHANG) > 0);
}

int checkPipe(){
    for(int i = 0;i < pipes.size();i++){
 	if(pipes[i].delay_count == 0)	return i;
    }
    return -1;
}

int mergePipeDelay(int delay){
    for(int i = 0;i < pipes.size();i++)
	if(pipes[i].delay_count == delay)    return i;
    return -1;
}

void pipeCountDown(){
    for(int i = 0;i < pipes.size();i++)
	pipes[i].delay_count--;
}

void removePipe(int index){
    vector<delayPipe>::iterator itr = pipes.begin() + index;
    pipes.erase(itr,itr+1);
}

void execPipeCmd(vector<vector<char*>> *parsed_cmd){
    int pipe_num = (*parsed_cmd).size(); 
    int status;

    for(int i = 0;i < pipe_num;i++){
	pipeCountDown();

    	int out_pipe = -1;
	char *delay = (*parsed_cmd)[i].back();
	(*parsed_cmd)[i].pop_back();
	if(!(i == pipe_num-1&&delay == NULL)){
            int delay_int = delay == NULL?1:atoi(delay);
	    out_pipe = mergePipeDelay(delay_int); //-1 means need to push new delayPipe, other return values mean there is already one in vector.
	    if(out_pipe == -1){ //out_pipe should >= -1 in normal
		int pipefd[2];
		if(pipe(pipefd) < 0){
	            cerr << "Cannot pipe()!" <<endl;
		    exit(-1);
		}
		struct delayPipe p = {.fd = {pipefd[0],pipefd[1]}, .delay_count = delay_int};
		out_pipe = pipes.size();
		pipes.push_back(p);
	    }			
	}

	int in_pipe = checkPipe(); //check if this command is piped. return pipe index, or -1(no pipe)
	pid_t pid = fork();
	if(pid == 0){
	    if(in_pipe > -1){
		close(pipes[in_pipe].fd[1]);
		dup2(pipes[in_pipe].fd[0],STDIN_FILENO);
		close(pipes[in_pipe].fd[0]);
	    }

            if(out_pipe > -1){
		close(pipes[out_pipe].fd[0]);
		dup2(pipes[out_pipe].fd[1],STDOUT_FILENO);
		close(pipes[out_pipe].fd[1]);
	    }
			
	    int cmd_len = (*parsed_cmd)[i].size();
	    if(cmd_len>2&&!strcmp((*parsed_cmd)[i][cmd_len-2],">")){
		int fd = open((*parsed_cmd)[i][cmd_len-1],O_RDWR|O_CREAT|O_TRUNC,S_IWUSR|S_IRUSR);
		dup2(fd,STDOUT_FILENO);	
		close(fd);
		(*parsed_cmd)[i].pop_back();
		(*parsed_cmd)[i].pop_back();
	    }
	    (*parsed_cmd)[i].push_back(NULL);

	    if(!strcmp((*parsed_cmd)[i][0],"ls")){
		function_ls(&((*parsed_cmd)[i]));
	    }
	    else if(!strcmp((*parsed_cmd)[i][0],"cat")){
		function_cat(&((*parsed_cmd)[i]));
	    }
	    else{
		function_executable(&((*parsed_cmd)[i]));
	    }
	    cout << "Weird! Why are u seeing this!?" <<endl;
	    exit(0);
        }
	else{
	    if(in_pipe > -1){
		close(pipes[in_pipe].fd[0]);
		close(pipes[in_pipe].fd[1]);
		removePipe(in_pipe);
	    }
	    wait(NULL);
	}
    }    
}

void printEnv(vector<char*> *args,char **envp){
    char *var;
    pipeCountDown();
    if((*args).size() > 1){
	if((var = getenv((*args)[1])) != NULL)
    	    cout << var << endl;
    }
    else{
    	for(char **ptr = envp;*ptr != 0; ptr++)
	    cout << (*ptr) <<endl;
    }   
}

void setEnv(vector<char*> *args){
    pipeCountDown();
    if((*args).size() < 3)    return;
    setenv((*args)[1],(*args)[2],1);
}

void parseCmd(char *cmd,vector<vector<char*>> *parsedc){
    char* sub_cmd;
    const char* delim="|";
    const char* delim1="!";
    char* saveptr;
    vector<char*> pipe_cmds;

    sub_cmd = strtok_r(cmd,delim,&saveptr);
    while(sub_cmd){
	char* sub_sub_cmd = strtok(sub_cmd,delim1);
	while(sub_sub_cmd){
	    pipe_cmds.push_back(sub_sub_cmd);
	    sub_sub_cmd = strtok(NULL,delim1);
	}
        sub_cmd = strtok_r(NULL,delim,&saveptr);
    }

    const char* delim2 = " ";
    for(int i = 0;i < pipe_cmds.size();i++){
	if(i>0&&pipe_cmds[i][0] != ' '){
	    char *pipe_delay = strtok(pipe_cmds[i],delim2);
	    (*parsedc).back().back() = pipe_delay;
	    sub_cmd = strtok(NULL,delim2);
	}
	else
	    sub_cmd = strtok(pipe_cmds[i],delim2);
			
	(*parsedc).push_back({});
	while(sub_cmd){
	    if(sub_cmd != "")   (*parsedc).back().push_back(sub_cmd);
	    sub_cmd = strtok(NULL,delim2);
	}
	
	if((*parsedc).back().size()==0) //empty(invalid) command
            (*parsedc).pop_back();
	else //valid command
	    (*parsedc).back().push_back(NULL); //indicate the pipeDelay num is 0 or unknown
    }
    vector<char*>().swap(pipe_cmds); //release pipe_cmds
}

void function_ls(vector<char*> *args){
    if(execvp("ls",(*args).data())<0){
    	cerr << "Error with ls!" << flush;
    	exit(-1);
    }
}

void function_cat(vector<char*> *args){    
    if(execvp("cat",(*args).data()) < 0){
        cerr << "Error with cat!" <<endl;
    	exit(-1);
    }
}

void function_executable(vector<char*> *args){    
    cout << flush;
    int result = execvp((*args)[0],(*args).data());
    if(result == -1){
        cerr << "Unknown command: [" << (*args)[0] << "]." << endl;
    	exit(0);
    }
    else if(result < -1){
    	cerr << "Error with executable!" <<endl;
    }
}
