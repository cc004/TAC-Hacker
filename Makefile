CXXFLAG = -O3 -Iinclude -std=c++11
LDFLAG = -lws2_32 -static
CXXSHELL = g++ -o $@ $< -c $(CXXFLAG) 
LDSHELL = g++ -o $@ $< $(LDFLAG) 

%.o: %.cpp
	$(CXXSHELL)
all: TacHacker.exe
TackeHacker.exe: main.o
	$(LDSHELL)

