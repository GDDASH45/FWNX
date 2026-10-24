
# FWNX - Framework for a New eXecution environment

VERSION = 0
PATCHLEVEL = 1
SUBLEVEL = 0
EXTRAVERSION =

NAME = FWNX
KERNEL = kzImage
ELF = fwnx.elf

CC = gcc
LD = ld
OBJCOPY = objcopy

export CC LD OBJCOPY

CFLAGS = -m32
CFLAGS += -ffreestanding
CFLAGS += -fno-stack-protector
CFLAGS += -fno-pic
CFLAGS += -fno-pie
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -fno-unwind-tables
CFLAGS += -Wall -Wextra
CFLAGS += -I$(CURDIR)/include

ASFLAGS = -m32 -ffreestanding

export CFLAGS ASFLAGS

KCONFIG = scripts/kconfig/conf
CONFIG_FILE = .config
LINKER_SCRIPT = kernel/linker.ld
BUILD_SCRIPT = scripts/Makefile.build

CORE_DIRS = kernel

.PHONY: all config oldconfig defconfig prepare build clean mrproper help

all: $(KERNEL)

$(KCONFIG): scripts/kconfig/conf.c scripts/kconfig/Makefile
	@$(MAKE) -C scripts/kconfig

config: $(KCONFIG)
	./$(KCONFIG) Kconfig $(CONFIG_FILE)

oldconfig: config

defconfig: $(KCONFIG)
	rm -f $(CONFIG_FILE)
	./$(KCONFIG) Kconfig $(CONFIG_FILE)

prepare: $(KCONFIG)
	@test -f $(CONFIG_FILE) || ./$(KCONFIG) Kconfig $(CONFIG_FILE)

# Compile selected objects in each kernel directory.
build: prepare
	@set -e; \
	for dir in $(CORE_DIRS); do \
		echo "  BUILD   $$dir"; \
		$(MAKE) --no-print-directory -f $(BUILD_SCRIPT) obj=$$dir __build; \
	done

# Always relink after building objects.
.PHONY: FORCE
FORCE:

$(ELF): FORCE build $(LINKER_SCRIPT) $(BUILD_SCRIPT)
	@$(LD) -m elf_i386 \
		-T $(LINKER_SCRIPT) \
		-o $(ELF) \
		$$(find $(CORE_DIRS) -type f -name '*.o' | sort)
	@echo "  LD      $(ELF)"

# Convert the linked ELF kernel into the final raw binary.
$(KERNEL): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(KERNEL)
	@echo "  IMAGE   $(KERNEL)"

clean:
	@$(MAKE) -C scripts/kconfig clean
	find $(CORE_DIRS) -type f \( -name '*.o' -o -name '*.a' \) -delete
	rm -f $(ELF) $(KERNEL)

mrproper: clean
	rm -f $(CONFIG_FILE)

help:
	@echo "FWNX kernel build targets:"
	@echo "  all       - Compile and link kzImage"
	@echo "  config    - Configure the kernel"
	@echo "  oldconfig - Update kernel configuration"
	@echo "  defconfig - Generate a fresh default configuration"
	@echo "  build     - Build selected kernel objects"
	@echo "  clean     - Remove generated build outputs"
	@echo "  mrproper  - Clean outputs and remove .config"
