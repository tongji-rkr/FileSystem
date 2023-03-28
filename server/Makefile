.PHONY: all clean

CC = g++ -g
CFLAG = -c -Wall -fpermissive

INC_PATH = ../include/
OBJ_PATH = ./objs/
FS_PATH = ./fs/
DEV_PATH = ./dev/
USER_PATH = ./user/

SOURCE = $(wildcard *.cpp) 
FS_SOURCE = $(wildcard $(FS_PATH)*.cpp) 
DEV_SOURCE = $(wildcard $(DEV_PATH)*.cpp) 
USER_SOURCE = $(wildcard $(USER_PATH)*.cpp)

OBJ = $(patsubst %.cpp,$(OBJ_PATH)%.o,$(SOURCE))
FS_OBJ = $(patsubst $(FS_PATH)%.cpp,$(OBJ_PATH)%.o,$(FS_SOURCE))
DEV_OBJ = $(patsubst $(DEV_PATH)%.cpp,$(OBJ_PATH)%.o,$(DEV_SOURCE))
USER_OBJ = $(patsubst $(USER_PATH)%.cpp,$(OBJ_PATH)%.o,$(USER_SOURCE))

HEADER = $(wildcard $(INC_PATH)*.h)
TAR = server

all:$(TAR)

$(shell if [ ! -e $(OBJ_PATH) ];then mkdir -p $(OBJ_PATH); fi)

$(TAR):$(OBJ) $(FS_OBJ) $(DEV_OBJ) $(USER_OBJ)
	$(CC) -I$(INC_PATH) -o $@  $^  -lpthread

$(OBJ):$(OBJ_PATH)%.o:%.cpp $(HEADER)
	$(CC) $(CFLAG) -I$(INC_PATH) -o $@ $<

$(FS_OBJ):$(OBJ_PATH)%.o:$(FS_PATH)%.cpp $(HEADER)
	$(CC) $(CFLAG) -I$(INC_PATH) -o $@ $<

$(DEV_OBJ):$(OBJ_PATH)%.o:$(DEV_PATH)%.cpp $(HEADER)
	$(CC) $(CFLAG) -I$(INC_PATH) -o $@ $<

$(USER_OBJ):$(OBJ_PATH)%.o:$(USER_PATH)%.cpp $(HEADER)
	$(CC) $(CFLAG) -I$(INC_PATH) -o $@ $<

clean:
	rm -f $(TAR) myDisk.img
	rm -r $(OBJ_PATH)


test:
	@echo $(OBJ)
	@echo $(FS_OBJ)
	@echo $(DEV_OBJ)
	@echo $(USER_OBJ)