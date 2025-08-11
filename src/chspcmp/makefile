CC = gcc
CXX = g++
AR = ar

# CFLAGS_ENV = # 32bit
CFLAGS_ENV =  -DHSP64 # 64bit
CFLAGS_CMP = -Wno-write-strings -std=c++17 --exec-charset=UTF-8 -DHSPLINUX -DHSPDEBUG -DHSP_COM_UNSUPPORTED $(CFLAGS_ENV)
PKG_CONFIG = pkg-config

OBJS_CCMP = \
	ahtmodel.o \
	ahtobj.o \
	codegen.o \
	codegen_lexer.o \
	codegen_writer.o \
	comutil.o \
	errormsg.o \
	hsc3.o \
	hspcmd.o \
	label.o \
	localinfo.o \
	main.o \
	membuf.o \
	tagstack.o \
	hsmanager.o \
	preprocessor.o \
	preprocessor_lexer.o \
	lexer_util.o \
	logger.o \
	ext/hsp_parser.o \
	ext/ast_serializer.o \
	../hsp3/strnote.o \
	../hsp3/strbuf.o \
	../hsp3/linux/supio_linux.o

TARGETS = chspcmp

all: $(TARGETS)

.SUFFIXES: .cpp

%.o: %.c
	$(CC) $(CFLAGS_CMP) -c $< -o $*.o
%.o: %.cpp
	$(CXX) $(CFLAGS_CMP) -c $< -o $*.o

chspcmp: $(OBJS_CCMP)
	$(CXX) $(CFLAGS_CMP) $(OBJS_CCMP) -o $@

clean:
	rm -f $(OBJS_CCMP)

