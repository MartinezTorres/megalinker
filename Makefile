
.PHONY: megalinker all clean test

megalinker: src/megalinker.cc
	@echo "\033[1;32m[$(@)]\033[1;31m\033[0m"
	@$(CXX) -o $@ $< -std=c++17 -O3 -Wall -Werror -Wextra -pedantic 

megalinker.exe: src/megalinker.cc
	@echo "\033[1;32m[$(@)]\033[1;31m\033[0m"
	@i686-w64-mingw32-g++ -static -o $@ $< -std=c++17 -O3 -Wall -Werror -Wextra -pedantic 

all: megalinker megalinker.exe

test: megalinker
	@echo "\033[1;32m[$(@)]\033[1;31m\033[0m"
	make -C test test

clean:
	@echo -n "Cleaning... "
	@rm -f megalinker megalinker.exe
	@echo "Done!"
