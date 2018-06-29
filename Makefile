#
#
BIN_DIR = ./bin

LIB_NAME = repslave
EXE_NAME = resp_slave_server

RM :=rm -f 

LIB_SRCS = $(wildcard src/lib/*.cpp)
LIB_OBJS = $(patsubst %.cpp,%.o,$(LIB_SRCS) )
EXE_SRCS = $(wildcard src/server/*.cpp src/server/jsoncpp/*.cpp)
EXE_OBJS = $(patsubst %.cpp,%.o,$(EXE_SRCS) )
LIB_ARC = lib$(LIB_NAME).a

CXXFLAGS = -g -Wall -fPIC
CPPFLAGS = -I./include/mysql -I./include -I./src/include
LINK_LIBS =-L./lib -lpthread -lcrypto -lssl -lboost_system -lmysqlclient_r -lrdkafka

all: dir $(LIB_ARC) $(EXE_NAME)

dir:
	if [ ! -d $(BIN_DIR) ]; then mkdir $(BIN_DIR) ; fi;


show:
	@echo "EXE_SRCS=$(EXE_SRCS)"
	@echo "LIB_SRCS=$(LIB_SRCS)"
	@echo "EXE_OBJS=$(EXE_OBJS)"
	@echo "LIB_OBJS=$(LIB_OBJS)"

$(LIB_ARC):$(LIB_OBJS)
	ar -cr $(BIN_DIR)/$@  $^

$(EXE_NAME):$(EXE_OBJS)
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $(BIN_DIR)/$@  $^ -L$(BIN_DIR) -l$(LIB_NAME) $(LINK_LIBS)

clean:
	$(RM) $(LIB_OBJS) $(EXE_OBJS)
	$(RM) $(BIN_DIR)/$(EXE_NAME) $(BIN_DIR)/$(LIB_ARC)
#
#
