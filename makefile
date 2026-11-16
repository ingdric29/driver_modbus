.DEFAULT_GOAL := all
# C++11 driver. Generated objects/dependencies never overwrite tracked artifacts.
CXX ?= g++
BUILD_DIR ?= .build
BIN_DIR ?= bin
TARGET ?= $(BIN_DIR)/automonitorv1
INCLUDES ?= -Isrc/include -I/usr/include/modbus
LIBS ?=
BUILD_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)
BUILD_DATE := $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
CPPFLAGS += $(INCLUDES)
$(BUILD_DIR)/src/main.o $(BUILD_DIR)/src/monitor_server.o: CPPFLAGS += -DAUTOMONITOR_COMMIT=\"$(BUILD_COMMIT)\" -DAUTOMONITOR_BUILD_DATE=\"$(BUILD_DATE)\"
# Refresh the identification banner even when only HEAD changed.
.PHONY: FORCE
FORCE:
$(BUILD_DIR)/src/main.o $(BUILD_DIR)/src/monitor_server.o: FORCE
CXXFLAGS ?= -g -O2 -Wall -Wextra -Wformat=2
CXXFLAGS += -std=c++11 -pthread
LDLIBS += -lmodbus -lmysqlcppconn -lcurl -lboost_filesystem -lboost_system -pthread
SRCS = src/main.cpp src/acquisition.cpp src/persistence.cpp src/loadconfig.cpp \
       src/ini.c src/INIReader.cpp src/cls_equipos.cpp src/conversiones.cpp \
       src/cls_cmd.cpp src/cls_defalarmas.cpp src/enviarmail.cpp src/cls_email.cpp \
       src/cls_defeventos.cpp src/cls_sms.cpp src/cls_logger.cpp src/rs232.c src/monitor_server.cpp
OBJECTS = $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(basename $(SRCS))))
.PHONY: all test test-sql clean clean-objects dist
all: $(TARGET) $(BIN_DIR)/automonitorctl $(BIN_DIR)/automonitor-mailcheck $(BIN_DIR)/version.h
$(BIN_DIR)/automonitorctl: tools/automonitorctl.py src/include/version.h
	@mkdir -p $(@D)
	install -m 755 $< $@
$(BIN_DIR)/version.h: src/include/version.h
	@mkdir -p $(@D)
	install -m 644 $< $@
$(BIN_DIR)/automonitor-mailcheck: tools/automonitor-mailcheck.py
	@mkdir -p $(@D)
	install -m 755 $< $@
$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) $(LDLIBS) -o $@
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
test: all
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_unit_tests.sh
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_state_test.sh
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_email_test.sh
	python3 tests/test_mailcheck.py
	python3 tests/test_monitor_client.py
	python3 tests/test_startup_without_license.py "$(abspath $(TARGET))"
test-sql: $(TARGET)
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' DRIVER_BINARY='$(abspath $(TARGET))' bash tests/run_sql_test.sh
clean-objects:
	rm -f $(OBJECTS) $(OBJECTS:.o=.d)
clean: clean-objects
	rm -f $(TARGET) $(BIN_DIR)/automonitorctl $(BIN_DIR)/automonitor-mailcheck $(BIN_DIR)/version.h
dist: all
	bash tools/package-runtime.sh "$(BIN_DIR)" "$(DIST_DIR)"
-include $(OBJECTS:.o=.d)
