.PHONY: all clean
CC = g++ -g
CFLAG = -c -Wall  -fpermissive -g
INC_PATH = ../include/
OBJ_PATH = ./build/obj/
BUILD_PATH = ./build/

SOURCE=$(wildcard *.cpp)
OBJ=$(patsubst %.cpp,$(OBJ_PATH)%.o,$(SOURCE))
HEADER = $(wildcard $(INC_PATH)*.h)
TAR= $(BUILD_PATH)FileSystemClient

all:$(TAR)

$(TAR):$(OBJ)
	$(CC) -I$(INC_PATH) -o $@  $^
$(OBJ):$(OBJ_PATH)%.o:%.cpp $(HEADER)
	$(CC) $(CFLAG) -I$(INC_PATH) -o $@ $<

clean:
	rm -f $(TAR) $(OBJ)