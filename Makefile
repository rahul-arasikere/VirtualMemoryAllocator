LIBS        = -lm
INC_PATH    = -I.
CC_FLAGS    = -g

all: mymalloc myvmalloc

mymalloc: testsuite.c mymalloc.c
	gcc $(CC_FLAGS) $(INC_PATH) -o mymalloc testsuite.c $(LIBS)

myvmalloc: testsuite.c myvmalloc.c
	gcc -DVHEAP $(CC_FLAGS) $(INC_PATH) -o myvmalloc testsuite.c $(LIBS)

clean :
	rm -f *.o *~ mymalloc myvmalloc $(PROGS)
