VERSION := 1.0-beta
CXX := g++
OPTIMIZE := -O3 -fno-stack-protector -fno-math-errno -funroll-loops -flto -flto-partition=one
CXXFLAGS := -Wall -Wextra -std=c++17 -DVERSION=\"$(VERSION)\" $(OPTIMIZE) -s -march=native -DNDEBUG

ifeq ($(OS),Windows_NT)
  EXE := .exe
  CXXFLAGS += -static
else
  EXE :=
endif

NAME := Thorn-$(VERSION)$(EXE)

SRCDIR := src
BLDDIR := build
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(BLDDIR)/%.o)

.PHONY: all pgo nopgo clean

all: pgo

pgo:
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(NAME) -fprofile-generate="thorn_pgo"

ifeq ($(OS),Windows_NT)
	$(NAME) bench
else
	./$(NAME) bench
endif

	$(CXX) $(CXXFLAGS) $(SRCS) -o $(NAME) -fprofile-use="thorn_pgo"

ifeq ($(OS),Windows_NT)
	powershell.exe -Command "Remove-Item -Recurse -Force thorn_pgo"
else
	rm -rf thorn_pgo
endif

nopgo: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(BLDDIR)/%.o: $(SRCDIR)/%.cpp | $(BLDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BLDDIR):
ifeq ($(OS),Windows_NT)
	powershell.exe -Command "New-Item -ItemType Directory -Force -Path '$(BLDDIR)'"
else
	mkdir -p $(BLDDIR)
endif

clean:
ifeq ($(OS),Windows_NT)
	powershell.exe -Command "If (Test-Path '$(BLDDIR)') { Remove-Item -Recurse -Force '$(BLDDIR)' }"
	powershell.exe -Command "If (Test-Path '$(NAME)') { Remove-Item -Force '$(NAME)' }"
	powershell.exe -Command "If (Test-Path 'thorn_pgo') { Remove-Item -Recurse -Force thorn_pgo }"
else
	rm -rf $(BLDDIR)
	rm -f $(NAME)
	rm -rf thorn_pgo
endif
