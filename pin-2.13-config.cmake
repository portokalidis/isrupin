### Pin stuff
### For pin-2.13-62732-gcc.4.4.7

if (NOT PIN_HOME)
	set (PIN_HOME $ENV{PIN_HOME})
endif ()
if (NOT PIN_HOME)
	set (PIN_HOME /opt/pin)
	message(STATUS "PIN_HOME not declared, defaulting to ${PIN_HOME}")
endif ()

if (NOT EXISTS "${PIN_HOME}/pin")
	message(FATAL_ERROR "`pin' not found in ${PIN_HOME}") 
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
	set(PIN_CXXFLAGS -fPIC -fno-strict-aliasing -unknown-pragmas 
		-fomit-frame-pointer -fno-stack-protector -DTARGET_LINUX)
endif ()

if (CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(arch intel64)
	set(ARCH IA32E)
else ()
	set(arch ia32)
	set(ARCH IA32)
endif ()
set(PIN_CXXFLAGS ${PIN_CXXFLAGS} -DTARGET_${ARCH} -DHOST_${ARCH} 
	-DBIGARRAY_MULTIPLIER=1 -DUSING_XED)

set (PIN_INCLUDES	${PIN_HOME}/source/tools/InstLib 
			${PIN_HOME}/source/include/pin
			${PIN_HOME}/source/include/pin/gen
			${PIN_HOME}/extras/components/include 
			${PIN_HOME}/extras/xed2-${arch}/include)

set(PIN_LPATHS	${PIN_HOME}/${arch}/lib ${PIN_HOME}/${arch}/lib-ext
		${PIN_HOME}/extras/xed2-${arch}/lib)
set(PIN_LIBS xed dl pin dwarf elf)
set(PIN_LDFLAGS -Wl,--hash-style=sysv -Wl,-Bsymbolic
	-Wl,--version-script=${PIN_HOME}/source/include/pin/pintool.ver)


