#Modificación hecha por: Ing. Javier Flores 12/11/24

SRCS = src/main.cpp \
	src/loadconfig.cpp \
	src/ini.c \
	src/INIReader.cpp \
	src/cls_equipos.cpp \
	src/conversiones.cpp \
	src/cls_cmd.cpp \
	src/cls_defalarmas.cpp \
	src/enviarmail.cpp \
	src/cls_email.cpp \
	src/cls_defeventos.cpp \
	src/cls_sms.cpp \
	src/cls_logger.cpp\
	src/rs232.c

OBJS = ${SRCS:.c=.o}

CC = g++
FLAGS = -g 
TARGET = bin/automonitorv1
INCLUDES = -I/usr/include/modbus	-I/usr/include/cppconn	-I/usr/local/include/modbus		-I/usr/local/include/cppconn	-I/usr/include/mysql	
	
LIBS = -L/usr/lib

.PHONY: depend clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(FLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LIBS) -lmodbus -lmysqlcppconn -lcurl -std=c++11 -pthread -lboost_filesystem -lboost_system

.c.o:
	$(CC) $(FLAGS) $(INCLUDES) -c $< -o $@
	
clean: 
	rm $(TARGET) *.o *~ $(TARGET)

depend: $(SRCS)
	makedepend $(INCLUDES) $<
	
orquesta:
	$(CC) src/orquesta.cpp -o orquesta
	
# NO BORRAR ESTA LINEA
