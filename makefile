# C++11 driver. Generated objects/dependencies never overwrite tracked artifacts.
CXX ?= g++
BUILD_DIR ?= .build
TARGET ?= $(BUILD_DIR)/automonitorv1
INCLUDES ?= -Isrc/include -I/usr/include/modbus
LIBS ?=
CPPFLAGS += $(INCLUDES)
CXXFLAGS ?= -g -O2 -Wall -Wextra -Wformat=2
CXXFLAGS += -std=c++11 -pthread
LDLIBS += -lmodbus -lmysqlcppconn -lcurl -lboost_filesystem -lboost_system -pthread
SRCS = src/main.cpp src/acquisition.cpp src/persistence.cpp src/loadconfig.cpp \
       src/ini.c src/INIReader.cpp src/cls_equipos.cpp src/conversiones.cpp \
       src/cls_cmd.cpp src/cls_defalarmas.cpp src/enviarmail.cpp src/cls_email.cpp \
       src/cls_defeventos.cpp src/cls_sms.cpp src/cls_logger.cpp src/rs232.c src/monitor_server.cpp
OBJECTS = $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(basename $(SRCS))))
.PHONY: all test test-sql orquesta clean
all: $(TARGET) $(BUILD_DIR)/automonitorctl
$(BUILD_DIR)/automonitorctl: tools/automonitorctl.py
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
orquesta: $(BUILD_DIR)/orquesta
$(BUILD_DIR)/orquesta: src/orquesta.cpp src/cls_logger.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -lboost_filesystem -lboost_system -o $@
test: all
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_unit_tests.sh
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' bash tests/run_state_test.sh
	python3 tests/test_monitor_client.py
	python3 tests/test_startup_without_license.py "$(abspath $(TARGET))"
test-sql: $(TARGET)
	CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS) $(LIBS)' DRIVER_BINARY='$(abspath $(TARGET))' bash tests/run_sql_test.sh
clean:
	rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET) $(BUILD_DIR)/automonitorctl
-include $(OBJECTS:.o=.d)
