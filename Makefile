#-----------------------------------------------------------------------
# File: Makefile
#
# $ make 				// same as $make all
# $ make all        // compiles every C_source_file into diferent execs
# $ make <C_source_file_w/o_extension>  // compiles 1 program
#
# Author: Sergio Romero Montiel
#
# Created on October 27th, 2016
#-----------------------------------------------------------------------

#-----------------------FILES-------------------------------------------
# Lista de fuentes
SOURCES := ${wildcard *.c}
INCDIR := ./include
LIBDIR := ./lib
# Ejecutables
EXECS := ${SOURCES:.c=}
LIB := libsimusil.a
#-----------------------TOOLS-------------------------------------------
# Compiladores y Enlazadores (no modificar, usamos los por defecto)
#CC =
# Opciones para el compilador
CFLAGS = -Wall -I$(INCDIR)
# Opciones de enlazado
LDFLAGS = -pthread
LDLIBS = -lrt
# ----------------------RULES-------------------------------------------
# Targets y sufijos
.PHONY: all clean
# regla para obtener todos los ejecutables
all: $(EXECS)
$(EXECS): $(LIBDIR)/$(LIB) $(INCDIR)/simusil.h
clean:
	-rm -fv $(EXECS)
#-----------------------------------------------------------------------
