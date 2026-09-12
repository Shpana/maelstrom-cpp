TESTS:= 
TESTS+=init-test
TESTS+=echo-test
TESTS+=echo-deferred-test
TESTS+=network-processor-test
TESTS+=dependencies-test

TEST_TARGETS:=$(addprefix build-,$(TESTS))

.PHONY: $(TEST_TARGETS)
$(TEST_TARGETS): build-%:
	cmake \
		--build $(BUILD_DIR) \
		--target $* 

RUN_TEST_TARGETS:=$(addprefix run-,$(TESTS))

.PHONY: $(RUN_TEST_TARGETS)
$(RUN_TEST_TARGETS): run-%: build-%
	ctest --output-on-failure -V --test-dir $(BUILD_DIR) -R $*

.PHONY: build-tests
build-tests: $(TEST_TARGETS)

.PHONY: run-tests
run-tests: build-tests
	ctest --output-on-failure -V --test-dir $(BUILD_DIR) 
