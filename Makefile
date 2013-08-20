EXECUTABLE_NAME = perl_test_executable
SRCS = perl_test_executable.c

OBJS = $(SRCS,.c=.o)

PERL = /usr/bin/perl
CC += $(shell $(PERL) -MExtUtils::Embed -e ccopts) -g
LDFLAGS += $(shell $(PERL) -MExtUtils::Embed -e ldopts) -o $(EXECUTABLE_NAME)
WARN ?= -Wall -Werror

$(EXECUTABLE_NAME): $(OBJS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS)
