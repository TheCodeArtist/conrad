# # What to call the final executable
TARGET = conrad 

# # Which object files that the executable consists of
OBJS= conrad.o

# # What compiler to use
CC = gcc

# # Compiler flags, -g for debug, -c to make an object file
CFLAGS = -I. 

# # This should point to a directory that holds libcurl, if it isn't
# # in the system's standard lib dir
# # We also set a -L to include the directory where we have the openssl
# # libraries
LDFLAGS =

# # We need -lcurl for the curl stuff
# # We need -lsocket and -lnsl when on Solaris
# # We need -lssl and -lcrypto when using libcurl with SSL support
# # We need -lpthread for the pthread example
LIBS = -lcurl ./fmodapi44407linux/api/lib/libfmodex.so

# # Link the target with all objects and libraries
$(TARGET) : $(OBJS)
	$(CC)  -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET) *.o *~ #*

