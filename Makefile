BUILD_DIR:=$(CURDIR)/build

.PHONY: warmup
warmup:
	cmake \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DMAELSTROM_CPP_TEST=ON \
		-S $(CURDIR) \
		-B $(BUILD_DIR)

include src/Makefile
include tests/Makefile

format:
	find $(CURDIR)/src/ -iname '*.cpp' | xargs clang-format -i
	find $(CURDIR)/include/ -iname '*.hpp' | xargs clang-format -i
	find $(CURDIR)/tests/ -iname '*.cpp' | xargs clang-format -i

tidy:
	find $(CURDIR)/src/ -iname '*.cpp' | xargs clang-tidy -p=$(BUILD_DIR) -fix-errors
	find $(CURDIR)/include/ -iname '*.hpp' | xargs clang-tidy -p=$(BUILD_DIR) -fix-errors
	find $(CURDIR)/tests/ -iname '*.cpp' | xargs clang-tidy -p=$(BUILD_DIR) -fix-errors
