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
SNAP7_DIR := third_party/snap7
SNAP7_LIBRARY := $(BIN_DIR)/lib/libsnap7.so
SNAP7_SOURCES := $(wildcard $(SNAP7_DIR)/src/sys/*.cpp $(SNAP7_DIR)/src/core/*.cpp $(SNAP7_DIR)/src/lib/*.cpp)
SNAP7_OBJECTS := $(addprefix $(BUILD_DIR)/,$(SNAP7_SOURCES:.cpp=.o))
SNAP7_WRAPPER := $(SNAP7_DIR)/release/wrappers/c-cpp/snap7.cpp
SRCS = src/main.cpp src/driver_runtime.cpp src/command_worker.cpp src/maintenance_worker.cpp \
       src/serial_worker.cpp src/event_worker.cpp src/alarm_worker.cpp \
       src/catalog_lookup.cpp src/runtime_support.cpp \
       src/acquisition.cpp src/persistence.cpp \
       src/config_file.cpp src/device_catalog.cpp src/action_catalog.cpp \
       src/command_repository.cpp src/sql_repository.cpp src/replication.cpp \
       src/retention.cpp src/catalog_lifecycle.cpp src/repository_support.cpp \
       src/ini.c src/INIReader.cpp src/cls_equipos.cpp \
       src/equipment_tcp.cpp \
       src/equipment_rtu.cpp \
       src/equipment_rs232.cpp src/conversiones.cpp \
       src/modbus_conversions.cpp \
       src/datetime_conversions.cpp \
       src/text_conversions.cpp \
       src/network_utils.cpp \
       src/cls_cmd.cpp src/cls_defalarmas.cpp \
       src/alarm_actions.cpp \
       src/alarm_history.cpp src/enviarmail.cpp src/cls_email.cpp \
       src/cls_defeventos.cpp src/cls_sms.cpp src/cls_logger.cpp src/rs232.c src/monitor_server.cpp \
       $(SNAP7_WRAPPER) src/s7_transport.cpp src/local_config.cpp src/local_acquisition.cpp src/s7_sql.cpp
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
$(TARGET): $(OBJECTS) $(SNAP7_LIBRARY)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) $(LDLIBS) -Wl,-rpath,'$$ORIGIN/lib' -o $@
# Keep upstream sources unchanged and compile them for the current target.
$(BUILD_DIR)/$(SNAP7_DIR)/src/%.o: $(SNAP7_DIR)/src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -std=c++11 -O2 -fPIC -pthread -I$(SNAP7_DIR)/src/sys -I$(SNAP7_DIR)/src/core -I$(SNAP7_DIR)/src/lib -MMD -MP -c $< -o $@
$(SNAP7_LIBRARY): $(SNAP7_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) -shared -Wl,-soname,libsnap7.so $^ -pthread -lrt -o $@
$(BUILD_DIR)/tests/test_snap7_dependency: tests/test_snap7_dependency.cpp $(BUILD_DIR)/$(SNAP7_WRAPPER:.cpp=.o) $(SNAP7_LIBRARY)
	@mkdir -p $(@D)
	$(CXX) -std=c++11 -Wall -Wextra -Werror -I$(SNAP7_DIR)/release/wrappers/c-cpp $^ -Wl,-rpath,$(abspath $(BIN_DIR)/lib) -o $@
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
test: all test-snap7 test-local
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_unit_tests.sh
	SNAP7_LIB_DIR='$(abspath $(BIN_DIR)/lib)' CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_state_test.sh
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_email_test.sh
	python3 tests/test_mailcheck.py
	python3 tests/test_monitor_client.py
	python3 tests/test_startup_without_license.py "$(abspath $(TARGET))"
.PHONY: test-snap7
$(BUILD_DIR)/tests/test_s7_transport: tests/test_s7_transport.cpp tests/s7_fixture.h src/s7_transport.cpp src/include/s7_transport.h src/include/retry_backoff.h $(BUILD_DIR)/$(SNAP7_WRAPPER:.cpp=.o) $(SNAP7_LIBRARY)
	@mkdir -p $(@D)
	$(CXX) -std=c++11 -pthread -Wall -Wextra -Werror -Isrc/include tests/test_s7_transport.cpp src/s7_transport.cpp $(BUILD_DIR)/$(SNAP7_WRAPPER:.cpp=.o) $(SNAP7_LIBRARY) -Wl,-rpath,$(abspath $(BIN_DIR)/lib) -o $@
test-snap7: all $(BUILD_DIR)/tests/test_snap7_dependency $(BUILD_DIR)/tests/test_s7_transport
	python3 tests/test_snap7_sources.py
	timeout 10 $(BUILD_DIR)/tests/test_snap7_dependency
	timeout 15 $(BUILD_DIR)/tests/test_s7_transport
	python3 tests/test_snap7_distribution.py "$(abspath $(BIN_DIR))"
test-sql: $(TARGET)
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' DRIVER_BINARY='$(abspath $(TARGET))' bash tests/run_sql_test.sh
.PHONY: test-local
$(BUILD_DIR)/tests/local_fixture: tests/local_fixture.cpp tests/s7_fixture.h $(BUILD_DIR)/$(SNAP7_WRAPPER:.cpp=.o) $(SNAP7_LIBRARY)
	@mkdir -p $(@D)
	$(CXX) -std=c++11 -pthread $(CPPFLAGS) tests/local_fixture.cpp $(BUILD_DIR)/$(SNAP7_WRAPPER:.cpp=.o) $(SNAP7_LIBRARY) $(LIBS) -lmodbus -Wl,-rpath,$(abspath $(BIN_DIR)/lib) -o $@
test-local: all $(BUILD_DIR)/tests/local_fixture
	python3 tests/test_local_acquisition.py "$(abspath $(TARGET))" "$(abspath $(BUILD_DIR)/tests/local_fixture)"
clean-objects:
	rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(SNAP7_OBJECTS) $(SNAP7_OBJECTS:.o=.d) $(BUILD_DIR)/tests/test_snap7_dependency
	rm -f $(BUILD_DIR)/tests/test_s7_transport
	rm -f $(BUILD_DIR)/tests/local_fixture
clean: clean-objects
	rm -f $(TARGET) $(SNAP7_LIBRARY) $(BIN_DIR)/automonitorctl $(BIN_DIR)/automonitor-mailcheck $(BIN_DIR)/version.h
dist: all
	bash tools/package-runtime.sh "$(BIN_DIR)" "$(DIST_DIR)"
-include $(OBJECTS:.o=.d) $(SNAP7_OBJECTS:.o=.d)

.PHONY: test-s7-sql
test-s7-sql: all $(BUILD_DIR)/tests/local_fixture
	python3 tests/test_s7_sql.py "$(abspath $(TARGET))" "$(abspath $(BUILD_DIR)/tests/local_fixture)"
