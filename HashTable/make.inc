

# Set compiler flags. -I... tells the compiler to look in our directory for
# include files
#
CFLAGS += -I$(MODULES)

OBJS += $(MODULES)/HashTable.o