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

        if(!strcmp(parsed_cmd[0][0],"exit")){
            exitflag = true;
        }
	else if(!strcmp(parsed_cmd[0][0],"printenv")){
	    printEnv(&parsed_cmd[0],envp);   
	}
	else if(!strcmp(parsed_cmd[0][0],"setenv")){
	    setEnv(&parsed_cmd[0]);
	}
        else if(!strcmp(parsed_cmd[0][0],"ls")){
            function_ls();
        }
        else if(!strcmp(parsed_cmd[0][0],"cat")){
            function_cat(&parsed_cmd[0]);
        }
	else if(!strcmp(parsed_cmd[0][0],"number")){
	    function_executable(&parsed_cmd[0]);
	}
        else{
            function_executable(&parsed_cmd[0]);  
        }
	parsed_cmd.clear();
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
    pid_t pid = fork();
    if(pid == 0){
        execlp("ls","ls",NULL);
        cout << flush;
	exit(0);
    }
    else{
        wait(NULL);
    }
}

void function_cat(vector<char*> *args){    
    pid_t pid = fork();
    if(pid == 0){
	if((*args).size() > 3&&!strcmp((*args)[2],">")){
 	    int fd = open((*args)[3],O_RDWR|O_CREAT|O_TRUNC,S_IWUSR|S_IRUSR);
	    close(1);
	    dup(fd);
	    close(fd);           
	}
	execlp("cat","cat",(*args)[1],NULL);
	cout << endl;
	exit(0);
    }
    else{
	wait(NULL);
    }
}

void function_executable(vector<char*> *args){    
    pid_t pid = fork();
    if(pid == 0){
	int result = execvp((*args)[0],(*args).data());
        if(result == -1)
	    cerr << "Unknown command: [" << (*args)[0] << "].";
	cout << endl;
	exit(0);
    }
    else{
	wait(NULL);
    }
}
