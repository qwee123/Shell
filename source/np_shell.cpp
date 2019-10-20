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

void execPipeCmd(vector<vector<char*>>*);
void function_ls();
void function_cat(vector<char*>*);
void function_executable(vector<char*>*);
void printEnv(vector<char*>*,char**);
void setEnv(vector<char*>*);
void parseCmd(char*,vector<vector<char*>>*);

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

void execPipeCmd(vector<vector<char*>> *parsed_cmd){
    int pipe_num = (*parsed_cmd).size(); 
    int status;
    int pipefd1[2];
    if(pipe(pipefd1) < 0){
	cerr << "Cannot pipe()" << endl;
	exit(-1);
    }

    for(int i = 0;i < pipe_num;i++){
    	int pipefd2[2];
	if(pipe(pipefd2) < 0){
	    cerr << "Cannot pipe()!" <<endl;
	    exit(-1);
	}

	pid_t pid = fork();
	if(pid == 0){
	    close(pipefd1[1]);
	    if(i > 0){
	        dup2(pipefd1[0],STDIN_FILENO);
	    }
	    close(pipefd1[0]);	    

	    close(pipefd2[0]);
	    if(i < pipe_num-1){ //not last cmd
	    	dup2(pipefd2[1],STDOUT_FILENO);
	    }
	    close(pipefd2[1]);

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
	        function_ls();
	    }
	    else if(!strcmp((*parsed_cmd)[i][0],"cat")){
	        function_cat(&((*parsed_cmd)[i]));
	    }
	    else{
	    	function_executable(&((*parsed_cmd)[i]));
	    }
	}
	else{
	    close(pipefd1[0]);
	    close(pipefd1[1]);
	    if(i < pipe_num-1){
	    	pipefd1[0] = pipefd2[0];
		pipefd1[1] = pipefd2[1];
	    }
	    else{
	        close(pipefd2[0]);
		close(pipefd2[1]);
	    }
	    wait(NULL);
	}
    }    
}

void printEnv(vector<char*> *args,char **envp){
    char *var;
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
    if((*args).size() < 3)    return;
    setenv((*args)[1],(*args)[2],1);
}

void parseCmd(char *cmd,vector<vector<char*>> *parsedc){
    char* sub_cmd;
    const char* delim="|";
    vector<char*> pipe_cmds;

    sub_cmd = strtok(cmd,delim);
    while(sub_cmd){
        pipe_cmds.push_back(sub_cmd);
        sub_cmd = strtok(NULL,delim);
    }

    const char* delim2 = " ";
    for(int i = 0;i < pipe_cmds.size();i++){
        (*parsedc).push_back({});
        sub_cmd = strtok(pipe_cmds[i],delim2);
        while(sub_cmd){
            if(sub_cmd != "")   (*parsedc).back().push_back(sub_cmd);
            sub_cmd = strtok(NULL,delim2);
        }
    }
    vector<char*>().swap(pipe_cmds); //release pipe_cmds
}

void function_ls(){
    if(execlp("ls","ls",NULL)<0){
    	cerr << "Error with ls!" << flush;
    	exit(0);
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
