#np_project1
all: np_shell.cpp
	g++ np_shell.cpp -o npshell
clean:
	rm -f npshell
