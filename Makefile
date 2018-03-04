TARGETS=ringmaster player

all: $(TARGETS)
clean:
	rm -f $(TARGETS)

ringmaster: ringmaster.c potato.h
	gcc -g -o $@ $<

player: player.c potato.h
	gcc -g -o $@ $<
