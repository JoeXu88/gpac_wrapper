GPAC_ROOT=../gpac
MP4file=MP4Writer
SERV_TARGET=./test
SERV_OBJS=main.o MP4Writer.o MP4WriterInter.o
CXX=g++
GCC=gcc
CXXFLAGS=-g -DGPAC_HAVE_CONFIG_H
LIBS=${GPAC_ROOT}/bin/gcc/libgpac_static.a -lz -lpthread

INCLUDES=-I ./ -I ${GPAC_ROOT}/include/ -I ../../

.PHONY:all

all: ${SERV_TARGET}

${SERV_TARGET}: ${SERV_OBJS} 
	${CXX} ${CXXFLAGS} $^ -o $@ ${LIBS} -lstdc++

main.o:main.cpp
	$(CXX) ${CXXFLAGS} ${INCLUDES} -c $< -o $@

MP4Writer.o:${MP4file}.cpp ${MP4file}.h
	$(CXX) ${CXXFLAGS} ${INCLUDES} -c $< -o $@

MP4WriterInter.o:MP4WriterInter.cpp MP4WriterInter.h
	$(CXX) ${CXXFLAGS} ${INCLUDES} -c $< -o $@

.PHONY: clean
clean:
	-rm ${SERV_OBJS} ${SERV_TARGET} -rf
