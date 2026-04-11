ID := $(shell hostname | sed -E 's/dc([0-9]+).*/\1/')

CXX := g++
CXXFLAGS := -IHeaders --std=c++11 -pthread

SRC_DIR := Source

SRC_FILES := Driver.cpp MatrixUtils.cpp NetworkUtils.cpp \
             DatabaseUtils.cpp ProtocolParserUtils.cpp LoggingUtils.cpp \
			 SystemUtils.cpp

SOURCES := $(addprefix $(SRC_DIR)/, $(SRC_FILES))

TARGET := driver.out

build:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)
	./$(TARGET) $(ID)


RECP_ID = 2
REQ_FILENAME = "JEDI.pdf"

T_SRC_FILES := FDPClient.cpp SystemUtils.cpp LoggingUtils.cpp

T_SOURCES := $(addprefix $(SRC_DIR)/, $(T_SRC_FILES))

T_TARGET := FDPClient.out

t1:
	$(CXX) $(CXXFLAGS) $(T_SOURCES) -o $(T_TARGET)
	./$(T_TARGET) $(RECP_ID) $(REQ_FILENAME)
