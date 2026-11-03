# On choisit le compilateur
CC = gcc

# On définit les options de compilation
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter -Iinclude -pthread

# On définit les options d'édition de liens
LDFLAGS = -pthread

# On liste les exécutables à produire
TARGETS = bin/main bin/sorting bin/display


# On compile tout par défaut
all: $(TARGETS)

# On crée un exécutable à partir de son fichier objet
bin/%: build/%.o
	$(CC) $(LDFLAGS) $< -o $@

# On compile un .c en .o, et on recompile si common.h change
build/%.o: src/%.c include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

# On garde les .o entre deux compilations
.PRECIOUS: build/%.o

# On supprime les fichiers générés et les FIFO
clean:
	rm -f $(TARGETS) build/*.o *.fifo

# On indique que all et clean ne sont pas des fichiers
.PHONY: all clean