#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <vector>
#include <signal.h>
using namespace std;

pid_t getFork();
int findOutpipe(int);
int checkPipe();
void pipeCountDown();
int mergePipeDelay(int);
void removePipe(int);
void pipeRedirection(int,int,char*,vector<char*>*);
void redirectInpipe(int);
void redirectOutpipe(int,char *);
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
const char *normalDelim = "|";
const char *plusErrDelim = "!";

void childHandler(int signo){
    int status;
    int pid;
    while((pid = waitpid(-1,&status,WNOHANG)) > 0) ; 
	//cerr << pid << " exits!(with signal)" << endl;
}

void initialize(){
    unsetenv("PATH");
    setenv("PATH","bin:.",1);
}

int main(int argc,char* argv[],char* envp[]){
    initialize();    
    signal(SIGCHLD,childHandler);
    bool exitflag = false;   
    char *cmd = new char[15000];
    vector<vector<char*>> parsed_cmd;
    //cout << &parsed_cmd <<endl;
    //cout << &cmd <<endl;
    while(!exitflag){
        cout << "% "<< flush;
        cin.getline(cmd,15000);
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

int findOutpipe(int delay_int){
    int out_pipe = mergePipeDelay(delay_int); //-1 means need to push new delayPipe, other return values mean there is already one in vector.
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
    return out_pipe;	
}

void redirectInpipe(int in_pipe){
    close(pipes[in_pipe].fd[1]);
    dup2(pipes[in_pipe].fd[0],STDIN_FILENO);
    close(pipes[in_pipe].fd[0]);
}

void redirectOutpipe(int out_pipe,char *pipeType){
    close(pipes[out_pipe].fd[0]);
    dup2(pipes[out_pipe].fd[1],STDOUT_FILENO);
   if(pipeType == plusErrDelim)
        dup2(pipes[out_pipe].fd[1],STDERR_FILENO);
    close(pipes[out_pipe].fd[1]);
}

pid_t getFork(){
    pid_t pid = fork();
    int retry = 0;
    while(pid < 0){
        if((++retry) > 20){
	    cerr << "Cannot fork!!" << endl;
	    exit(-1);
	}
	sleep(1);
	pid = fork();
    }
    return pid;
}

void pipeRedirection(int in_pipe, int out_pipe, char *pipeType,vector<char*> *cmd){
    if(in_pipe > -1)
	redirectInpipe(in_pipe);
    if(out_pipe > -1)
	redirectOutpipe(out_pipe,pipeType);

    int cmd_len = (*cmd).size();
    if(cmd_len>2 && !strcmp((*cmd)[cmd_len-2],">")){
    	int fd = open((*cmd).back(),O_RDWR|O_CREAT|O_TRUNC,S_IWUSR|S_IRUSR);
	dup2(fd,STDOUT_FILENO);
	close(fd);
	(*cmd).pop_back();
	(*cmd).pop_back();
    }
    (*cmd).push_back(NULL);//exec() based function need a NULL at the end of argv
}

void execPipeCmd(vector<vector<char*>> *parsed_cmd){
    int pipe_num = (*parsed_cmd).size(); 
    int status;

    for(int i = 0;i < pipe_num;i++){
	pipeCountDown();

    	int out_pipe;
	char *delay = (*parsed_cmd)[i].back();
	(*parsed_cmd)[i].pop_back();
	if(!(i == pipe_num-1&&delay == NULL)){
            int delay_int = delay == NULL?1:atoi(delay);
            out_pipe = findOutpipe(delay_int);
	}
        else  //output directly
            out_pipe = -1;
    
        // find if the pipe is '|','!' or NULL
	char *pipeType = (*parsed_cmd)[i].back();
	(*parsed_cmd)[i].pop_back();

	int in_pipe = checkPipe(); //check if this command is piped. return pipe index, or -1(no pipe)
	pid_t pid = getFork(); // fork until success,or exit if cannot fork after 20 retries.
        
	if(pid == 0){
            pipeRedirection(in_pipe,out_pipe,pipeType,&(*parsed_cmd)[i]);

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
	    //wait(NULL);
	    if(i == pipe_num-1&&delay == NULL)
	        waitpid(pid,&status,0);
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
    char* saveptr;
    vector<char*> pipe_cmds;

    sub_cmd = strtok_r(cmd,normalDelim,&saveptr);
    while(sub_cmd){
	char* sub_sub_cmd = strtok(sub_cmd,plusErrDelim);
	while(sub_sub_cmd){
	    pipe_cmds.push_back(sub_sub_cmd);
	    pipe_cmds.push_back((char*)plusErrDelim);  //push '!'
	    sub_sub_cmd = strtok(NULL,plusErrDelim);
	}
	pipe_cmds.pop_back();
	pipe_cmds.push_back((char*)normalDelim); //push "|"
        sub_cmd = strtok_r(NULL,normalDelim,&saveptr);
    }
    pipe_cmds.pop_back();

    const char* delim2 = " ";
    for(int i = 0;i < pipe_cmds.size();i++){
	if(!strcmp(pipe_cmds[i],"|")||!strcmp(pipe_cmds[i],"!")){
	    *((*parsedc).back().end()-2) = pipe_cmds[i];
	    continue;
	}

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
	else{ //valid command
	    (*parsedc).back().push_back(NULL); //indicate the following pipe symbol, NULL means no pipe
    	    (*parsedc).back().push_back(NULL); //indicate the pipeDelay num is 0 or unknown
	}
    }
    vector<char*>().swap(pipe_cmds); //release pipe_cmds
}

void function_ls(vector<char*> *args){
    if(execvp("ls",(*args).data())<0){
    	cerr << "Unknown command: [" <<(*args)[0] << "]." << endl;
    	exit(-1);
    }
}

void function_cat(vector<char*> *args){    
    if(execvp("cat",(*args).data()) < 0){
        cerr << "Unknown command: [" <<(*args)[0] << "]." << endl;
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
