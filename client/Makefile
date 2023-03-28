.PHONY: all clean

CC = g++ -g
CFLAG = -c -Wall -fpermissive

INC_PATH = ../include/
OBJ_PATH = ./objs/

SOURCE = $(wildcard *.cpp) 
OBJ = $(patsubst %.cpp,$(OBJ_PATH)%.o,$(SOURCE))

HEADER = $(wildcard $(INC_PATH)*.h)
TAR = client

all:$(TAR)

$(shell if [ ! -e $(OBJ_PATH) ];then mkdir -p $(OBJ_PATH); fi)

$(TAR):$(OBJ) $(FS_OBJ) $(DEV_OBJ) $(USER_OBJ)
	$(CC) -I$(INC_PATH) -o $@  $^  -lpthread

$(OBJ):$(OBJ_PATH)%.o:%.cpp $(HEADER)
	$(CC) $(CFLAG) -I$(INC_PATH) -o $@ $<

clean:
	rm -f $(TAR)
	rm -r $(OBJ_PATH)

test:
	@echo $(OBJ)